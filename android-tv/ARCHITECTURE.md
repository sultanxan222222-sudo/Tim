# Android TV architecture and module contracts

The Android TV build is a separate application target. The PS4 renderer, PS4 toolchain, GNM/RADV integration, and PS4 package scripts remain isolated from this target.

## Work streams

1. **Core integration** — pin upstream Flycast, build its Android native libraries, expose only stable engine APIs, and preserve save/VMU behavior.
2. **TV shell and navigation** — Russian 10-foot interface, D-pad focus model, back/menu behavior, launch/resume/library screens.
3. **Game library and storage** — SAF folder access, removable USB storage, supported image formats, metadata and artwork cache.
4. **Settings and capability discovery** — query renderer/backend capabilities at runtime; build the settings UI from capabilities actually reported by the running backend.
5. **Renderer and image quality** — internal scale, supported AA options, optional post-processing filter chain, safe fallback to unmodified output.
6. **Timing and performance** — detect game timing/region, keep emulated time correct, measure frame time and memory, profile on the actual RK3528 device.
7. **Integration and QA** — common interfaces, contract tests, CI builds, APK inspection, and device acceptance tests.

These are parallelizable work packages, not independent applications. They must share contracts and merge into one app.

## Shared contracts

### GameSession
- Stable game identity (disc/game ID when available), source URI, region/timing hint, and current session state.
- A new session or disc change invalidates temporal history and per-session cached resources.
- Saves and VMU data are owned by the emulator core; the shell only asks the core to load/save.

### EngineCapabilities
- Reports the active renderer, supported internal render sizes/scales, maximum texture/render-target dimensions, supported AA modes, and available shader/filter features.
- The settings screen must not invent capabilities. Unsupported items are omitted.
- Capabilities are re-queried when the renderer/backend changes.

### RenderProfile
- Preset: Performance, Balanced, or Quality.
- Internal render scale/resolution and AA mode only when supported.
- Per-game override may replace the global profile.
- Applying a profile must use the core's real settings API and report failure; never persist a value the backend rejected.

### FrameTiming
- Records the game's detected timing/region and actual presentation cadence.
- PAL/NTSC metadata or core timing is authoritative where available.
- A 30-FPS presentation target is never imposed on a game designed for 50/60 FPS. Frame pacing may drop/repeat presentation frames under load, but emulated time must remain correct.
- Unknown or ambiguous timing falls back to the core's native timing, not a guessed universal cap.

### RetroFilter
- One selected filter ID or Off; default is Off.
- Filter choice is independent from render scale, AA, preset, and timing.
- Selecting a filter replaces the previous filter atomically.
- Filter IDs are exposed only when their shaders are implemented and validated on the active renderer.

## Dependency direction

TV shell -> application services -> Flycast adapter -> Flycast core/backend.

The UI must not call native renderer internals directly. The adapter owns settings translation, capability validation, session lifecycle, and errors. Renderer-specific post-processing stays behind a backend interface. Optional effects always have a clean bypass path.

## Device compatibility

The target is Android 13/14 on the user's RK3528 box. Actual ABI, GPU, OpenGL ES/Vulkan extensions, maximum texture size, and controller mappings must be detected at runtime or confirmed on-device. Do not assume Vulkan or a particular Mali driver merely from the SoC model.
