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
#include "FFWaveSolverShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"

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
            // Create texture descriptions
            FRHITextureCreateDesc Desc =
                FRHITextureCreateDesc::Create2D(
                    TEXT("FF_HeightBuffer_A"),
                    W, H,
                    PF_R32_FLOAT)
                    .SetFlags(ETextureCreateFlags::UAV | ETextureCreateFlags::ShaderResource);

            Resources->HeightBuffers[0] = RHICreateTexture(Desc);

            // Seed an initial wave center spike to trigger immediate propagation
            TArray<float> InitialData;
            InitialData.Init(0.0f, W * H);
            if (W > 2 && H > 2)
            {
                InitialData[(H / 2) * W + (W / 2)] = 15.0f; // Strong initial kick
            }

            FUpdateTextureRegion2D FullRegion(0, 0, 0, 0, W, H);
            RHICmdList.UpdateTexture2D(
                Resources->HeightBuffers[0],
                0,
                FullRegion,
                W * sizeof(float),
                reinterpret_cast<const uint8 *>(InitialData.GetData()));

            Desc.SetDebugName(TEXT("FF_HeightBuffer_B"));
            Resources->HeightBuffers[1] = RHICreateTexture(Desc);

            // Explicitly clear Buffer B to zero
            TArray<float> ZeroData;
            ZeroData.Init(0.0f, W * H);
            RHICmdList.UpdateTexture2D(
                Resources->HeightBuffers[1],
                0,
                FullRegion,
                W * sizeof(float),
                reinterpret_cast<const uint8 *>(ZeroData.GetData()));

            Desc.SetDebugName(TEXT("FF_VelocityBuffer"));
            Resources->VelocityBuffer = RHICreateTexture(Desc);
            RHICmdList.UpdateTexture2D(
                Resources->VelocityBuffer,
                0,
                FullRegion,
                W * sizeof(float),
                reinterpret_cast<const uint8 *>(ZeroData.GetData()));
        });

    FlushRenderingCommands();
    bGPUResourcesInitialized = true;
    Resources->CurrentBufferIndex = 0;
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

void AFFWaterMesh::DispatchGPUSolver(float DeltaTime)
{
    if (!bGPUResourcesInitialized || !GPUResources)
        return;

    FFGPUResources *Resources = GET_GPU_RESOURCES();
    int32 W = GridWidth;
    int32 H = GridHeight;
    int32 ReadIdx = Resources->CurrentBufferIndex;
    int32 WriteIdx = 1 - ReadIdx;
    float dt = FMath::Min(DeltaTime, 0.016f);

    FTextureRenderTargetResource *RTResource = HeightfieldRT ? HeightfieldRT->GameThread_GetRenderTargetResource() : nullptr;
    if (!RTResource)
        return;

    // Cache parameters safely on the Game Thread before clearing frame flags
    int32 DistX = PendingGPUDisturbanceX;
    int32 DistY = PendingGPUDisturbanceY;
    float DistStrength = PendingGPUDisturbanceStrength;
    int32 DistRadius = PendingGPUDisturbanceRadius;

    // Clear single frame trigger immediately
    PendingGPUDisturbanceStrength = 0.0f;

    float LocalWaveSpeed = WaveSpeed;

    ENQUEUE_RENDER_COMMAND(FluidForgeGPUSolverDispatch)(
        [Resources, W, H, ReadIdx, WriteIdx, dt, RTResource, DistX, DistY, DistStrength, DistRadius, LocalWaveSpeed](FRHICommandListImmediate &RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList);

            FRDGTexture *PrevHeightRDG = GraphBuilder.RegisterExternalTexture(
                CreateRenderTarget(Resources->HeightBuffers[ReadIdx].GetReference(), TEXT("FF_PreviousHeight")));

            FRDGTexture *CurrHeightRDG = GraphBuilder.RegisterExternalTexture(
                CreateRenderTarget(Resources->HeightBuffers[WriteIdx].GetReference(), TEXT("FF_CurrentHeight")));

            FRDGTexture *VelocityRDG = GraphBuilder.RegisterExternalTexture(
                CreateRenderTarget(Resources->VelocityBuffer.GetReference(), TEXT("FF_Velocity")));

            // Register the external material texture to blit into
            FRDGTexture *ExportTargetRDG = GraphBuilder.RegisterExternalTexture(
                CreateRenderTarget(RTResource->GetTexture2DRHI(), TEXT("FF_ExportRenderTarget")));

            TShaderMapRef<FFWaveSolverCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
            FFWaveSolverCS::FParameters *Params = GraphBuilder.AllocParameters<FFWaveSolverCS::FParameters>();

            Params->PreviousHeight = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(PrevHeightRDG));
            Params->CurrentHeight = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(CurrHeightRDG));
            Params->Velocity = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(VelocityRDG));

            Params->DeltaTime = dt;
            Params->Gravity = 9.8f;
            Params->Damping = 0.998f;
            Params->WaveSpeed = LocalWaveSpeed;
            Params->GridWidth = W;
            Params->GridHeight = H;

            // Pass disturbance state
            Params->DisturbanceX = DistX;
            Params->DisturbanceY = DistY;
            Params->DisturbanceStrength = DistStrength;
            Params->DisturbanceRadius = DistRadius;

            FIntVector GroupCount(FMath::DivideAndRoundUp(W, 8), FMath::DivideAndRoundUp(H, 8), 1);
            FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("FluidForge_WaveSolver"), ComputeShader, Params, GroupCount);

            // Blit calculation outputs straight onto the Material Render Target on the GPU
            AddCopyTexturePass(GraphBuilder, CurrHeightRDG, ExportTargetRDG, FRHICopyTextureInfo());

            GraphBuilder.Execute();
        });

    Resources->CurrentBufferIndex = WriteIdx;
}

void AFFWaterMesh::BeginPlay()
{
    Super::BeginPlay();

    WaveGrid.Initialize(GridWidth, GridHeight, CellSize);
    WaveGrid.WaveSpeed = WaveSpeed;

    if (bUseGPUSolver)
    {
        InitGPUResources();
    }

    // Allocate render target as RTF_R32f to match internal PF_R32_FLOAT shader buffers exactly
    HeightfieldRT = UKismetRenderingLibrary::CreateRenderTarget2D(
        this,
        GridWidth,
        GridHeight,
        RTF_R32f);

    if (HeightfieldRT)
    {
        UE_LOG(LogFluidForge, Display, TEXT("HeightfieldRT created successfully: %dx%d"), GridWidth, GridHeight);
    }

    BuildMesh();

    if (WaterMaterial)
    {
        UMaterialInstanceDynamic *DynMaterial = UMaterialInstanceDynamic::Create(WaterMaterial, this);
        if (DynMaterial && HeightfieldRT)
        {
            DynMaterial->SetTextureParameterValue(FName("HeightfieldTexture"), HeightfieldRT);
            WaterMesh->SetMaterial(0, DynMaterial);
        }
        else
        {
            WaterMesh->SetMaterial(0, WaterMaterial);
        }
    }

    if (bUseGPUSolver && WaterMesh)
    {
        // Artificially scale the component up by 10x on the Z axis to stretch the bounding box
        WaterMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 10.0f));
    }

    // Trigger an initial center splash
    AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength, 3);
}

void AFFWaterMesh::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    ReleaseGPUResources();
}

void AFFWaterMesh::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    WaveGrid.WaveSpeed = WaveSpeed;

    // Handle the 2-second periodic timer
    TimeSinceDisturbance += DeltaTime;
    bool bTriggerDisturbance = false;
    if (TimeSinceDisturbance >= 2.0f)
    {
        bTriggerDisturbance = true;
        TimeSinceDisturbance = 0.0f;
    }

    // Completely separate the CPU and GPU branches
    if (bUseGPUSolver && bGPUResourcesInitialized)
    {
        if (bTriggerDisturbance)
        {
            PendingGPUDisturbanceX = GridWidth / 2;
            PendingGPUDisturbanceY = GridHeight / 2;
            PendingGPUDisturbanceStrength = DisturbanceStrength;
            PendingGPUDisturbanceRadius = 3;
        }

        // Compute shader runs and copies directly into the HeightfieldRT texture
        DispatchGPUSolver(DeltaTime);

        // Skip CPU texture copy loops and manual vertex updates.
        // In GPU mode, Material WPO handles visual displacement!
    }
    else
    {
        if (bTriggerDisturbance)
        {
            WaveGrid.AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength, 3);
        }

        WaveGrid.Tick(DeltaTime);

        // Fallback CPU-to-RenderTarget render pass
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
                                RhiTexture, 0, Region, DestStride,
                                reinterpret_cast<const uint8 *>(DataCopy->GetData()));
                        }
                        delete DataCopy;
                    });
            }
        }

        UpdateMesh();
    }
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
    if (bUseGPUSolver)
    {
        PendingGPUDisturbanceX = X;
        PendingGPUDisturbanceY = Y;
        PendingGPUDisturbanceStrength = Strength;
        PendingGPUDisturbanceRadius = Radius;
    }
    else
    {
        WaveGrid.AddDisturbance(X, Y, Strength, Radius);
    }

    FVector Origin = GetActorLocation();
    FVector WorldLoc = Origin + FVector(X * CellSize, Y * CellSize, 0.0f);
    OnWaveDisturbance.Broadcast(WorldLoc, Strength, Radius);
}

void AFFWaterMesh::AddDisturbanceAtWorldLocation(FVector WorldLocation, float Strength, int32 Radius)
{
    FVector Origin = GetActorLocation();
    FVector LocalPos = WorldLocation - Origin;

    int32 GridX = FMath::Clamp(FMath::RoundToInt32(LocalPos.X / CellSize), 0, GridWidth - 1);
    int32 GridY = FMath::Clamp(FMath::RoundToInt32(LocalPos.Y / CellSize), 0, GridHeight - 1);

    if (bUseGPUSolver)
    {
        PendingGPUDisturbanceX = GridX;
        PendingGPUDisturbanceY = GridY;
        PendingGPUDisturbanceStrength = Strength;
        PendingGPUDisturbanceRadius = Radius;
    }
    else
    {
        WaveGrid.AddDisturbance(GridX, GridY, Strength, Radius);
    }

    OnWaveDisturbance.Broadcast(WorldLocation, Strength, Radius);
}

float AFFWaterMesh::GetHeightAtWorldLocation(FVector WorldLocation) const
{
    FVector Origin = GetActorLocation();
    FVector LocalPos = WorldLocation - Origin;

    // Continuous grid coordinates
    float GridFX = LocalPos.X / CellSize;
    float GridFY = LocalPos.Y / CellSize;

    // Out of bounds check
    if (GridFX < 0.0f || GridFX >= (GridWidth - 1) || GridFY < 0.0f || GridFY >= (GridHeight - 1))
    {
        return 0.0f;
    }

    // Integer cell indices for the four surrounding cells
    int32 X0 = FMath::FloorToInt32(GridFX);
    int32 Y0 = FMath::FloorToInt32(GridFY);
    int32 X1 = X0 + 1;
    int32 Y1 = Y0 + 1;

    // Fractional part for interpolation weights
    float FracX = GridFX - (float)X0;
    float FracY = GridFY - (float)Y0;

    // Sample the four corner heights safely
    float H00 = WaveGrid.GetHeight(X0, Y0);
    float H10 = WaveGrid.GetHeight(X1, Y0);
    float H01 = WaveGrid.GetHeight(X0, Y1);
    float H11 = WaveGrid.GetHeight(X1, Y1);

    // Bilinear interpolation
    float InterpolatedHeight = FMath::BiLerp(H00, H10, H01, H11, FracX, FracY);
    return InterpolatedHeight;
}