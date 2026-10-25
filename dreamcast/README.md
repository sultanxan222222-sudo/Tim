# Dreamcast Graphics Stack — WHAT'S NEXT + Mini-DLSS

This directory tracks the Dreamcast/Flycast enhancement work alongside the existing Tim catalog application. The existing catalog remains unchanged.

## Goal

A coordinated enhancement stack for Dreamcast games on a PS4 Pro running Flycast:
- WHAT'S NEXT: frame history, motion estimation/reuse experiments, and history invalidation.
- Mini-DLSS: a conservative spatial finishing pass on normal and generated presentation frames; the standalone compute upscaler remains a later stage.
- Flycast integration: the renderer adapter is applied to Resonance's pinned Flycast Vulkan path.
- Game library and saves: reuse Resonance/Flycast game discovery and save-state interfaces rather than duplicating them.

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
- [x] A real PS4-toolchain build completed in GitHub Actions (run 40135686654); the workflow uploaded `Resonance-v01.42-PS4-PKG` (artifact 12577830429). This confirms packaging, not runtime correctness on a console.
- [ ] Install and test on PS4 Pro; measure frame pacing, latency, memory use, and image artifacts.
- [ ] Wire the standalone compute upscaler into a dedicated GPU render target. It is shader-validated but not yet part of the live presentation pipeline.

## Important limitations

A PS4 PKG has been produced by the repository's GitHub Actions workflow, but no on-console validation has been performed. The currently integrated Mini-DLSS stage is a conservative spatial sharpening/edge-aware finishing pass, not NVIDIA DLSS and not neural reconstruction. WHAT'S NEXT currently provides frame-history/lifecycle gating and a motion-estimation reference; it does not yet recognize or reconstruct faces, hair, clothing, or missing 3D geometry. These character-detail goals require a separate, validated reconstruction approach and must not be presented as implemented. The currently integrated Mini-DLSS stage is a conservative spatial sharpening pass, not NVIDIA DLSS and not neural reconstruction. It cannot invent reliable missing geometry or facial detail. Temporal reconstruction/frame generation can introduce ghosting, edge wobble, or added input latency. Existing Resonance game discovery, VMU, and save-state features should be reused rather than reimplemented.
