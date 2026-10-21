#!/usr/bin/env bash
# End-to-end build entry point for Resonance + WHAT'S NEXT / Mini-DLSS.
# Run inside the prepared WSL/Linux PS4 build environment.
# Example:
#   RESONANCE_DIR="$HOME/resonance" PS4_BUILD_ROOT="$HOME/resonance-build" \
#     bash scripts/build-ps4-emulator.sh
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RESONANCE_DIR="${RESONANCE_DIR:-$HOME/resonance}"
PS4_BUILD_ROOT="${PS4_BUILD_ROOT:-$HOME/resonance-build}"
FLYCAST_SRC="$PS4_BUILD_ROOT/src/flycast-whats-next"
CRM="${CRM:-$HOME/cemups4-build}"
OPENORBIS="${OPENORBIS:-/opt/pacbrew/ps4/openorbis}"
LOG_DIR="$PS4_BUILD_ROOT/logs"
OUT_DIR="$PS4_BUILD_ROOT/out"
VERSION="${RESONANCE_VERSION:-01.00}"
EXPECTED_BUILD_ROOT="$HOME/resonance-build"

fail() { printf '\nBUILD STOPPED: %s\n' "$*" >&2; exit 1; }
step() { printf '\n========== %s ==========\n' "$*"; }

[[ "$VERSION" =~ ^[0-9]{2}\.[0-9]{2}$ ]] || fail "RESONANCE_VERSION must use XX.YY format (for example 01.00)."
[[ "$(readlink -m "$PS4_BUILD_ROOT")" == "$(readlink -m "$EXPECTED_BUILD_ROOT")" ]] || fail "Resonance log/build helpers are pinned to $EXPECTED_BUILD_ROOT; set PS4_BUILD_ROOT to that path."
for cmd in git python3 cmake ninja clang-21 clang++-21 ld.lld-21 llvm-ar-21 llvm-ranlib-21; do
    command -v "$cmd" >/dev/null 2>&1 || fail "Required command is missing: $cmd"
done
[[ -d "$RESONANCE_DIR/.git" ]] || fail "Resonance checkout not found at $RESONANCE_DIR. Set RESONANCE_DIR to the existing Resonance Git checkout."
[[ -f "$RESONANCE_DIR/patches/flycast-ps4.patch" ]] || fail "Resonance PS4 patch is missing."
[[ -f "$RESONANCE_DIR/patches/BASE.txt" ]] || fail "Resonance patches/BASE.txt is missing."

# Check every required upstream build script before touching the build tree.
for script in seed-build-tree.sh build-netfs.sh rebuild-flycast.sh package-resonance.sh; do
    [[ -f "$RESONANCE_DIR/scripts/$script" ]] || fail "Required Resonance build script is missing: scripts/$script"
done
[[ -f "$ROOT/scripts/apply-resonance-whats-next.sh" ]] || fail "WHAT'S NEXT overlay script is missing."
[[ -x "$ROOT/scripts/test-whats-next.sh" || -f "$ROOT/scripts/test-whats-next.sh" ]] || fail "WHAT'S NEXT host test script is missing."

# Fail early if the machine-specific PS4 toolchain/sysroot used by Resonance is absent.
# Host-only CMake success is not enough to produce an installable PS4 package.
for path in "$CRM/sysroot" "$CRM/host-compat" "$CRM/build/mesa"; do
    [[ -d "$path" ]] || fail "Missing PS4 build prerequisite: $path. Prepare the Crema/DolphinPS4 build tree first."
done
for path in "$OPENORBIS/ps4vars.sh" "$OPENORBIS/bin/linux/PkgTool.Core" "$OPENORBIS/bin/linux/create-gp4" "$OPENORBIS/samples/piglet/sce_module"; do
    [[ -e "$path" ]] || fail "Missing OpenOrbis toolchain component: $path."
done
[[ -f "$RESONANCE_DIR/sce_sys/icon0.png" && -f "$RESONANCE_DIR/sce_sys/pic1.png" ]] || fail "Resonance package icons are missing."
[[ -d "$RESONANCE_DIR/launcher-assets/launcher" ]] || fail "Resonance launcher assets are missing."

mkdir -p "$PS4_BUILD_ROOT" "$LOG_DIR" "$OUT_DIR"
export PS4_BUILD_ROOT PSB="$PS4_BUILD_ROOT" CRM OPENORBIS FLYCAST_SRC
export RESONANCE_VERSION="$VERSION"

step "1/7 — Host contract tests"
bash "$ROOT/scripts/test-whats-next.sh" 2>&1 | tee "$LOG_DIR/whats-next-host-tests.log"

step "2/7 — Prepare pinned Flycast and network dependency sources"
# Upstream clone-flycast.sh contains a machine-specific /mnt/e symlink. Clone here
# instead, so this driver respects RESONANCE_DIR and never rewrites that path.
if [[ ! -d "$HOME/resonance" && ! -e "$HOME/resonance" ]]; then
    ln -s "$RESONANCE_DIR" "$HOME/resonance"
fi
if [[ -L "$HOME/resonance" && "$(readlink -f "$HOME/resonance")" != "$(readlink -f "$RESONANCE_DIR")" ]]; then
    fail "$HOME/resonance points to a different checkout. Set up the expected symlink manually; this script will not replace it."
fi
BASE="$(awk '$1 == "base:" {print $2; exit}' "$RESONANCE_DIR/patches/BASE.txt")"
[[ "$BASE" =~ ^[0-9a-f]{9,40}$ ]] || fail "Could not read a valid pinned Flycast revision from patches/BASE.txt: $BASE"
if [[ ! -d "$FLYCAST_SRC/.git" ]]; then
    [[ ! -e "$FLYCAST_SRC" ]] || fail "$FLYCAST_SRC exists but is not a Git checkout; move it aside before cloning."
    git clone --recursive https://github.com/flyinghead/flycast.git "$FLYCAST_SRC" 2>&1 | tee "$LOG_DIR/clone-flycast.log"
else
    REMOTE="$(git -C "$FLYCAST_SRC" remote get-url origin 2>/dev/null || true)"
    [[ "$REMOTE" == "https://github.com/flyinghead/flycast.git" || "$REMOTE" == "git@github.com:flyinghead/flycast.git" ]] || fail "Dedicated Flycast build folder has an unexpected origin: $REMOTE"
    # This folder is reserved for repeatable builds. Reset only this dedicated checkout so
    # a previous run's patch/overlay cannot break the next run's pinned-base check.
    git -C "$FLYCAST_SRC" fetch --quiet origin "$BASE"
    git -C "$FLYCAST_SRC" reset --hard "$BASE"
    git -C "$FLYCAST_SRC" clean -ffd
fi
git -C "$FLYCAST_SRC" fetch --quiet origin "$BASE"
git -C "$FLYCAST_SRC" checkout --detach "$BASE"
git -C "$FLYCAST_SRC" submodule update --init --recursive
for dep in libnfs libsmb2; do
    url="$(awk -v name="$dep" '$1 == name ":" {print $3; exit}' "$RESONANCE_DIR/patches/BASE.txt")"
    sha="$(awk -v name="$dep" '$1 == name ":" {print $2; exit}' "$RESONANCE_DIR/patches/BASE.txt")"
    [[ "$sha" =~ ^[0-9a-f]{40}$ ]] || fail "Could not read a valid pinned $dep revision from patches/BASE.txt: $sha"
    [[ "$url" =~ ^https://github\.com/[^[:space:]]+$ ]] || fail "Could not read a valid HTTPS GitHub URL for $dep from patches/BASE.txt: $url"
    if [[ ! -d "$PS4_BUILD_ROOT/src/$dep/.git" ]]; then
        [[ ! -e "$PS4_BUILD_ROOT/src/$dep" ]] || fail "$PS4_BUILD_ROOT/src/$dep exists but is not a Git checkout."
        git clone "$url" "$PS4_BUILD_ROOT/src/$dep"
    fi
    DEP_SRC="$PS4_BUILD_ROOT/src/$dep"
    DEP_REMOTE="$(git -C "$DEP_SRC" remote get-url origin 2>/dev/null || true)"
    case "$dep:$DEP_REMOTE" in
        "libnfs:https://github.com/sahlberg/libnfs"|"libnfs:https://github.com/sahlberg/libnfs.git"|"libnfs:git@github.com:sahlberg/libnfs.git"|"libsmb2:https://github.com/sahlberg/libsmb2"|"libsmb2:https://github.com/sahlberg/libsmb2.git"|"libsmb2:git@github.com:sahlberg/libsmb2.git") ;;
        *) fail "$DEP_SRC has an unexpected origin: $DEP_REMOTE" ;;
    esac
    git -C "$DEP_SRC" fetch --quiet origin "$sha"
    # build-netfs.sh patches libsmb2's CMakeLists.txt locally; reset only these
    # dedicated dependency trees so every build starts from the exact pinned source.
    git -C "$DEP_SRC" reset --hard "$sha"
    git -C "$DEP_SRC" clean -ffd
done

step "3/7 — Apply Resonance PS4 patch and WHAT'S NEXT overlay"
PATCH="$RESONANCE_DIR/patches/flycast-ps4.patch"
if git -C "$FLYCAST_SRC" apply --reverse --check "$PATCH" >/dev/null 2>&1; then
    echo "Resonance PS4 patch already applied."
elif git -C "$FLYCAST_SRC" apply --check "$PATCH" >/dev/null 2>&1; then
    git -C "$FLYCAST_SRC" apply "$PATCH"
else
    fail "Resonance PS4 patch does not apply cleanly to pinned Flycast $BASE. Use a clean build tree."
fi
if ! grep -q 'whatsNextBridge.begin_real_frame' "$FLYCAST_SRC/core/rend/vulkan/vulkan_context.cpp" ||
   ! grep -q 'Mini-DLSS spatial finishing pass' "$FLYCAST_SRC/core/rend/vulkan/frame_gen.cpp"; then
    bash "$ROOT/scripts/apply-resonance-whats-next.sh" "$FLYCAST_SRC"
else
    echo "WHAT'S NEXT overlay already present."
fi
grep -q 'whatsNextBridge.commit_real_frame' "$FLYCAST_SRC/core/rend/vulkan/vulkan_context.cpp" || fail "Missing WHAT'S NEXT history commit hook."
grep -q 'Mini-DLSS spatial finishing pass' "$FLYCAST_SRC/core/rend/vulkan/frame_gen.cpp" || fail "Missing Mini-DLSS renderer finishing pass."

step "4/7 — Seed PS4 build tree"
bash "$RESONANCE_DIR/scripts/seed-build-tree.sh" 2>&1 | tee "$LOG_DIR/seed-build-tree.log"

step "5/7 — Build network filesystem dependencies"
bash "$RESONANCE_DIR/scripts/build-netfs.sh" 2>&1 | tee "$LOG_DIR/build-netfs.log"

step "6/7 — Compile and package PS4 application"
# Force a clean native build and inspect logrun's recorded exit status. The upstream
# wrapper prints error summaries but does not propagate ninja's exit code reliably.
bash "$RESONANCE_DIR/scripts/rebuild-flycast.sh" clean 2>&1 | tee "$LOG_DIR/rebuild-flycast.log"
BUILD_LOG="$EXPECTED_BUILD_ROOT/logs/build.log"
[[ -f "$BUILD_LOG" ]] || fail "Build log was not created: $BUILD_LOG"
grep -qx 'exit=0' "$BUILD_LOG" || fail "Native Flycast/PS4 compilation failed; last build log lines: $(tail -8 "$BUILD_LOG" | tr '\n' ' ')"
EBOOT="$PS4_BUILD_ROOT/build/flycast/shell/ps4/flycast_eboot/eboot.bin"
[[ -s "$EBOOT" ]] || fail "Native build did not produce a non-empty eboot.bin: $EBOOT"
rm -f "$RESONANCE_DIR/out/Resonance-v$VERSION.pkg"
bash "$RESONANCE_DIR/scripts/package-resonance.sh" 2>&1 | tee "$LOG_DIR/package-resonance.log"

step "7/7 — Verify output"
PKG="$RESONANCE_DIR/out/Resonance-v$VERSION.pkg"
[[ -s "$PKG" ]] || fail "Expected package was not produced: $PKG. Inspect logs in $LOG_DIR."
printf 'Verified package file: %s\n' "$PKG"
ls -lh "$PKG"
printf 'Build logs: %s\n' "$LOG_DIR"
printf 'The PKG was created by the PS4 toolchain. Console installation and visual/performance testing still require a PS4.\n'
