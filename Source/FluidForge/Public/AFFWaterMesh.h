#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FFWaveGrid.h"
#include "ProceduralMeshComponent.h"
#include "AFFWaterMesh.generated.h"

class UProceduralMeshComponent;
class UTextureRenderTarget2D;

/** Broadcast whenever a disturbance is added to the water surface. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWaveDisturbance, FVector, WorldLocation, float, Strength, int32, Radius);

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

    // ── Blueprint API ───────────────────────────────────────────────

    /** Add a disturbance at grid coordinate (X, Y) with given strength and radius. */
    UFUNCTION(BlueprintCallable, Category = "FluidForge")
    void AddDisturbance(int32 X, int32 Y, float Strength, int32 Radius = 3);

    /** Add a disturbance at a world-space location. Converts world XY to grid XY automatically. */
    UFUNCTION(BlueprintCallable, Category = "FluidForge")
    void AddDisturbanceAtWorldLocation(FVector WorldLocation, float Strength, int32 Radius = 3);

    /** Get the absolute world-space Z height of the water surface at a world location. Uses bilinear interpolation for smooth results. */
    UFUNCTION(BlueprintCallable, Category = "FluidForge")
    float GetHeightAtWorldLocation(FVector WorldLocation) const;

    /** Fires whenever a disturbance is added. Useful for triggering VFX or audio. */
    UPROPERTY(BlueprintAssignable, Category = "FluidForge")
    FOnWaveDisturbance OnWaveDisturbance;

    // ── Configuration ───────────────────────────────────────────────

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

    // The render target that stores the heightfield data for Materials
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FluidForge")
    UTextureRenderTarget2D *HeightfieldRT = nullptr;

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