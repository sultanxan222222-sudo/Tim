# TIM File Manager for PS4 (HEN)

This directory begins the **native file-manager** track alongside the existing web catalog. The repository's current `index.html` is a web prototype and is not a PS4 PKG.

## Goal
Browse accessible USB storage and copy files to a user-selected destination on the PS4 HDD, with progress, free-space checks, and error reporting. The app must never claim that copying a PKG installs a game.

## First native milestone
- POSIX file-operation core in `file_ops.c` / `file_ops.h`: list directory entries and copy a regular file with chunked I/O.
- The PS4 UI, controller navigation, USB mount discovery, recursive directory copy, and PKG packaging remain to be integrated and tested with the OpenOrbis toolchain on a real HEN console.
- USB mount points and writable HDD locations vary by environment; do not hard-code a guessed `/user/app/PlayStation` destination. The UI should let the user choose only paths that are actually mounted and writable.
- Never overwrite an existing file without confirmation. Preserve the source file; this is copy, not move.

## Build status
**Source milestone only — no installable PKG yet.** A native PS4 build cannot be considered complete until the OpenOrbis build succeeds, the ELF is packaged correctly, and the PKG is tested on the user's PS4/HEN.

## File operation API
See `file_ops.h`. Functions return zero on success and a negative value on failure; `errno` gives the underlying POSIX error where applicable.
