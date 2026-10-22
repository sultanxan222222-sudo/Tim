# PS4 firmware compatibility target

The user reports their console firmware as **17.02.31** and uses HEN.

## Compatibility policy
- Treat 17.02.31 as the target environment for design and testing.
- Do not claim compatibility merely because the source compiles.
- Before calling a PKG installable, verify the OpenOrbis SDK/toolchain and package metadata against this firmware and test on the actual console/HEN environment where possible.
- Use only mount points and writable directories discovered at runtime; do not assume an internal HDD path is writable.
- USB mounting, HEN-specific access, controller UI, and package installation remain unverified until tested on the user's console.
- If a build requires a firmware-specific SDK setting or ELF/SELF packaging adjustment, document the exact setting and validate the resulting PKG rather than guessing.
