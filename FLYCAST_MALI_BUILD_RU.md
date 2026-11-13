# Flycast Mali GLES2 — Android build branch

This isolated branch contains a reproducible GitHub Actions recipe for producing a debug Android APK of Flycast with the GLES2 compatibility patch.

- Target source commit: `72b8e67141d85ca5c9259f04563b49f7f92f928b`
- Patch: `flycast-mali-gles2-compat.patch`
- Build workflow: `.github/workflows/flycast-mali-android.yml`
- Artifact name after a successful run: `Flycast-Mali-TV-debug-apk`

The default branch is not modified. The workflow runs on pushes to `build/flycast-mali-android`. It checks the patch against the pinned source, compiles the Android debug variant, and uploads the resulting APK with a SHA-256 checksum.

This branch does not claim GPU-level performance validation. The patch handles optional GLES2 fragment `highp`, improves shader compile/link error handling, and safely logs missing GL strings. Performance and visual correctness still require testing on the target Android TV box with the exact Dreamcast game.
