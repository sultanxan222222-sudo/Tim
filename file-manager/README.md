# TIM File Manager for PS4 (HEN)

This is the native file-manager track alongside the existing web catalog. The website itself is not a PS4 application or PKG.

## Goal
Browse a USB drive that the console has mounted and copy files to a user-selected writable location on the PS4 HDD. Copying a PKG file does **not** install the game.

## Implemented source modules
- `file_ops.c/.h`: lists regular files and directories; copies regular files in chunks, reports progress, refuses to overwrite, and removes a partial destination if a copy fails.
- `file_browser.c/.h`: rooted directory navigation, selection, entering folders, parent navigation that cannot escape the selected root, and copying a selected file into a chosen destination directory.
- `tests/test_file_ops.c`: host tests for copying, no-overwrite behavior, cancellation cleanup, directory listing, rooted navigation, and browser copy flow.
- `CMakeLists.txt` and `.github/workflows/file-manager-core.yml`: compile with strict warnings and run the host-side tests automatically.

## Build and console status
The host-side core checks pass in GitHub Actions. This verifies portable file operations only; it does **not** prove PS4 compatibility.

**There is not yet a finished installable file-manager PKG.** Remaining work: connect these modules to a PS4 controller/rendering UI, discover mounted USB and writable HDD locations at runtime, handle directory-copy UX and free-space errors, link with the pinned OpenOrbis toolchain, package the app, and test launch/copy behavior on the user's PS4 with HEN on firmware 13.02.

USB mount points and writable HDD locations vary. Do not hard-code an assumed `/user/app/PlayStation` destination. Let the user choose from directories that are actually mounted and writable. Keep copying separate from installing games.

## API
See `file_ops.h` and `file_browser.h`. Functions return zero on success and -1 on failure; `errno` contains the error where applicable.
