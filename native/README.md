# Native PS4 client scaffold

This directory is the starting point for a native homebrew port. It is not yet a runnable PS4 app or a PKG.

## Why there is no build command yet
The repository currently contains a static web prototype and native C modules for queue storage and menu navigation. It still does not include a verified PS4 SDK/toolchain build, platform input/rendering layer, linker setup, or complete package assets. A valid PKG cannot be produced by zipping the website or renaming an HTML file.

## Implementation order
1. Select and pin a PS4 homebrew toolchain compatible with the target console and HEN environment.
2. Connect the tested `TimAppState` navigation model to PS4 controller input and a real rendering layer.
3. Add a minimal native entry point and build a bootable ELF/PKG.
4. Verify the app launches on the target console.
5. Integrate a torrent library that can actually be built for the selected toolchain.
6. Add torrent-file/magnet parsing, peer discovery, piece verification, storage, queue, and live progress.
7. Connect the UI to the single configured catalog source in `../catalog.json`.

## Torrent engine acceptance test
Before integrating the catalog, download a small, explicitly authorized test torrent. Progress and speed must come from the engine's real status API; do not simulate progress. Confirm pause/resume, piece verification, disk-full handling, and completion.

## Safety and data integrity
Only use torrents for content you are authorized to download and redistribute. Never invent magnet links, torrent hashes, peers, or download URLs.
