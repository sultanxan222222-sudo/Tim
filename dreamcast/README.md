# Dreamcast Graphics Stack — WHAT'S NEXT + Mini-DLSS

This directory tracks the Dreamcast/Flycast enhancement work alongside the existing Tim catalog application. The existing catalog remains unchanged.

## Goal

A coordinated enhancement stack for Dreamcast games on a PS4 Pro running Flycast:
- WHAT'S NEXT: frame history, motion estimation/reuse experiments, and history invalidation.
- Mini-DLSS: spatial reconstruction/sharpening experiments and later optional temporal reconstruction.
- Flycast integration: the actual renderer/API adapter, to be identified from the selected PS4 Flycast source/build.
- Game library and saves: later integration points; game discovery and save-state handling must use Flycast/platform-supported interfaces.

## Integration rules

1. One shared frame/resource contract; modules must not invent incompatible frame formats.
2. Each stage is tested both alone and in the combined pipeline.
3. History is invalidated on resolution changes, discontinuities, game changes, and incompatible resources.
4. Optional effects can be disabled independently and must have measured frame-time/memory costs.
5. Never claim console validation unless tested on the PS4 itself.

## Current status

- [x] Initial architecture and integration constraints documented.
- [ ] Identify and pin the exact Flycast PS4 source revision and renderer/API.
- [ ] Build a renderer adapter against that source.
- [ ] Integrate the existing Mini-DLSS GLSL prototype where supported.
- [ ] Add automated tests for shared frame contracts and history invalidation.
- [ ] Measure performance on target hardware.
- [ ] Design library discovery and save-state integration after confirming the actual PS4 app/runtime constraints.

## Important limitations

This is not yet a PS4 PKG and is not yet integrated with Flycast. Generic image shaders cannot reliably reconstruct missing geometry or infer every material. Temporal reconstruction requires valid motion/history inputs and can produce artifacts. Game discovery and save-state support depend on the real platform permissions and Flycast build.
