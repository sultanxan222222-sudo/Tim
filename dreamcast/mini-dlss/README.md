# WHAT'S NEXT + Mini-DLSS: integration core

This directory contains a backend-neutral C++ core for the coordinated Dreamcast graphics pipeline.

## Implemented

- Shared frame contract for completed emulator frames.
- Conservative history gate: rejects invalid frames, stale/non-consecutive frames, and session/resource/size/format changes.
- Integrated frame-generation controller and renderer-facing bridge: real-frame IDs are assigned separately from generated presentation repeats, and only committed real source frames enter temporal history.
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

The host-side control layer includes `frame_generation_policy.hpp/.cpp`, `frame_generation_controller.hpp/.cpp`, and `frame_generation_bridge.hpp/.cpp`. The bridge assigns IDs only to actual source frames; the renderer must not call `begin_real_frame()` for repeated vblanks or synthesized frames. It does not render pixels or allocate Vulkan resources. It makes
the safe scheduling decision around a renderer's existing frame-generation path:

- Disabled feature -> bypass and show the real frame.
- Missing history -> show the real frame and seed history.
- Invalid/discontinuous history or changed resources/session -> show the real frame, do not synthesize.
- Cadence other than exactly two display refresh intervals -> show the real frame.
- Only enabled + valid accepted history + a two-refresh interval -> permit an intermediate frame.

The policy, controller, and bridge are covered by dedicated CMake/CTest targets. The bridge's renderer contract is: call `begin_real_frame()` once for each completed real frame, use the returned decision, and call `commit_real_frame()` only after the renderer accepts that real frame; call `reset()` on teardown, game switch, or resource reset.

Important discovery: the actual Resonance PS4 patch already adds `core/rend/vulkan/frame_gen.cpp/.h` and hooks it into `VulkanContext::PresentFrame()` / `DrawFrame()`. It uses GPU coarse/fine motion search and an intermediate Vulkan draw. WHAT'S NEXT should connect its controller to this existing path instead of creating a second renderer. Per Triangle / Per Strip / Per Pixel are alpha-sorting settings, not frame-generation modes. This controller is a tested control layer, not a claim that it is already wired into Resonance.
