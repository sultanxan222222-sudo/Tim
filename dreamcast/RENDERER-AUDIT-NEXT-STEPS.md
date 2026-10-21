# Renderer audit: findings and next integration boundary

## Repository evidence

Audited candidate: `iHaiDeeZ/Resonance`.

- `BUILDING.md` identifies the renderer as Flycast Vulkan on the DolphinPS4 RADV/GNM port, with final presentation through PS4 VideoOut.
- `patches/BASE.txt` pins the upstream Flycast source revision to `0d9853df9`.
- `scripts/rebuild-flycast.sh` rebuilds the patched Flycast tree under `~/resonance-build`; `scripts/package-resonance.sh` packages the app.
- The UI contains an experimental frame-generation setting with three modes: per triangle, per strip, and per pixel. Its own text warns about moving-edge artifacts and extra input latency.
- Existing features include library scanning, VMU/save-state management, and network shares; reuse them rather than introducing a parallel game catalog.

## What the audit has not proven

The GitHub code-search index did not return the actual Vulkan submission or frame-generation implementation for queries such as `vkQueueSubmit`, `vkQueuePresentKHR`, or `FrameGeneration`. Therefore we have not yet established the precise insertion point, ownership/lifetime of the source and destination images, synchronization requirements, or whether useful motion/depth data is exposed. Do not wire a shader based only on README descriptions.

## Required integration sequence

1. Obtain and inspect the complete pinned Flycast source tree and the full PS4 patch locally; locate the existing frame-generation implementation and the Vulkan image/render-pass lifecycle.
2. Add a small renderer adapter that fills `FrameContract` from actual image metadata and advances `resource_generation` on image recreation and `game_session` on game restart.
3. Connect `HistoryGate` first as diagnostics only; log accept/reject reasons and verify resets for resize, game switch, and renderer restart.
4. Compile the spatial shader with the toolchain used by the target build, validate descriptors/formats and synchronization, then add it as an opt-in pass with an unprocessed fallback.
5. Compare disabled/enabled output and GPU cost before enabling anything by default.
6. Investigate temporal reconstruction only after actual motion vectors/depth and frame pacing have been confirmed.

## Status

The standalone C++ history-gate host test passed in the development environment. The GLSL shader has not yet been compiled. No changes have been made to Resonance/Flycast itself, and no PS4 build, console test, or PKG has been produced. A source checkout/build environment with the documented PS4 toolchain is still needed to complete the real integration safely.
