# WHAT'S NEXT + Mini-DLSS: integration core

This directory contains the first small C++ component intended to become part of the coordinated graphics pipeline.

## Implemented

- A backend-neutral frame contract shared by processing stages.
- A history compatibility gate that rejects invalid frames, stale/non-consecutive frames, resolution/format/resource changes, and game-session changes.
- A host-side CMake test target.
- An unintegrated Vulkan compute shader prototype for conservative spatial sharpening (not FSR/DLSS and not temporal reconstruction).

## Build and test on a desktop host

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The test exercises the acceptance path and the invalidation paths. The code has been compiled with GNU C++ 14.2 and the CTest target passed in the development environment.

## What is not implemented yet

This is not yet connected to Resonance/Flycast's Vulkan render pass. The sharpening shader source exists, but no Vulkan descriptors, shader compilation, dispatch, motion vectors, depth images, or GPU submissions are connected. The PS4 build toolchain and console runtime are not present in this host test, so this is not a PS4 build and produces no PKG.

## Integration constraints

- The renderer adapter must increment `resource_generation` whenever the backing render images or format are recreated.
- `game_session` must change on game/session restart.
- History is only eligible for consecutive frames in this initial conservative version.
- The temporal stage must remain disabled until the renderer adapter proves that its inputs are valid.
