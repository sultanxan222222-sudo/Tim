# Native PS4 client scaffold

This directory is the starting point for a native homebrew port. It is not yet a runnable PS4 app or a PKG.

## Current native modules

- `src/torrent_task.h` — common download status and progress calculation.
- `src/torrent_queue.c` — bounded queue storage; does not download data.
- `src/torrent_engine.h/.c` — engine adapter contract for start, pause, resume, cancel, polling real status and shutdown.
- `tests/test_torrent_engine.c` — contract tests using a fake backend. These tests validate integration semantics only; they do not access the network or prove that torrent transfers work.

The engine deliberately returns `TIM_ENGINE_NO_BACKEND` if no complete backend is provided. No synthetic progress is generated.

## Why there is no PKG yet

The repository still does not include a verified PS4 SDK/toolchain build, platform input/rendering layer, linker setup, complete package assets, or a torrent library ported and built for Orbis. A valid PKG cannot be produced by zipping the website or renaming an HTML file.

## Implementation order

1. Select and pin a PS4 homebrew toolchain compatible with the target console and HEN environment.
2. Connect the tested `TimAppState` navigation model to PS4 controller input and a real rendering layer.
3. Add a minimal native entry point and build a bootable ELF/PKG.
4. Verify the app launches on the target console.
5. Implement a `TimTorrentBackend` using a torrent library that builds for the selected toolchain.
6. Run an explicitly authorized test torrent; verify piece hashes, pause/resume, disk-full handling, real rates and completion.
7. Connect the UI to the single configured catalog source in `../catalog.json`.

## Safety and data integrity

Only use torrents for content you are authorized to download and redistribute. Never invent magnet links, torrent hashes, peers, or download URLs.
