# Development roadmap

## Repository and baseline
- [x] Isolated `android-tv-dreamcast` branch.
- [x] Pin upstream Flycast as a submodule.
- [x] Add Android TV launcher source and patch application script.
- [x] Add CI workflow to build and upload an installable APK.
- [ ] Confirm CI build and inspect resulting APK.

## TV shell
- [x] Russian TV-first launcher, readable from a distance.
- [x] D-pad focusable controls.
- [x] SAF game picker and recent-game shortcuts.
- [ ] Test navigation and storage providers on the actual RK3528 box.
- [ ] Finish custom TV game-library presentation and artwork.

## Core and storage
- [x] Reuse the pinned Flycast Android application and native engine.
- [ ] Verify direct game launch for GDI/CHD/CDI/CUE and other supported formats.
- [ ] Verify removable USB storage and persistable document URI grants.
- [ ] Verify VMU and save-state behavior.

## Graphics and timing
- [ ] Query actual renderer capabilities at runtime.
- [ ] Implement Performance/Balanced/Quality presets through real Flycast settings.
- [ ] Offer render sizes up to 1800p only within reported limits.
- [ ] Add only actual backend-supported anti-aliasing modes.
- [ ] Preserve native PAL/NTSC timing and emulated speed.

## Optional retro effects
- [ ] One-choice retro filter selector, default Off.
- [ ] Implement and measure each shader on the target GPU.
- [ ] Keep filters independent from render scale, AA and timing.
- [ ] Hide unsupported filters.

## Release validation
- [ ] CI build green; APK inspected for manifest, ABI and dependencies.
- [ ] Install and launch on Android 13/14 device.
- [ ] Extended game-play test on RK3528, including frame pacing, memory and thermal checks.
- [ ] Publish the APK only after build validation; clearly distinguish CI from on-device validation.
