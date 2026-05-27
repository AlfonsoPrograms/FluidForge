#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphBuilder.h"

/**
 * FFWaveSolverCS
 *
 * Compute shader that runs one step of the wave equation.
 * One thread per grid cell. 8x8 thread groups.
 */
class FFWaveSolverCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFWaveSolverCS);
    SHADER_USE_PARAMETER_STRUCT(FFWaveSolverCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, PreviousHeight)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, CurrentHeight)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, Velocity)
    SHADER_PARAMETER(float, DeltaTime)
    SHADER_PARAMETER(float, Gravity)
    SHADER_PARAMETER(float, Damping)
    SHADER_PARAMETER(float, WaveSpeed)
    SHADER_PARAMETER(float, CellSize)
    SHADER_PARAMETER(int32, GridWidth)
    SHADER_PARAMETER(int32, GridHeight)
    SHADER_PARAMETER(int32, DisturbanceX)
    SHADER_PARAMETER(int32, DisturbanceY)
    SHADER_PARAMETER(float, DisturbanceStrength)
    SHADER_PARAMETER(int32, DisturbanceRadius)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters &Parameters,
        FShaderCompilerEnvironment &OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 8);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 8);
    }
};