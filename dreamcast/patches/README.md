# WHAT'S NEXT renderer integration

The integration is applied by the exact-anchor script. It inserts a conservative, edge-aware Mini-DLSS spatial finishing operation into Resonance's existing frame-generation fragment shader and creates a separate PS4 presentation shader variant for ordinary game frames. It avoids an extra offscreen target and leaves menu/UI rendering on the original shader path:

```sh
scripts/apply-resonance-whats-next.sh /absolute/path/to/flycast-after-resonance-patch
```

The target tree must first be checked out at the Flycast revision pinned in Resonance's `patches/BASE.txt` and have Resonance's `patches/flycast-ps4.patch` applied. The script copies the host-tested WHAT'S NEXT lifecycle module into `core/rend/vulkan/whats_next`, registers the needed sources with CMake, and hooks its cadence/history decision into the existing `FrameGenerator::Prepare/Draw` flow.

The GitHub Actions workflow `Resonance overlay compatibility` clones that exact Flycast base, applies the current Resonance patch, runs the integration script, and verifies that the new sources and bridge call are present. This proves patch application and source registration—not PS4 compilation or runtime correctness.

## Remaining work

- Compile the integrated Vulkan target with Resonance's PS4 toolchain.
- [x] Confirmed the source-frame format at the pinned Flycast base: `BaseVulkanRenderer::RenderFramebuffer()` creates `TextureType::_8888`, mapped in `Texture::UploadToGPU()` to `vk::Format::eR8G8B8A8Unorm`, matching the RGBA8 frame contract.
- The standalone `mini_dlss_upscale.comp` remains a separate compute upscaler prototype; it is staged in the renderer tree but is not yet wired to an offscreen Vulkan target. The current live path is spatial sharpening, not neural or temporal reconstruction.
- The ordinary-frame and generated-frame shader variants both compile in CI, but the full Vulkan C++ target has not yet been compiled using the PS4 toolchain.
- Verify the actual image format/resource lifecycle and test latency, visual artifacts, memory use, and frame pacing on PS4 Pro before claiming a finished PKG.
