#include "FFWaveSolverShader.h"

// Register the shader with UE's shader system
// Maps this C++ class to the USF entry point
IMPLEMENT_GLOBAL_SHADER(
    FFWaveSolverCS,
    "/FluidForge/Private/FFWaveSolverCS.usf",
    "FFWaveSolverCS",
    SF_Compute);