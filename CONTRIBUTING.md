# Contributing to FluidForge

First of all — thank you. FluidForge exists to serve the UE5 indie dev community, and every contribution, no matter how small, moves that mission forward.

---

## Before You Start

- Check the [open issues](../../issues) before starting work — someone may already be working on it
- For large changes, open an issue first to discuss your approach before writing code
- All contributions go to the `develop` branch — never open a PR against `main` or `beta`

---

## Branch Naming

Follow this convention:

    feature/short-description     → new functionality
    fix/short-description         → bug fixes
    docs/short-description        → documentation only
    refactor/short-description    → restructuring, no behavior change
    shader/short-description      → GPU / HLSL specific work

Examples:

    feature/cpu-swe-solver
    fix/cfl-stability-clamp
    shader/heightfield-compute
    docs/installation-guide

---

## Commit Style

FluidForge uses Conventional Commits. Every commit message must follow this format:

    type: short description in present tense

Valid types:

| Type | When to use |
|------|------------|
| `feat` | New functionality |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `refactor` | Code restructure, no behavior change |
| `chore` | Build system, config, tooling |
| `shader` | HLSL / compute shader work |
| `perf` | Performance improvement |

Examples:

    feat: add finite difference height propagation
    fix: clamp negative height values to prevent NaN cascade
    docs: add shader debugging guide
    chore: update .gitignore for UE5 binaries

---

## Pull Request Process

1. Fork the repository
2. Create a branch off `develop` following the naming convention above
3. Make your changes in small, logical commits
4. Open a PR against `develop` — not `main` or `beta`
5. Fill out the PR description fully — explain what and why, not just what
6. Link any related issues with `Fixes #123` or `Related to #123`

Your PR will not be merged if:

- It is opened against `main` or `beta`
- The description does not explain the change
- It breaks existing functionality
- It touches the render thread unsafely
- It contains debug code, commented out blocks, or magic numbers without explanation

---

## Code Style

FluidForge follows standard UE5 C++ conventions:

- Use UE types — `FString`, `TArray`, `int32` etc, not STL equivalents
- All public classes use the `FF` prefix — `FFWaveGrid`, `FFSimConfig`
- All UObjects use the `UFF` prefix — `UFFSimComponent`
- All Actors use the `AFF` prefix — `AFFWaterVolume`
- Log using `LogFluidForge` — never `LogTemp`
- No raw `new` or `delete` for UObjects — use `NewObject<T>`
- Wrap editor-only code in `#if WITH_EDITOR`
- Wrap debug output in `#if !UE_BUILD_SHIPPING`

---

## Understanding The Codebase

FluidForge is built on the Shallow Water Equations (SWE). Before contributing to the simulation core, make sure you understand:

- What the SWE finite difference update loop does
- What the CFL condition is and why violating it causes instability
- How the heightfield render target connects the sim to the material

If you are unsure, open a discussion and ask. There are no stupid questions here.

---

## PR Review Checklist

Before submitting, review your own PR against this list:

- [ ] Branch is off `develop`, PR targets `develop`
- [ ] Commit messages follow Conventional Commits
- [ ] No debug screen messages or temporary log spam
- [ ] No magic numbers — constants are named and explained
- [ ] Render thread access is safe
- [ ] Blueprint exposed functions are properly marked
- [ ] No STL types — UE equivalents used throughout
- [ ] I can explain every line of this PR in plain English

---

## Reporting Bugs

Open an issue and include:

- UE5 version
- GPU and driver version
- Steps to reproduce
- Expected vs actual behavior
- Output Log excerpt if relevant

---

## License

By contributing to FluidForge you agree that your contributions will be licensed under the MIT License.

---

Built with love for the UE5 indie dev community.
