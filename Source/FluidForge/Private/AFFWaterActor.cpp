#include "AFFWaterActor.h"
#include "FluidForge.h"
#include "DrawDebugHelpers.h"

AFFWaterActor::AFFWaterActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AFFWaterActor::BeginPlay()
{
    Super::BeginPlay();

    WaveGrid.Initialize(GridWidth, GridHeight, CellSize);
    WaveGrid.WaveSpeed = WaveSpeed;

    // Add initial disturbance at center with radius 3
    WaveGrid.AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength, 3);

    UE_LOG(LogFluidForge, Display, TEXT("AFFWaterActor started"));
}

void AFFWaterActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Sync WaveSpeed dynamically
    WaveGrid.WaveSpeed = WaveSpeed;

    // Step the simulation
    WaveGrid.Tick(DeltaTime);

    // Add a periodic disturbance every 2 seconds to keep it alive
    TimeSinceDisturbance += DeltaTime;
    if (TimeSinceDisturbance >= 2.0f)
    {
        WaveGrid.AddDisturbance(GridWidth / 2, GridHeight / 2, DisturbanceStrength, 3);
        TimeSinceDisturbance = 0.0f;
    }

#if !UE_BUILD_SHIPPING
    if (bShowDebugVis)
    {
        DrawDebugHeightfield();
    }
#endif
}

void AFFWaterActor::DrawDebugHeightfield()
{
    FVector Origin = GetActorLocation();

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            float Height = WaveGrid.GetHeight(X, Y);

            // World position of this cell
            FVector Base = Origin + FVector(
                                        X * CellSize,
                                        Y * CellSize,
                                        0.0f);

            FVector Top = Base + FVector(0.0f, 0.0f, Height * 100.0f);

            // Color by height — blue low, white high
            FColor Color = FColor::MakeRedToGreenColorFromScalar(
                FMath::Clamp(Height, 0.0f, 1.0f));

            DrawDebugLine(
                GetWorld(),
                Base,
                Top,
                Color,
                false, // persistent
                -1.0f, // lifetime
                0,     // depth priority
                2.0f   // thickness
            );
        }
    }
}