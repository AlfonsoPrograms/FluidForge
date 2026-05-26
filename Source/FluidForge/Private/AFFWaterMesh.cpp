#include "AFFWaterMesh.h"
#include "FluidForge.h"
#include "KismetProceduralMeshLibrary.h"

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
    WaveGrid.AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength);

    BuildMesh();

    if (WaterMaterial)
    {
        WaterMesh->SetMaterial(0, WaterMaterial);
    }

    UE_LOG(LogFluidForge, Display, TEXT("AFFWaterMesh started"));
}

void AFFWaterMesh::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    WaveGrid.Tick(DeltaTime);

    TimeSinceDisturbance += DeltaTime;
    if (TimeSinceDisturbance >= 2.0f)
    {
        WaveGrid.AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength);
        TimeSinceDisturbance = 0.0f;
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