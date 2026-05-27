# FluidForge

> Free, open source GPU-accelerated water simulation plugin for Unreal Engine 5.

![Status](https://img.shields.io/badge/status-early%20development-orange)
![License](https://img.shields.io/badge/license-MIT-green)
![UE5](https://img.shields.io/badge/Unreal%20Engine-5.x-blue)

---

## Vision

FluidForge aims to bring dynamic, physically-based water simulation to every UE5 project — completely free, forever.

No paywalls. No licensing fees. Just water that actually behaves like water.

Inspired by the beauty of emergent complexity — the same simple physics rules that govern a puddle, a river, and an ocean, running in real time inside your game.

---

## Status

🚧 **Early Development — v0.4.0**

FluidForge is in its earliest stage. The foundation is being laid.
Contributors and testers are welcome at every stage of the journey.

---

## Roadmap

- [x] CPU-side SWE wave propagator
- [x] Render target heightfield output
- [x] Basic water material
- [x] Blueprint API
- [ ] Editor viewport live simulation toggle
- [x] GPU compute shader port
- [ ] Physics / buoyancy coupling
- [ ] Niagara VFX integration
- [ ] UE Water Plugin integration
- [ ] Shoreline and boundary handling

---

## Getting Started

### Requirements

- Unreal Engine 5.x
- Visual Studio 2022 or VSCode
- Windows 10/11

### Installation

1. Download or clone this repository
2. Copy the `FluidForge` folder into your project's `Plugins/` directory
3. Right click your `.uproject` and select Generate Visual Studio Project Files
4. Reopen your project — UE will prompt you to rebuild FluidForge
5. Click Yes

### Verify It Works

Open the Output Log in UE5 and look for:

    LogFluidForge: Display: FluidForge v0.1.0 initialized

If you see that line, FluidForge is running.

---

## Contributing

FluidForge welcomes contributors of all skill levels — whether you are a graphics engineer who knows shallow water equations inside out, or someone who just wants to help with documentation.

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a PR.

### Branch Structure

| Branch | Purpose |
|--------|---------|
| `main` | Stable releases only |
| `beta` | Feature complete, in testing |
| `develop` | Active development, all PRs go here |
| `feature/*` | Individual features |
| `fix/*` | Bug fixes |

### Commit Style

FluidForge uses Conventional Commits:

    feat: add CPU-side SWE height propagation
    fix: clamp negative height values to prevent NaN cascade
    docs: update installation guide
    chore: update .gitignore

---

## Architecture Overview

    Input Events (player, physics objects)
                ↓
        SWE Grid Solver (CPU → GPU)
                ↓
        Render Target (heightfield + velocity)
                ↓
      ┌─────────┼──────────────┐
    Water Material  Niagara DI  Physics Query

---

## License

MIT — free forever, for everyone.

Built with love for the UE5 indie dev community.

---

## Author

**AlfonsoPrograms**  
[GitHub](https://github.com/AlfonsoPrograms)
