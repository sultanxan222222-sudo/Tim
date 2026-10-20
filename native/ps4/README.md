# Tim native PS4 build

This target uses the official OpenOrbis toolchain container documented at https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/DOCKER.md.

The GitHub Actions workflow `.github/workflows/ps4-build.yml` builds the native screen and attempts to publish a PKG artifact. A successful CI build proves only that the package was produced by the toolchain; a real PS4/HEN launch test is still required.

The current native screen is an early rendering/build test. It displays the queue panel and explicitly states that the torrent backend is not connected. The engine interface in `../src/torrent_engine.h` is not a torrent library and does not perform network downloads.

## Known missing pieces

- A real torrent implementation compiled and linked for Orbis.
- Controller input, catalog retrieval, game-card data binding and download storage management.
- Console verification of package installation and launch.
