# PS4 renderer integration findings

## Selected base

Candidate target: [iHaiDeeZ/Resonance](https://github.com/iHaiDeeZ/Resonance), a standalone Flycast-based homebrew app for jailbroken PS4.

Observed in the repository documentation:
- The graphics path is Flycast's Vulkan renderer using a RADV/GNM adaptation, presented via PS4 VideoOut.
- The project pins its Flycast base in `patches/BASE.txt`: upstream `https://github.com/flyinghead/flycast`, base revision `0d9853df9`.
- Its documented build requires WSL Ubuntu plus the DolphinPS4/Crema OpenOrbis toolchain (clang 21 + libc++ 21), and a prepared sysroot/RADV build tree.
- Existing app features include game-folder scanning, SMB/NFS network libraries, VMU management, and save-state slots.
- The README describes an experimental frame-generation feature for 30-FPS games. Its UI text warns about edge wobble and added input latency.
- The settings documentation says full framebuffer emulation is very slow and incompatible with upscaling and widescreen. This is an important compatibility constraint for our pipeline.

## Integration decision

Use Resonance as the candidate PS4/Flycast base rather than trying to retrofit the old generic RetroArch PS4 core first. Keep the Tim catalog code separate from the emulator runtime: Tim is the project's repository/workspace, not the rendering backend.

Do not duplicate Resonance's existing game scanner, VMU saves, or save-state UI before auditing their implementations. Integrate WHAT'S NEXT and Mini-DLSS into the actual render path and reuse existing library/save features where suitable.

## Planned implementation order

1. Pin and record the exact Resonance commit and its Flycast base.
2. Audit the actual Vulkan frame submission/render pass and existing frame-generation implementation.
3. Map the current WHAT'S NEXT frame descriptor to the real Vulkan resources, including image lifetime and reset rules.
4. Implement a minimal opt-in spatial pass first; preserve the original output as a fallback.
5. Add tests for resize/game switch/resource recreation and compare output with the effect disabled/enabled.
6. Only then investigate temporal reconstruction. Do not assume valid motion vectors or depth are available until verified in the renderer.
7. Build using the documented PS4 toolchain and test on console; host tests alone do not establish PS4 compatibility.

## Status and limitations

This is source reconnaissance, not a completed integration. The PS4 toolchain/sysroot and console are not available in this workspace, so a real PS4 build and performance test have not been performed. No PKG is produced by this change.


## Source-level renderer findings (Flycast)

Reviewed upstream Flycast's `core/rend/vulkan/vulkan_renderer.cpp` and `vulkan_renderer.h` at the source revision returned by GitHub search, plus Resonance's README/settings strings.

- Flycast's `BaseVulkanRenderer::RenderFramebuffer()` currently calls `ReadFramebuffer(info, pb, width, height)` into a CPU `PixelBuffer`, then uploads that image into a GPU `Texture`. This is not a good insertion point for real-time motion interpolation: the CPU readback/upload is already a costly path and can introduce a stall.
- The Vulkan presentation path later calls `BaseVulkanRenderer::presentFramebuffer()`, which hands `fbTexture->GetImage()` and its image view to `VulkanContext::PresentFrame()`. The actual renderer-specific `Present()` path is therefore the first place to trace for a GPU-resident integration; we must preserve image layouts, synchronization, swapchain lifetime, and overlay ordering.
- Confirmed from the actual `patches/flycast-ps4.patch`: Resonance already adds `core/rend/vulkan/frame_gen.cpp` and `frame_gen.h`, a GPU-side Vulkan generator with coarse and fine motion grids, plus a fragment interpolation pass. `VulkanContext::PresentFrame()` gates it using the PS4 vblank counter and passes previous/current image views to `FrameGenerator::Prepare()`; `DrawFrame()` invokes `FrameGenerator::Draw()` when an intermediate image is due. The setting is experimental and warns about edge wobble/input latency. Per Triangle / Per Strip / Per Pixel are separate alpha-sorting settings, not frame-generation modes.
- The exact pinned Flycast base used by Resonance is recorded in `patches/BASE.txt` as `0d9853df9`. Upstream source search results can differ from that pinned revision; any final patch must be based on the pinned revision and the actual PS4 patch.

## Updated integration decision

1. Audit Resonance's existing frame-generation implementation first; avoid duplicating its UI, timing detector, and interpolation passes.
2. Place WHAT'S NEXT's `FrameContract`, `HistoryGate`, and new `FrameGenerationPolicy` around that existing path as a conservative control layer: reject stale history after resize, resource recreation, game switch, dropped frames, or unsupported cadence.
3. The implementation has now been located in `patches/flycast-ps4.patch` (`core/rend/vulkan/frame_gen.cpp`, `frame_gen.h`, and `vulkan_context.cpp`). Compare its coarse/fine GPU motion search and confidence fallback with M33's block-motion + half-warp + cross-fade. Do not add a competing generator; improve the existing one behind the same toggle.
4. Prefer the existing GPU-resident frame/image path. Do not insert a CPU full-frame readback pass into the normal game-present loop.
5. Keep the original presentation path as the fail-safe; frame generation remains optional until frame-time and input-latency tests on PS4 Pro.

## Current implementation status

- Added a backend-neutral `FrameGenerationPolicy` API and host test source in Tim. It selects a generated intermediate only when the option is enabled, history is accepted, and the observed cadence is exactly two display refresh intervals; otherwise it directs the caller to present the real frame.
- Added the new source and test target to the CMake project.
- The actual Resonance patch already contains the Vulkan frame-generation implementation; WHAT'S NEXT's shared `FrameContract`, `HistoryGate`, and `FrameGenerationPolicy` are not yet wired into that renderer. The new GitHub Actions host workflow has successfully configured, built, and run all CTest targets (including history, motion-estimator, and frame-generation-policy tests). This is a host build, not a PS4 toolchain build. No PS4 build or PKG has been produced.
