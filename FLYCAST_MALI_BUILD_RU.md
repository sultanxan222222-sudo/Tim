# Flycast Mali GLES2 — Android build branch

This isolated branch builds a debug Android APK from a pinned official Flycast revision and applies source-checked GLES2 compatibility changes.

- Source commit: `72b8e67141d85ca5c9259f04563b49f7f92f928b`
- Source patcher: `scripts/apply_patch.py`
- Build workflow: `.github/workflows/flycast-mali-android.yml`
- Successful artifact: `Flycast-Mali-TV-debug-apk`

The patcher refuses any other source revision and requires each expected source fragment to occur exactly once. The workflow then runs unit checks, builds the Android debug APK, and uploads the APK plus a checksum.

The changes provide an optional GLES2 fragment-precision fallback when the driver reports no fragment highp support, safer GL driver-string handling, explicit shader compiler/linker errors, and correct cleanup on failed shader creation/compilation/linking.

This branch is separate from the default branch. It is not yet proof of improved frame rate: the resulting APK still needs a real-device test with the exact Android TV box and Dreamcast title.
