#include "FluidForge.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogFluidForge);

#define LOCTEXT_NAMESPACE "FFluidForgeModule"

void FFluidForgeModule::StartupModule()
{
    UE_LOG(LogFluidForge, Display, TEXT("FluidForge v0.1.0 initialized"));
}

void FFluidForgeModule::ShutdownModule()
{
    UE_LOG(LogFluidForge, Display, TEXT("FluidForge shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFluidForgeModule, FluidForge)