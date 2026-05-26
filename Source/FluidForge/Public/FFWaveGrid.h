#pragma once

#include "CoreMinimal.h"

/**
 * FFWaveGrid
 *
 * Core CPU-side Shallow Water Equation solver.
 * Stores and updates a 2D grid of water height and velocity values.
 * This is the heart of FluidForge v0.1.0.
 */
class FLUIDFORGE_API FFWaveGrid
{
public:
    FFWaveGrid();
    ~FFWaveGrid();

    // Initialize the grid with given dimensions and cell size
    void Initialize(int32 InWidth, int32 InHeight, float InCellSize);

    // Step the simulation forward by DeltaTime seconds
    void Tick(float DeltaTime);

    // Add a disturbance at grid coordinate (X, Y) with given strength and radius
    void AddDisturbance(int32 X, int32 Y, float Strength, int32 Radius = 3);

    // Get water height at grid coordinate (X, Y)
    float GetHeight(int32 X, int32 Y) const;

    // Grid dimensions and config
    int32 Width;
    int32 Height;
    float CellSize;
    float WaveSpeed;

private:
    // Convert 2D coordinates to flat array index
    int32 Index(int32 X, int32 Y) const;

    // Check if coordinates are within grid bounds
    bool InBounds(int32 X, int32 Y) const;

    // Check if a cell is solid (wall)
    bool IsSolid(int32 X, int32 Y) const;

    // Water height per cell
    TArray<float> HeightGrid;

    // Velocity per cell
    TArray<float> Velocity;

    // Gravity constant
    static constexpr float Gravity = 9.8f;

    // Damping factor — controls how fast waves lose energy
    static constexpr float Damping = 0.995f;
};