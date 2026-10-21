# WHAT'S NEXT renderer integration

The integration is applied by the exact-anchor script:

```sh
scripts/apply-resonance-whats-next.sh /absolute/path/to/flycast-after-resonance-patch
```

The target tree must first be checked out at the Flycast revision pinned in Resonance's `patches/BASE.txt` and have Resonance's `patches/flycast-ps4.patch` applied. The script copies the host-tested WHAT'S NEXT lifecycle module into `core/rend/vulkan/whats_next`, registers the needed sources with CMake, and hooks its cadence/history decision into the existing `FrameGenerator::Prepare/Draw` flow.

The GitHub Actions workflow `Resonance overlay compatibility` clones that exact Flycast base, applies the current Resonance patch, runs the integration script, and verifies that the new sources and bridge call are present. This proves patch application and source registration—not PS4 compilation or runtime correctness.

## Remaining work

- Compile the integrated Vulkan target with Resonance's PS4 toolchain.
- Verify the actual presentation image format; the adapter currently declares RGBA8 and that assumption must be checked against the renderer.
- The new `mini_dlss_upscale.comp` is an edge-aware spatial-upscaling prototype. It must be compiled to SPIR-V and connected to a Vulkan compute pipeline with explicit image layout barriers, descriptors, and resize-safe resources; it is not yet part of the live renderer.
- Use the same optional post-process path for both source frames and the generated intermediate frame, without inserting CPU framebuffer readback.
- Test latency, visual artifacts, memory use, and frame pacing on the console before packaging a PKG.
