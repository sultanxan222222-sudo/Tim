# Development roadmap

Each stage must produce a reviewable result before the next stage depends on it.

## Stage 0 — Repository and upstream baseline
- [x] Create isolated `android-tv-dreamcast` branch from the PS4 Dreamcast work.
- [x] Pin Flycast upstream as a submodule.
- [x] Add CI build of the upstream Android baseline APK.
- [ ] Confirm the CI build succeeds and record the exact artifact/commit.

## Stage 1 — App shell
- [ ] Create the branded Android TV application shell and Russian localization.
- [ ] Verify focus navigation with D-pad, remote and connected gamepad.
- [ ] Confirm Android 13/14 launch behavior and Leanback launcher visibility.
- [ ] Preserve the ability to enter the actual Flycast game session.

## Stage 2 — Core adapter and library
- [ ] Wrap the existing Flycast Android APIs behind GameSession/EngineCapabilities contracts.
- [ ] Add library scanning through SAF and removable USB storage.
- [ ] Verify supported disc/image formats and save-state/VMU handling.
- [ ] Add error handling for missing files and inaccessible removable storage.

## Stage 3 — Graphics and timing
- [ ] Query real backend capabilities before showing settings.
- [ ] Implement Performance/Balanced/Quality presets using real engine options.
- [ ] Add resolution choices up to 1800p only when valid render-target limits allow them.
- [ ] Add only those AA modes that the selected backend truly supports.
- [ ] Verify PAL/NTSC and game-native timing; preserve emulated speed.

## Stage 4 — Optional retro filters
- [ ] Add a one-choice filter menu, default Off.
- [ ] Implement and measure each filter independently.
- [ ] Ensure changing filters never silently changes core graphics or timing options.
- [ ] Hide filters that are unsupported by the active backend.

## Stage 5 — Release readiness
- [ ] Run automated tests and produce a signed/reproducible debug or release APK.
- [ ] Inspect APK ABI, manifest, target SDK, native libraries and permissions.
- [ ] Install and test on the actual RK3528 Android TV box.
- [ ] Measure stability, frame pacing, GPU load, memory and thermals over extended play.
- [ ] Publish the installable APK only after the device test passes.

## Acceptance rule

A green host/CI build proves that the checked code compiles. It does not prove that the emulator runs correctly on the user's box. The final status must distinguish CI success from real-device validation.
