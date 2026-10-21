# Frame generation research for WHAT'S NEXT

## Relevant reference implementation

The public iHaiDeeZ/M33 project documents an experimental 30-to-60 FPS frame-generation path in its PPSSPP PS4 patch, at `GPU/Common/PresentationCommon.cpp`. Its technical notes describe the algorithm:

1. Compare two real frames at the PSP's native resolution.
2. Estimate block motion by searching small shifts for each 8-pixel block, favouring smaller motion and scoring how well samples match.
3. Reconstruct an intermediate frame using half-shifted samples; where the match is poor, use a cross-fade fallback.
4. Keep real 60 FPS games on their normal path. The feature is optional because it can add latency and create artifacts.

The same project documents FSR RCAS sharpening and optional EASU upscaling. This is useful as an implementation reference, not a drop-in library: it is integrated with PPSSPP's Draw abstractions and PSP render sizes, not Flycast's Vulkan renderer.

## Implications for Dreamcast / PS4

- Do not copy PPSSPP-specific classes or framebuffer assumptions into Flycast.
- Reuse the algorithmic idea only after checking its source/license and adapting it to Flycast's image lifecycle, formats, resolution changes, and GPU synchronization.
- Dreamcast scenes can have different geometry, resolution, and motion patterns; block matching may fail on fast camera pans, thin geometry, particles, transparency, and scene cuts.
- The first version should be optional and have a clean bypass. On bad matches, reject temporal blending or use a conservative fallback.
- This does not replace a real temporal upscaler. Without renderer-provided motion vectors and depth, it is image-based motion estimation and may ghost.

## Implementation sequence

1. Inspect the full M33 patch around `GPU/Common/PresentationCommon.cpp` and record the algorithm's resource lifecycle and shader interfaces.
2. Inspect Flycast's pinned source and Resonance's PS4 patch locally, then find the matching frame acquisition/presentation boundary.
3. Implement an independent, backend-neutral motion-estimation specification and host tests for static frames, translation, scene cuts, resolution changes, and low-confidence matches.
4. Implement the GPU pass in Flycast's own rendering abstractions; retain the original frame path as fallback.
5. Measure GPU time and input latency on PS4 Pro before enabling the feature by default.

## Current status

The M33 repository was searched and its technical documentation/patch excerpts were reviewed. The full patch has not been applied to Tim or Flycast, and no PS4 build or console test has been performed. The current Mini-DLSS shader remains a spatial sharpening prototype; it is not the frame-generation algorithm described here.


## M33 source-level findings (reviewed from the patch)

The M33 implementation is more than a conceptual note: its PS4 patch contains the complete experimental pass in `GPU/Common/PresentationCommon.cpp` (see [source patch](https://github.com/iHaiDeeZ/M33/blob/main/patches/ppsspp-ps4.patch)).

- It runs at the presentation/post-processing boundary, before the regular post-shader chain, and can substitute a generated frame for the current source framebuffer.
- It keeps two real-frame buffers, two reduced-size frames for motion estimation, a motion texture, an intermediate output, and output framebuffers matching the game's source-buffer dimensions. Resources are released/reset when dimensions change or the option is disabled.
- Motion estimation is a GPU fragment pass: for each reduced-resolution location it searches shifts from -8 to +8 pixels in both axes, samples a 4x4 grid for each candidate, scores RGB absolute differences, and lightly penalizes larger shifts.
- The interpolation pass samples the previous and current images at half-motion offsets, averages the warped samples, and blends back toward a plain cross-fade when the two samples disagree. This helps with bad matches, but does not solve occlusion/disocclusion or guarantee artifact-free output.
- It detects new game frames using the emulator's flip counter and only generates when the observed interval indicates a 30-FPS frame repeated over two refreshes. The feature is optional and fails closed if its resources/shaders cannot be created.
- It uses PPSSPP's `Draw::Framebuffer`, `Draw::Pipeline`, blit and post-processing interfaces, so those calls must be mapped to Flycast's Vulkan abstractions rather than copied verbatim.

### What we will reuse, and what we will not copy blindly

**Reuse as the reference design:** GPU-side motion search, half-motion warping, error-gated cross-fade fallback, optional toggle, dimension/resource reset, and pass placement at the frame presentation boundary. These directly inform WHAT'S NEXT.

**Adapt for Dreamcast:** the frame cadence detector, source/destination image ownership, texture formats and dimensions, render-thread synchronization, and resource lifetime. Flycast's displayed image and Vulkan/Resonance presentation path differ from PPSSPP's framebuffer lifecycle.

**Do not treat this as modern DLSS:** M33's code is image-based interpolation without engine motion vectors or depth. It can improve perceived smoothness in suitable 30-FPS content, but can add latency, ghosting, edge wobble, and HUD artifacts. It is separate from upscaling; RCAS sharpening is a later image-processing pass.

## Updated implementation order

1. Map M33's three GPU passes and frame-cadence logic to the actual Flycast/Resonance frame boundary.
2. Keep WHAT'S NEXT's frame contract and history gate as the shared control layer; route frame validity, scene-cut/resize resets, and confidence/fallback decisions through it.
3. Implement a Flycast-native GPU prototype with a user-visible off switch and original-frame bypass. Do not run the CPU reference motion estimator over full-resolution frames in the real-time PS4 path.
4. Keep spatial upscaling/sharpening as a separate stage, initially using an independently validated FSR-compatible shader where the renderer/API supports it.
5. Validate with host tests, then the PS4 toolchain, then real-console captures and frame-time/input-latency measurements.

## Verification status

Reviewed source: M33 technical notes and the frame-generation section of its PS4 patch, including motion and interpolation shader logic, resource setup/reset, and frame-cadence gating. This is source review only. No M33 code has been transplanted into Flycast, and no PS4 build or console test has been performed.
