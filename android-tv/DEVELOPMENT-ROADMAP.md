# Development roadmap

## Repository and build
- [x] Isolated `android-tv-dreamcast` branch; the PS4 branch remains separate.
- [x] Pin the upstream Flycast Android engine to commit `72b8e67141d85ca5c9259f04563b49f7f92f928b`.
- [x] Add an Android TV build workflow.
- [ ] Obtain a successful build of the expanded TV app and inspect the APK.

## Russian Android TV shell
- [x] Russian TV-first launcher with large readable controls.
- [x] D-pad focus and gamepad input through Android/Flycast.
- [x] Pick one or multiple game files from internal or removable storage where Android's picker exposes them.
- [x] Select a game folder and scan supported-looking disc-image entries.
- [x] Keep a persistent game library and add user-selected cover art.
- [x] Launch selected games through the real Flycast Android activity.
- [ ] Verify folder scanning, URI grants and launch behavior on the RK3528 box.

## Flycast engine, saves and controllers
- [x] Use the real native Flycast engine, not a mock emulator.
- [x] Keep Flycast's original browser/settings available for BIOS, controller mapping, save states and VMU.
- [x] Preserve the normal Android/Flycast controller input path.
- [ ] Test GDI, CHD, CDI and CUE launch paths and multi-file images on the target box.
- [ ] Test VMU, in-game save and save-state restore behavior on the target box.
- [ ] Verify USB providers used by the specific Android TV firmware.

## Graphics and timing
- [x] Add Performance / Balanced / Quality presets mapped to Flycast render resolution and anisotropic filtering.
- [x] Add internal render-height choices from 480p up to 1800p, guarded by a live GLES texture/renderbuffer limit query.
- [x] Use supersampling as the supported antialiasing method; do not expose a fake MSAA control.
- [x] Keep VSync and Flycast frame pacing on; do not force frame skip or a 30-FPS cap.
- [x] Set Dreamcast region and broadcast to Flycast's Default values so game region/timing are not forced to PAL or NTSC.
- [x] Implement an optional GLES final-pass shader selector: Off, CRT, Old TV, Composite, RGB Mask and VHS.
- [ ] Verify the native bridge and shader code compile in CI.
- [ ] Measure all filters and render scales on the actual RK3528 GPU; hide options if a driver/backend does not support them.
- [ ] Check game speed, pacing, heat and memory use during extended play.

## Release gate
- [ ] CI build succeeds and produces an APK.
- [ ] Inspect package manifest, Android entry point, ABI/native libraries and APK signature.
- [ ] Install and launch on Android 13/14.
- [ ] Complete a real-device play test on the user's RK3528 box.
- [ ] Only then call the build the finished release.

## Notes
- The settings UI and native patch are committed to the TV branch, but that is not a substitute for a successful APK build or a test on the RK3528 hardware.
- 1800p is an internal render-height option. It is not an HDMI output-resolution switch, and it is offered only when the queried GLES limits permit the requested 16:9 buffer dimensions.
- The antialiasing method exposed here is supersampling by rendering above the output resolution. A separate MSAA toggle is intentionally not shown.
- Retro filters are optional and Off by default. They operate in Flycast's GLES presentation pass; target-driver performance still needs verification.
- Games and BIOS are not bundled. Use files you own or are otherwise legally entitled to use.
