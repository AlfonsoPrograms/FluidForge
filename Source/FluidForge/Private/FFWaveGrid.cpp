#include "FFWaveGrid.h"
#include "FluidForge.h"

FFWaveGrid::FFWaveGrid()
    : Width(0), Height(0), CellSize(1.0f)
{
}

FFWaveGrid::~FFWaveGrid()
{
}

void FFWaveGrid::Initialize(int32 InWidth, int32 InHeight, float InCellSize)
{
    Width = InWidth;
    Height = InHeight;
    CellSize = InCellSize;

    int32 TotalCells = Width * Height;

    HeightGrid.Init(0.0f, TotalCells);
    VelocityX.Init(0.0f, TotalCells);
    VelocityY.Init(0.0f, TotalCells);

    UE_LOG(LogFluidForge, Display,
           TEXT("FFWaveGrid initialized: %dx%d, CellSize=%.2f"),
           Width, Height, CellSize);
}

void FFWaveGrid::Tick(float DeltaTime)
{
    if (Width == 0 || Height == 0)
        return;

    float MaxHeight = 1.0f;
    for (float H : HeightGrid)
    {
        MaxHeight = FMath::Max(MaxHeight, H);
    }

    float WaveSpeed = FMath::Sqrt(Gravity * MaxHeight);
    float SafeDt = FMath::Min(DeltaTime, CellSize / WaveSpeed);

    for (int32 Y = 0; Y < Height; Y++)
    {
        for (int32 X = 0; X < Width; X++)
        {
            if (IsSolid(X, Y))
                continue;

            float H = HeightGrid[Index(X, Y)];
            float HRight = IsSolid(X + 1, Y) ? H : HeightGrid[Index(X + 1, Y)];
            float HLeft = IsSolid(X - 1, Y) ? H : HeightGrid[Index(X - 1, Y)];
            float HUp = IsSolid(X, Y + 1) ? H : HeightGrid[Index(X, Y + 1)];
            float HDown = IsSolid(X, Y - 1) ? H : HeightGrid[Index(X, Y - 1)];

            VelocityX[Index(X, Y)] += (HLeft - HRight) * Gravity * SafeDt;
            VelocityY[Index(X, Y)] += (HDown - HUp) * Gravity * SafeDt;
        }
    }

    for (int32 Y = 0; Y < Height; Y++)
    {
        for (int32 X = 0; X < Width; X++)
        {
            if (IsSolid(X, Y))
                continue;

            HeightGrid[Index(X, Y)] +=
                (VelocityX[Index(X, Y)] + VelocityY[Index(X, Y)]) * SafeDt;

            HeightGrid[Index(X, Y)] = FMath::Max(0.0f, HeightGrid[Index(X, Y)]);

            VelocityX[Index(X, Y)] *= Damping;
            VelocityY[Index(X, Y)] *= Damping;
        }
    }
}

void FFWaveGrid::AddDisturbance(int32 X, int32 Y, float Strength)
{
    if (!InBounds(X, Y))
        return;
    HeightGrid[Index(X, Y)] += Strength;
}

float FFWaveGrid::GetHeight(int32 X, int32 Y) const
{
    if (!InBounds(X, Y))
        return 0.0f;
    return HeightGrid[Index(X, Y)];
}

int32 FFWaveGrid::Index(int32 X, int32 Y) const
{
    return Y * Width + X;
}

bool FFWaveGrid::InBounds(int32 X, int32 Y) const
{
    return X >= 0 && X < Width && Y >= 0 && Y < Height;
}

bool FFWaveGrid::IsSolid(int32 X, int32 Y) const
{
    return !InBounds(X, Y);
}