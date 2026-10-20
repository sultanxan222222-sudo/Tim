# Native PS4 client scaffold

This directory is the starting point for a native homebrew port. It is not yet a runnable PS4 app or a PKG.

## Why there is no build command yet
The repository currently contains only a static web prototype and does not include a PS4 SDK/toolchain, native entry point, linker setup, or package metadata. A valid PKG cannot be produced by zipping the website or renaming an HTML file.

## Implementation order
1. Select and pin a PS4 homebrew toolchain compatible with the target console and HEN environment.
2. Add a minimal native app and build a bootable ELF/PKG.
3. Verify the app launches on the target console.
4. Integrate a torrent library that can actually be built for the selected toolchain.
5. Add torrent-file/magnet parsing, peer discovery, piece verification, storage, queue, and live progress.
6. Connect the UI to the single configured catalog source in `../catalog.json`.

## Torrent engine acceptance test
Before integrating the catalog, download a small, explicitly authorized test torrent. Progress and speed must come from the engine's real status API; do not simulate progress. Confirm pause/resume, piece verification, disk-full handling, and completion.

## Safety and data integrity
Only use torrents for content you are authorized to download and redistribute. Never invent magnet links, torrent hashes, peers, or download URLs.
