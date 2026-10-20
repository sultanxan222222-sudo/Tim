# Native PS4 build target

This directory is the native PS4 build boundary for Tim. It intentionally does not claim that a PKG is ready until the output is produced by the OpenOrbis toolchain and the app is launched on a real HEN console.

## Toolchain

- Official toolchain: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
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

The C adapter contract in `../src/torrent_engine.h` exposes start, pause, resume, cancel and polling. It is intentionally backend-neutral; it is not itself a torrent implementation. A real backend must provide the library calls and real transfer statistics. The host-side fake backend tests validate the interface and queue synchronization only, not network downloading or PS4 compatibility.
