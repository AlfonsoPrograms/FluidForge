#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FFWaveGrid.h"
#include "AFFWaterActor.generated.h"

/**
 * AFFWaterActor
 *
 * Owns and ticks the FFWaveGrid simulation.
 * Visualizes the heightfield using debug lines in the viewport.
 * This is the entry point for placing FluidForge in a level.
 */
UCLASS()
class FLUIDFORGE_API AFFWaterActor : public AActor
{
    GENERATED_BODY()

public:
    AFFWaterActor();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // Grid dimensions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    int32 GridWidth = 32;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    int32 GridHeight = 32;

    // Size of each cell in world units (cm)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    float CellSize = 50.0f;

    // Strength of disturbance added each tick for testing
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge")
    float DisturbanceStrength = 1.0f;

    // Toggle debug visualization
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FluidForge | Debug")
    bool bShowDebugVis = true;

private:
    // The simulation grid
    FFWaveGrid WaveGrid;

    // Draw the heightfield as debug lines
    void DrawDebugHeightfield();

    // Tick counter for periodic disturbance
    float TimeSinceDisturbance = 0.0f;
};