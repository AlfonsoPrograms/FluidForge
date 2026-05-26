#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"

/**
 * FFWaveSolverCS
 *
 * C++ representation of the FFWaveSolverCS compute shader.
 * Declares the parameter layout that maps to the USF file.
 */
class FFWaveSolverCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFWaveSolverCS);
    SHADER_USE_PARAMETER_STRUCT(FFWaveSolverCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER_TEXTURE(Texture2D<float>, PreviousHeight)
    SHADER_PARAMETER_UAV(RWTexture2D<float>, CurrentHeight)
    SHADER_PARAMETER_UAV(RWTexture2D<float>, Velocity)
    SHADER_PARAMETER(float, DeltaTime)
    SHADER_PARAMETER(float, Gravity)
    SHADER_PARAMETER(float, Damping)
    SHADER_PARAMETER(float, WaveSpeed)
    SHADER_PARAMETER(int32, GridWidth)
    SHADER_PARAMETER(int32, GridHeight)
    END_SHADER_PARAMETER_STRUCT()

    // Only compile on platforms that support compute shaders
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    // 8x8 thread group matches [numthreads(8, 8, 1)] in the USF
    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters &Parameters,
        FShaderCompilerEnvironment &OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 8);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 8);
    }
};