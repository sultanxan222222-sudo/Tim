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

The repository now contains a renderer integration script and a repeatable PS4 build driver. CI applies the overlay to the Flycast revision pinned by Resonance, compiles the WHAT'S NEXT C++ lifecycle modules, and compiles both the ordinary-frame Mini-DLSS fragment shader and the generated-frame fragment shader. Host contract tests, sanitizer tests, and shader validation pass.

A full PS4 Vulkan target build still has not been completed here: it depends on the local Crema/DolphinPS4 sysroot, RADV build tree, and OpenOrbis packaging tools. No PKG has been produced from this integration and no on-console image-quality or performance test has been performed. The integrated Mini-DLSS pass is spatial sharpening, not neural DLSS.


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

- Added a backend-neutral `FrameGenerationPolicy`, `FrameGenerationController`, and renderer-facing `FrameGenerationBridge` in Tim. The bridge assigns monotonically increasing IDs to real source frames, rejects invalid/stale history and changed resources/sessions, and exposes explicit reset/commit operations. Generated presentation repeats must not be counted as new source frames.
- Added source and dedicated tests to CMake/CTest. GitHub Actions completed successfully after adding the bridge: [host build and tests](https://github.com/sultanxan222222-sudo/Tim/actions/runs/39655087127).
- The integration script now wires WHAT'S NEXT's `FrameGenerationBridge` into `VulkanContext::PresentFrame()`: it starts a contract for each real source frame, invalidates history when the render extent/session changes, and commits only on the accepted source-frame path. Existing Resonance GPU frame interpolation remains the generator; WHAT'S NEXT does not add a competing frame generator.
- The Mini-DLSS spatial finish is now applied to both the generated intermediate-frame fragment shader and a separate ordinary-frame quad shader variant. The ordinary path uses dedicated PS4 presentation pipelines only while the existing Frame Generation setting is enabled; the normal UI/overlay shader path is left unchanged.
- GitHub Actions validates the pinned source overlay, compiles the bridge C++ modules, compiles both modified fragment shaders, and separately builds/runs the host tests with sanitizers. This is source-level/host validation, not a PS4 toolchain build. No PKG has been produced yet.
