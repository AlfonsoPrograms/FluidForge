#include "FFWaveGrid.h"
#include "FluidForge.h"

FFWaveGrid::FFWaveGrid()
    : Width(0), Height(0), CellSize(1.0f), WaveSpeed(1.0f)
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
    Velocity.Init(0.0f, TotalCells);

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

            Velocity[Index(X, Y)] += Laplacian * Gravity * WaveSpeed * SafeDt;

            HeightGrid[Index(X, Y)] += Velocity[Index(X, Y)] * SafeDt;

            Velocity[Index(X, Y)] *= Damping;

            HeightGrid[Index(X, Y)] = FMath::Clamp(HeightGrid[Index(X, Y)], -5.0f, 5.0f);
        }
    }

    // Reflective boundary conditions: mirror inner cell values to edges
    for (int32 Y = 0; Y < Height; Y++)
    {
        HeightGrid[Index(0, Y)] = HeightGrid[Index(1, Y)];
        HeightGrid[Index(Width - 1, Y)] = HeightGrid[Index(Width - 2, Y)];

        Velocity[Index(0, Y)] = Velocity[Index(1, Y)];
        Velocity[Index(Width - 1, Y)] = Velocity[Index(Width - 2, Y)];
    }
    for (int32 X = 0; X < Width; X++)
    {
        HeightGrid[Index(X, 0)] = HeightGrid[Index(X, 1)];
        HeightGrid[Index(X, Height - 1)] = HeightGrid[Index(X, Height - 2)];

        Velocity[Index(X, 0)] = Velocity[Index(X, 1)];
        Velocity[Index(X, Height - 1)] = Velocity[Index(X, Height - 2)];
    }
}

void FFWaveGrid::AddDisturbance(int32 X, int32 Y, float Strength, int32 Radius)
{
    if (Radius <= 0)
    {
        if (InBounds(X, Y))
        {
            HeightGrid[Index(X, Y)] += Strength;
        }
        return;
    }

    for (int32 Row = Y - Radius; Row <= Y + Radius; Row++)
    {
        for (int32 Col = X - Radius; Col <= X + Radius; Col++)
        {
            if (InBounds(Col, Row))
            {
                float Dist = FMath::Sqrt(FMath::Square(static_cast<float>(Col - X)) + FMath::Square(static_cast<float>(Row - Y)));
                if (Dist <= Radius)
                {
                    // Cosine falloff from 1.0 at center to 0.0 at edge
                    float Falloff = FMath::Cos((Dist / Radius) * (PI * 0.5f));
                    HeightGrid[Index(Col, Row)] += Strength * Falloff;
                }
            }
        }
    }
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