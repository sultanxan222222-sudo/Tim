# TIM File Manager — native PS4 target

This target is a separate app from the existing TIM download-manager screen. It reuses the repository's `Scene2D` renderer and the portable `file_ops.c` / `file_browser.c` modules.

## Controls

- **L1 / R1** — switch between USB source and HDD destination panes.
- **L2 / R2** — cycle through detected root locations for the active pane.
- **D-pad Up / Down** — select an entry.
- **Cross** — enter a directory; selecting a regular file only selects it.
- **Circle** — go to the parent directory, never above the chosen root.
- **Square** — copy the selected source file or directory into the current destination directory.
- **Triangle** — refresh both directory listings.

The app probes known USB mount candidates and a short list of possible internal writable locations at runtime. It only offers paths that currently exist and pass the relevant access checks; mount layouts and permissions can differ by firmware/HEN environment. If a location is not detected, it is not assumed to exist.

## Build

The `Build TIM PS4 File Manager PKG` GitHub Actions workflow compiles this target with OpenOrbis and uploads a `TIM-PS4-File-Manager-PKG` artifact if all build/package checks succeed.

A successful build is not a console test. The package still needs installation and real USB/HDD copy testing on a PS4 running HEN (target: firmware 13.02). Copying a `.pkg` file only copies the file; it does not install that package.

## Current limitations

- The runtime path candidate list is intentionally conservative and may need adjustment based on mount paths actually exposed on the target console.
- Destination free space is not precomputed; a failed copy reports an error and the file-copy core removes incomplete regular files. A failed recursive directory copy can leave completed children behind.
- The current display uses a built-in ASCII bitmap font; non-ASCII file names may not render correctly.
- No console installation or real-device copy test is claimed until explicitly performed.
