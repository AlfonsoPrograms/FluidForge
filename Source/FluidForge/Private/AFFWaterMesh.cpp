#include "AFFWaterMesh.h"
#include "FluidForge.h"
#include "KismetProceduralMeshLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Math/Float16.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"
#include "Materials/MaterialInstanceDynamic.h"

// Private GPU resource container
// Private GPU resource container — global to avoid UHT conflicts
struct FFGPUResources
{
    TRefCountPtr<FRHITexture> HeightBuffers[2];
    TRefCountPtr<FRHITexture> VelocityBuffer;
    int32 CurrentBufferIndex = 0;
};

// Helper macro for clean casting
#define GET_GPU_RESOURCES() static_cast<FFGPUResources *>(GPUResources)

AFFWaterMesh::AFFWaterMesh()
{
    PrimaryActorTick.bCanEverTick = true;

    WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
    RootComponent = WaterMesh;
}

void AFFWaterMesh::InitGPUResources()
{
    if (bGPUResourcesInitialized)
        return;

    GPUResources = new FFGPUResources();

    int32 W = GridWidth;
    int32 H = GridHeight;
    FFGPUResources *Resources = GET_GPU_RESOURCES();

    ENQUEUE_RENDER_COMMAND(InitFluidForgeGPUResources)(
        [Resources, W, H](FRHICommandListImmediate &RHICmdList)
        {
            FRHITextureCreateDesc Desc =
                FRHITextureCreateDesc::Create2D(
                    TEXT("FF_HeightBuffer_A"),
                    W, H,
                    PF_R32_FLOAT)
                    .SetFlags(ETextureCreateFlags::UAV |
                              ETextureCreateFlags::ShaderResource);

            Resources->HeightBuffers[0] = RHICreateTexture(Desc);

            Desc.SetDebugName(TEXT("FF_HeightBuffer_B"));
            Resources->HeightBuffers[1] = RHICreateTexture(Desc);

            Desc.SetDebugName(TEXT("FF_VelocityBuffer"));
            Resources->VelocityBuffer = RHICreateTexture(Desc);
        });

    FlushRenderingCommands();

    bGPUResourcesInitialized = true;
    UE_LOG(LogFluidForge, Display, TEXT("FluidForge GPU resources initialized: %dx%d"), GridWidth, GridHeight);
}

void AFFWaterMesh::ReleaseGPUResources()
{
    if (!bGPUResourcesInitialized || !GPUResources)
        return;

    FFGPUResources *Resources = GET_GPU_RESOURCES();

    ENQUEUE_RENDER_COMMAND(ReleaseFluidForgeGPUResources)(
        [Resources](FRHICommandListImmediate &RHICmdList)
        {
            Resources->HeightBuffers[0].SafeRelease();
            Resources->HeightBuffers[1].SafeRelease();
            Resources->VelocityBuffer.SafeRelease();
            delete Resources;
        });

    GPUResources = nullptr;
    bGPUResourcesInitialized = false;
    UE_LOG(LogFluidForge, Display, TEXT("FluidForge GPU resources released"));
}

void AFFWaterMesh::BeginPlay()
{
    Super::BeginPlay();

    WaveGrid.Initialize(GridWidth, GridHeight, CellSize);
    WaveGrid.WaveSpeed = WaveSpeed;
    WaveGrid.AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength, 3);

    // Initialize GPU resources if GPU solver is enabled
    if (bUseGPUSolver)
    {
        InitGPUResources();
    }

    // Create render target for material heightfield sampling
    HeightfieldRT = UKismetRenderingLibrary::CreateRenderTarget2D(
        this,
        GridWidth,
        GridHeight,
        RTF_R16f);

    if (HeightfieldRT)
    {
        UE_LOG(LogFluidForge, Display, TEXT("HeightfieldRT created: %dx%d"), GridWidth, GridHeight);
    }
    else
    {
        UE_LOG(LogFluidForge, Warning, TEXT("Failed to create HeightfieldRT!"));
    }

    BuildMesh();

    if (WaterMaterial)
    {
        UMaterialInstanceDynamic *DynMaterial = UMaterialInstanceDynamic::Create(WaterMaterial, this);
        if (DynMaterial && HeightfieldRT)
        {
            DynMaterial->SetTextureParameterValue(FName("HeightfieldTexture"), HeightfieldRT);
            WaterMesh->SetMaterial(0, DynMaterial);
            UE_LOG(LogFluidForge, Display, TEXT("Bound HeightfieldRT to material"));
        }
        else
        {
            WaterMesh->SetMaterial(0, WaterMaterial);
        }
    }

    // Initialize readback cache
    GPUReadbackCache.Init(0.0f, GridWidth * GridHeight);

    UE_LOG(LogFluidForge, Display, TEXT("AFFWaterMesh started — GPU solver: %s"),
           bUseGPUSolver ? TEXT("ON") : TEXT("OFF"));
}

void AFFWaterMesh::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    ReleaseGPUResources();
}

void AFFWaterMesh::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Sync WaveSpeed dynamically
    WaveGrid.WaveSpeed = WaveSpeed;

    WaveGrid.Tick(DeltaTime);

    // Dynamic disturbance logic
    TimeSinceDisturbance += DeltaTime;
    if (TimeSinceDisturbance >= 2.0f)
    {
        WaveGrid.AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength, 3);
        TimeSinceDisturbance = 0.0f;
    }

    // Write heightfield data to the Render Target
    if (HeightfieldRT && GridWidth > 0 && GridHeight > 0)
    {
        FTextureRenderTargetResource *RTResource = HeightfieldRT->GameThread_GetRenderTargetResource();
        if (RTResource)
        {
            TArray<FFloat16> HeightData;
            HeightData.SetNumUninitialized(GridWidth * GridHeight);

            for (int32 Y = 0; Y < GridHeight; Y++)
            {
                for (int32 X = 0; X < GridWidth; X++)
                {
                    float HeightVal = WaveGrid.GetHeight(X, Y);
                    HeightData[Y * GridWidth + X] = FFloat16(HeightVal);
                }
            }

            // Copy data to a render thread safe array and enqueue the update
            TArray<FFloat16> *DataCopy = new TArray<FFloat16>(MoveTemp(HeightData));
            int32 Width = GridWidth;
            int32 Height = GridHeight;

            ENQUEUE_RENDER_COMMAND(UpdateFluidForgeRTCommand)(
                [RTResource, DataCopy, Width, Height](FRHICommandListImmediate &RHICmdList)
                {
                    auto RhiTexture = RTResource->GetTexture2DRHI();
                    if (RhiTexture)
                    {
                        FUpdateTextureRegion2D Region(0, 0, 0, 0, Width, Height);
                        uint32 DestStride = Width * sizeof(FFloat16);
                        RHICmdList.UpdateTexture2D(
                            RhiTexture,
                            0,
                            Region,
                            DestStride,
                            reinterpret_cast<const uint8 *>(DataCopy->GetData()));
                    }
                    delete DataCopy;
                });
        }
    }

    UpdateMesh();
}

void AFFWaterMesh::BuildMesh()
{
    Vertices.Empty();
    Triangles.Empty();
    UVs.Empty();

    for (int32 Y = 0; Y <= GridHeight; Y++)
    {
        for (int32 X = 0; X <= GridWidth; X++)
        {
            Vertices.Add(FVector(
                X * CellSize,
                Y * CellSize,
                0.0f));

            UVs.Add(FVector2D(
                (float)X / GridWidth,
                (float)Y / GridHeight));
        }
    }

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            int32 BottomLeft = Y * (GridWidth + 1) + X;
            int32 BottomRight = BottomLeft + 1;
            int32 TopLeft = BottomLeft + (GridWidth + 1);
            int32 TopRight = TopLeft + 1;

            Triangles.Add(BottomLeft);
            Triangles.Add(TopLeft);
            Triangles.Add(BottomRight);

            Triangles.Add(BottomRight);
            Triangles.Add(TopLeft);
            Triangles.Add(TopRight);
        }
    }

    WaterMesh->CreateMeshSection(
        0,
        Vertices,
        Triangles,
        Normals,
        UVs,
        VertexColors,
        Tangents,
        true);
}

void AFFWaterMesh::UpdateMesh()
{
    for (int32 Y = 0; Y <= GridHeight; Y++)
    {
        for (int32 X = 0; X <= GridWidth; X++)
        {
            int32 VertIndex = Y * (GridWidth + 1) + X;

            int32 SampleX = FMath::Clamp(X, 0, GridWidth - 1);
            int32 SampleY = FMath::Clamp(Y, 0, GridHeight - 1);

            float Height = WaveGrid.GetHeight(SampleX, SampleY);

            Vertices[VertIndex].Z = Height * HeightScale;
        }
    }

    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
        Vertices,
        Triangles,
        UVs,
        Normals,
        Tangents);

    WaterMesh->UpdateMeshSection(
        0,
        Vertices,
        Normals,
        UVs,
        VertexColors,
        Tangents);
}

// ── Blueprint API ───────────────────────────────────────────────────

void AFFWaterMesh::AddDisturbance(int32 X, int32 Y, float Strength, int32 Radius)
{
    WaveGrid.AddDisturbance(X, Y, Strength, Radius);

    // Calculate the world location of this grid cell and broadcast the event
    FVector Origin = GetActorLocation();
    FVector WorldLoc = Origin + FVector(X * CellSize, Y * CellSize, 0.0f);
    OnWaveDisturbance.Broadcast(WorldLoc, Strength, Radius);
}

void AFFWaterMesh::AddDisturbanceAtWorldLocation(FVector WorldLocation, float Strength, int32 Radius)
{
    FVector Origin = GetActorLocation();
    FVector LocalPos = WorldLocation - Origin;

    // Convert world-space offset to grid coordinates
    int32 GridX = FMath::RoundToInt32(LocalPos.X / CellSize);
    int32 GridY = FMath::RoundToInt32(LocalPos.Y / CellSize);

    // Clamp to valid grid range
    GridX = FMath::Clamp(GridX, 0, GridWidth - 1);
    GridY = FMath::Clamp(GridY, 0, GridHeight - 1);

    WaveGrid.AddDisturbance(GridX, GridY, Strength, Radius);

    OnWaveDisturbance.Broadcast(WorldLocation, Strength, Radius);
}

float AFFWaterMesh::GetHeightAtWorldLocation(FVector WorldLocation) const
{
    FVector Origin = GetActorLocation();
    FVector LocalPos = WorldLocation - Origin;

    // Continuous grid coordinates (floating-point)
    float GridFX = LocalPos.X / CellSize;
    float GridFY = LocalPos.Y / CellSize;

    // Integer cell indices for the four surrounding cells
    int32 X0 = FMath::FloorToInt32(GridFX);
    int32 Y0 = FMath::FloorToInt32(GridFY);
    int32 X1 = X0 + 1;
    int32 Y1 = Y0 + 1;

    // Clamp to valid grid range
    X0 = FMath::Clamp(X0, 0, GridWidth - 1);
    Y0 = FMath::Clamp(Y0, 0, GridHeight - 1);
    X1 = FMath::Clamp(X1, 0, GridWidth - 1);
    Y1 = FMath::Clamp(Y1, 0, GridHeight - 1);

    // Fractional part for interpolation weights
    float FracX = GridFX - FMath::FloorToFloat(GridFX);
    float FracY = GridFY - FMath::FloorToFloat(GridFY);

    // Sample the four corner heights
    float H00 = WaveGrid.GetHeight(X0, Y0);
    float H10 = WaveGrid.GetHeight(X1, Y0);
    float H01 = WaveGrid.GetHeight(X0, Y1);
    float H11 = WaveGrid.GetHeight(X1, Y1);

    // Bilinear interpolation
    float HBottom = FMath::Lerp(H00, H10, FracX);
    float HTop = FMath::Lerp(H01, H11, FracX);
    float InterpolatedHeight = FMath::Lerp(HBottom, HTop, FracY);

    // Return absolute world Z: actor Z + scaled height
    return Origin.Z + InterpolatedHeight * HeightScale;
}