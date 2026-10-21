# Applying WHAT'S NEXT to Resonance

The overlay patch in `dreamcast/patches/resonance-whats-next.patch` connects the tested WHAT'S NEXT history/cadence bridge to Resonance's existing Vulkan frame generator. It does **not** add a competing interpolation shader: `FrameGenerator::Prepare/Draw` remain the GPU motion-estimation and interpolation backend.

## Apply order

1. Prepare Resonance's pinned Flycast checkout as described in its BUILDING.md.
2. Apply Resonance's own `patches/flycast-ps4.patch` to that Flycast checkout first.
3. From this Tim repository, run:

   ```sh
   scripts/apply-resonance-whats-next.sh /absolute/path/to/resonance-build/src/flycast
   ```

   The script performs `git apply --check` before changing files.

## Integration scope and remaining validation

The patch adds the backend-neutral history/cadence sources to the Vulkan target, gates the existing interpolation pass through WHAT'S NEXT, and invalidates history on extent changes and GUI transitions. The current renderer contract maps the Flycast presentation image to RGBA8; confirm that assumption against the actual framebuffer format before relying on the gate for every renderer configuration.

This is an overlay patch committed in Tim; it has not been applied to or committed into the third-party Resonance repository. A successful host CI run for Tim does not prove that the PS4 Vulkan target compiles. Build with Resonance's documented PS4 toolchain and test on console before treating this as a usable PS4 package. Mini-DLSS's spatial shader is not yet wired into this patch.
