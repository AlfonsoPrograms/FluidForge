#include "AFFWaterMesh.h"
#include "FluidForge.h"
#include "KismetProceduralMeshLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Math/Float16.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "RHI.h"
#include "Materials/MaterialInstanceDynamic.h"

AFFWaterMesh::AFFWaterMesh()
{
    PrimaryActorTick.bCanEverTick = true;

    WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
    RootComponent = WaterMesh;
}

void AFFWaterMesh::BeginPlay()
{
    Super::BeginPlay();

    WaveGrid.Initialize(GridWidth, GridHeight, CellSize);
    WaveGrid.WaveSpeed = WaveSpeed;
    WaveGrid.AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength, 3);

    // Create the render target dynamically matching the grid dimensions in single-channel 16-bit float format
    HeightfieldRT = UKismetRenderingLibrary::CreateRenderTarget2D(
        this,
        GridWidth,
        GridHeight,
        RTF_R16f);

    if (HeightfieldRT)
    {
        UE_LOG(LogFluidForge, Display, TEXT("HeightfieldRT successfully created: %dx%d"), GridWidth, GridHeight);
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
            UE_LOG(LogFluidForge, Display, TEXT("Created dynamic material instance and bound HeightfieldRT to 'HeightfieldTexture'"));
        }
        else
        {
            WaterMesh->SetMaterial(0, WaterMaterial);
        }
    }

    UE_LOG(LogFluidForge, Display, TEXT("AFFWaterMesh started"));
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