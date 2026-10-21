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
