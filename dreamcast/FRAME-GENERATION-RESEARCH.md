# Frame generation research for WHAT'S NEXT

## Relevant reference implementations

### M33 / PPSSPP PS4

The public iHaiDeeZ/M33 project documents an experimental 30-to-60 FPS frame-generation path in its PPSSPP PS4 patch, at `GPU/Common/PresentationCommon.cpp`. Its technical notes describe the algorithm:

1. Compare two real frames at the PSP's native resolution.
2. Estimate block motion by searching small shifts for each 8-pixel block, favouring smaller motion and scoring how well samples match.
3. Reconstruct an intermediate frame using half-shifted samples; where the match is poor, use a cross-fade fallback.
4. Keep real 60 FPS games on their normal path. The feature is optional because it can add latency and create artifacts.

The same project documents FSR RCAS sharpening and optional EASU upscaling. This is useful as an implementation reference, not a drop-in library: it is integrated with PPSSPP's Draw abstractions and PSP render sizes, not Flycast's Vulkan renderer.

Source: [M33 repository](https://github.com/iHaiDeeZ/M33).

### VCFG / GoldHEN PS4 optical-flow frame generation

A second, more directly relevant reference is [Melnikoff07/vcfg](https://github.com/Melnikoff07/vcfg), an MIT-licensed GoldHEN plugin that reports hardware-tested 30→60 generation on several PS4 game titles. Its source and architecture notes describe:

- GPU-side luma pyramids and hierarchical motion search (coarse-to-fine), followed by motion filtering, interpolation phase and warp into a 1280×720 generated surface.
- Optional RCAS sharpening and anti-aliasing after interpolation.
- A conservative per-frame admission gate: unsupported buffer formats, unrecognised flip command layout, unknown compute-register state, missing previous frame, or unexpected resource state means the native frame is shown instead.
- A GPU memory/synchronization lifecycle that explicitly waits until generated surfaces are no longer scanned out before reuse. The project warns not to free or unregister memory before GPU, command-buffer and scanout completion are established.
- Reported limitations: the new engine is verified only on specific title IDs; unknown games remain experimental. GPU command-buffer formats and compute-register state differ by game, and generated frames can increase latency or fail to run.

The VCFG algorithm is valuable as a reference for efficient hierarchical optical flow and for fail-closed resource handling. Its shader sources include AMD GCN-specific LLVM IR and its plugin hooks the game's PS4 flip submission. Those pieces are **not directly portable** to Flycast's Vulkan renderer. For WHAT'S NEXT, port the algorithmic stages to Flycast's actual Vulkan abstractions and shaders; do not copy VCFG's PS4 flip-hook/runtime or raw GCN shader binaries into the emulator.

License note: VCFG declares MIT. Any source code copied or adapted must retain the required copyright and license notice. M33/PPSSPP has a different license (GPL-2.0); keep provenance clear and do not mix code without checking the resulting distribution obligations.

Sources: [VCFG README](https://github.com/Melnikoff07/vcfg), [architecture](https://github.com/Melnikoff07/vcfg/blob/main/docs/ARCHITECTURE.md), [compatibility evidence](https://github.com/Melnikoff07/vcfg/blob/main/docs/COMPATIBILITY.md).

## Implications for Dreamcast / PS4

- Do not copy PPSSPP-specific classes or framebuffer assumptions into Flycast.
- Do not reuse VCFG's GoldHEN hooks, DirectMemory allocation plan, PS4 flip-command parsing, or AMD GCN shader binaries inside Flycast.
- Reuse algorithmic ideas: GPU-side coarse-to-fine motion search, half-motion warping, confidence-aware blending, optional enablement, and conservative fallback to the real frame.
- Dreamcast scenes can have different geometry, resolution, and motion patterns; optical flow may fail on fast camera pans, thin geometry, particles, transparency, HUD overlays, and scene cuts.
- The first version should be optional and have a clean bypass. On bad matches, reject temporal blending or use a conservative fallback.
- This does not replace a real temporal upscaler. Without renderer-provided motion vectors and depth, it is image-based motion estimation and may ghost.

## Implementation sequence

1. Reuse Resonance's existing `FrameGenerator` instead of implementing a second renderer.
2. Integrate `FrameGenerationController` at `VulkanContext::PresentFrame()`: real frames only, explicit reset on resize/menu/game-session changes, and commit only real source frames to history.
3. Compare the current coarse/fine block search against M33 and VCFG; improve confidence/disocclusion fallback only when measurements justify the cost.
4. Keep spatial upscaling/sharpening as a separate stage; do not label a sharpening-only pass as DLSS or temporal upscaling.
5. Validate with host tests, then the PS4 toolchain, then real-console captures and frame-time/input-latency measurements.

## Current status

The M33 frame-generation logic and VCFG architecture/compatibility notes have been reviewed. The Mini-DLSS host CMake build and CTest suite pass in GitHub Actions. WHAT'S NEXT has a backend-neutral frame contract, history gate, CPU reference block-motion estimator, and cadence policy. These components are not yet wired into Flycast's renderer. No PS4 build or console test has been performed.

The current Mini-DLSS sharpening shader remains a spatial prototype, not a temporal upscaler or a production frame-generation pass.
