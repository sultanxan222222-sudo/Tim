# WHAT'S NEXT + Mini-DLSS: integration core

This directory contains a backend-neutral C++ core for the coordinated Dreamcast graphics pipeline.

## Implemented

- Shared frame contract for completed emulator frames.
- Conservative history gate: rejects invalid frames, stale/non-consecutive frames, and session/resource/size/format changes.
- CPU reference block-motion estimator for tightly packed 8-bit luminance frames. It performs bounded sum-of-absolute-differences matching, rejects flat/low-texture blocks, and attaches a confidence score.
- Host-side CMake tests for history validation and motion estimation.
- Unintegrated Vulkan compute shader prototype for spatial edge-aware sharpening.

## Build and test on a desktop host

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Motion estimator contract

`estimate_block_motion(previous, current, config)` returns a coarse grid of displacement vectors. A vector marked invalid must not be used to synthesize an in-between frame; the caller should fall back to the real frame or a conservative blend. Flat blocks and blocks with a poor match are invalid. The current implementation is a CPU reference intended to validate behavior and serve as a baseline for a future GPU implementation.

## Not yet implemented / limitations

- No integration into Resonance/Flycast's Vulkan render pass.
- No GPU dispatch, Vulkan descriptors, motion-vector image, depth image, or presentation scheduling is connected.
- The motion estimator is not yet optimized for real-time PS4 use; it is a CPU baseline, not an acceptable final full-resolution path.
- The estimator returns displacement/confidence only; it does not synthesize an intermediate frame.
- The sharpening shader is not FSR/DLSS or temporal reconstruction.
- No PS4 build, PKG, or console test has been performed.

## Integration constraints

- The renderer adapter must increment `resource_generation` whenever backing render images or format are recreated.
- `game_session` must change on game/session restart.
- History is eligible only for consecutive frames in this conservative first version.
- Temporal interpolation must remain disabled until frame input, motion confidence, and presentation timing are validated on the target renderer.


## M33-inspired frame-generation scheduling

The host-side control layer now includes `whats_next/frame_generation_policy.hpp` and
`src/frame_generation_policy.cpp`. It does not render pixels or allocate Vulkan resources. It makes
the safe scheduling decision around a renderer's existing frame-generation path:

- Disabled feature -> bypass and show the real frame.
- Missing history -> show the real frame and seed history.
- Invalid/discontinuous history or changed resources/session -> show the real frame, do not synthesize.
- Cadence other than exactly two display refresh intervals -> show the real frame.
- Only enabled + valid accepted history + a two-refresh interval -> permit an intermediate frame.

The decision is covered by `tests/frame_generation_policy_test.cpp` and added to CMake/CTest.

Important discovery: Resonance already has its own experimental frame-generation feature and exposes Per Triangle / Per Strip / Per Pixel modes. Before implementing a second interpolation renderer, WHAT'S NEXT should wrap/audit that existing implementation and compare it with M33's GPU block-motion method. This policy is a control layer, not a claim that M33's shader has been integrated.
