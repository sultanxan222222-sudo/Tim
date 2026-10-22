# PS4-Xplorer Russian adaptation (work in progress)

This folder tracks our Russian-language adaptation work based on the upstream PS4-Xplorer project by Lapy.

## Base selection
- Use the latest available PS4-Xplorer 2.08 package/source, not the older 2.07 listing.
- Upstream release notes report support through firmware 13.52 (published 2026-09-22).
- Upstream source repository: https://github.com/Lapy055/PS4-Xplorer
- Upstream release/package information: https://pkg-zone.com/details/LAPY20009

## Our intended additions
1. Russian UI text throughout the application.
2. Clear Russian labels for file operations, advanced options, warnings, and help.
3. Preserve existing file-management behavior and firmware compatibility.
4. Integrate only after source/build compatibility is verified; do not claim a working PKG until it has been built and tested.

## Important limitation
The public GitHub source tree is not demonstrably the exact source used for the latest 2.08 binary, and the repository does not include a documented, ready-to-edit Russian .xlang file format. A translation patch must be validated against the actual source/project version before packaging. This branch is a work area, not a finished installable PKG.
