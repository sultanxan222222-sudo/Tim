# Dreamcast Graphics Stack — WHAT'S NEXT + Mini-DLSS

This directory tracks the Dreamcast/Flycast enhancement work alongside the existing Tim catalog application. The existing catalog remains unchanged.

## Goal

A coordinated enhancement stack for Dreamcast games on a PS4 Pro running Flycast:
- WHAT'S NEXT: frame history, motion estimation/reuse experiments, and history invalidation.
- Mini-DLSS: spatial reconstruction/sharpening experiments and later optional temporal reconstruction.
- Flycast integration: the actual renderer/API adapter, to be identified from the selected PS4 Flycast source/build.
- Game library and saves: later integration points; game discovery and save-state handling must use Flycast/platform-supported interfaces.

## Integration rules

1. One shared frame/resource contract; modules must not invent incompatible frame formats.
2. Each stage is tested both alone and in the combined pipeline.
3. History is invalidated on resolution changes, discontinuities, game changes, and incompatible resources.
4. Optional effects can be disabled independently and must have measured frame-time/memory costs.
5. Never claim console validation unless tested on the PS4 itself.

## Current status

- [x] Selected Resonance as the PS4/Flycast base and use the Flycast revision pinned by Resonance in `patches/BASE.txt`.
- [x] Implemented and host-tested the shared WHAT'S NEXT frame contract, history gate, cadence policy, controller, and renderer bridge.
- [x] Added an integration script that applies the bridge to Resonance's existing Vulkan frame-generation path and places a conservative spatial sharpening operation in its interpolation fragment shader.
- [x] GitHub Actions successfully applies the overlay to the pinned Flycast source, compiles the bridge C++ files, compiles the modified fragment shader, and runs normal plus ASan/UBSan host tests.
- [x] Added `scripts/build-ps4-emulator.sh` to run the host tests, prepare the pinned source/dependencies, apply the integration, build with the local PS4 toolchain, package the app, and verify the expected `.pkg`.
- [ ] Complete a real PS4-toolchain build and produce an installable PKG.
- [ ] Install and test on PS4 Pro; measure frame pacing, latency, memory use, and image artifacts.
- [ ] Wire the standalone compute upscaler into a dedicated GPU render target. It is validated as a shader but is not yet part of the live presentation pipeline.

## Important limitations

No PS4 PKG has yet been built from this repository's integration, and no on-console validation has been performed. The currently integrated Mini-DLSS stage is a conservative spatial sharpening pass, not NVIDIA DLSS and not neural reconstruction. It cannot invent reliable missing geometry or facial detail. Temporal reconstruction/frame generation can introduce ghosting, edge wobble, or added input latency. Existing Resonance game discovery, VMU, and save-state features should be reused rather than reimplemented.
