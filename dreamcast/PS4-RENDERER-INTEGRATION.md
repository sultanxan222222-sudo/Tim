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
