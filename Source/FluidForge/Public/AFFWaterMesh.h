#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FFWaveGrid.h"
#include "ProceduralMeshComponent.h"
#include "AFFWaterMesh.generated.h"

/**
 * AFFWaterMesh
 *
 * Owns the FFWaveGrid simulation and renders it
 * as a deforming procedural mesh. This replaces
 * the debug line visualization with real geometry.
 */
UCLASS()
class FLUIDFORGE_API AFFWaterMesh : public AActor
{
    GENERATED_BODY()

public:
    AFFWaterMesh();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // material
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    UMaterialInterface *WaterMaterial = nullptr;
    // Grid dimensions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    int32 GridWidth = 32;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    int32 GridHeight = 32;

    // Size of each cell in world units (cm)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    float CellSize = 50.0f;

    // Speed of wave propagation
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    float WaveSpeed = 1.0f;

    // Height scale for visual exaggeration
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    float HeightScale = 50.0f;

    // Disturbance strength
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    float DisturbanceStrength = 0.5f;

private:
    // The simulation grid
    FFWaveGrid WaveGrid;

    // Procedural mesh component
    UPROPERTY()
    UProceduralMeshComponent *WaterMesh;

    // Build the initial flat mesh
    void BuildMesh();

    // Update mesh vertices from current heightfield
    void UpdateMesh();

    // Cached mesh data
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    float TimeSinceDisturbance = 0.0f;
};