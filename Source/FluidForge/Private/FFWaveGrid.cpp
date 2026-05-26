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

    float SafeDt = FMath::Min(DeltaTime, 0.016f);

    for (int32 Y = 1; Y < Height - 1; Y++)
    {
        for (int32 X = 1; X < Width - 1; X++)
        {
            float H = HeightGrid[Index(X, Y)];
            float HRight = HeightGrid[Index(X + 1, Y)];
            float HLeft = HeightGrid[Index(X - 1, Y)];
            float HUp = HeightGrid[Index(X, Y + 1)];
            float HDown = HeightGrid[Index(X, Y - 1)];

            float Laplacian = HRight + HLeft + HUp + HDown - 4.0f * H;

            VelocityX[Index(X, Y)] += Laplacian * Gravity * SafeDt;

            HeightGrid[Index(X, Y)] += VelocityX[Index(X, Y)] * SafeDt;

            VelocityX[Index(X, Y)] *= Damping;

            HeightGrid[Index(X, Y)] = FMath::Clamp(HeightGrid[Index(X, Y)], -5.0f, 5.0f);
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