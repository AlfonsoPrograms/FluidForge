#include "FluidForge.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogFluidForge);

#define LOCTEXT_NAMESPACE "FFluidForgeModule"

void FFluidForgeModule::StartupModule()
{
    // Register shader directory
    FString ShaderDir = FPaths::Combine(
        IPluginManager::Get().FindPlugin(TEXT("FluidForge"))->GetBaseDir(),
        TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/FluidForge"), ShaderDir);

    UE_LOG(LogFluidForge, Display, TEXT("FluidForge v0.1.0 initialized"));
    UE_LOG(LogFluidForge, Display, TEXT("Shader directory registered: %s"), *ShaderDir);
}

void FFluidForgeModule::ShutdownModule()
{
    UE_LOG(LogFluidForge, Display, TEXT("FluidForge shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFluidForgeModule, FluidForge)