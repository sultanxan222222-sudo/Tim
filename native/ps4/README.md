# Native PS4 build target

This directory is the native PS4 build boundary for Tim. It intentionally does not claim that a PKG is ready until the output is produced by the OpenOrbis toolchain and the app is launched on a real HEN console.

## Toolchain

- Official toolchain: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
- GitHub Actions helper: https://github.com/OpenOrbis/toolchain-action
- Build examples: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/tree/master/samples/hello_world

The package build requires the toolchain's compiler/linker and packaging utilities, plus valid `sce_sys` metadata/assets and runtime modules. Do not replace missing assets with empty files and do not rename an ELF/HTML file to `.pkg`.

## Required integration before a user-testable build

1. A PS4 entry point compiled and linked for Orbis.
2. A working controller input implementation.
3. A rendering implementation that displays the Tim menu on the television.
4. Valid `param.sfo`, icon and required modules.
5. A CI artifact containing the actual package, not just source files.
6. A console launch test confirming the package opens under the user's HEN setup.

## Download engine

Torrent progress must be supplied by a real engine status callback. The current queue is only state storage; it is not a downloader. Start integration with an explicitly authorized test torrent and verify piece hashes, pause/resume, storage errors and completion before connecting any catalog entries.
