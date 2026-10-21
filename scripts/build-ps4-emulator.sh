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
FLYCAST_SRC="$PS4_BUILD_ROOT/src/flycast"
LOG_DIR="$PS4_BUILD_ROOT/logs"
OUT_DIR="$PS4_BUILD_ROOT/out"
VERSION="${RESONANCE_VERSION:-01.00}"

fail() { printf '\nBUILD STOPPED: %s\n' "$*" >&2; exit 1; }
step() { printf '\n========== %s ==========\n' "$*"; }

[[ "$VERSION" =~ ^[0-9]{2}\.[0-9]{2}$ ]] || fail "RESONANCE_VERSION must use XX.YY format (for example 01.00)."
for cmd in git python3 cmake ninja; do
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

mkdir -p "$PS4_BUILD_ROOT" "$LOG_DIR" "$OUT_DIR"
export PS4_BUILD_ROOT
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
if [[ ! -d "$FLYCAST_SRC/.git" ]]; then
    [[ ! -e "$FLYCAST_SRC" ]] || fail "$FLYCAST_SRC exists but is not a Git checkout; move it aside before cloning."
    git clone --recursive https://github.com/flyinghead/flycast.git "$FLYCAST_SRC" 2>&1 | tee "$LOG_DIR/clone-flycast.log"
fi

BASE="$(awk '$1 == "base:" {print $2; exit}' "$RESONANCE_DIR/patches/BASE.txt")"
[[ -n "$BASE" ]] || fail "Could not read the pinned Flycast revision from patches/BASE.txt."
git -C "$FLYCAST_SRC" fetch --quiet origin "$BASE"
git -C "$FLYCAST_SRC" checkout --detach "$BASE"
git -C "$FLYCAST_SRC" submodule update --init --recursive
for dep in libnfs libsmb2; do
    url="$(awk -v name="$dep" '$1 == name \\( $0 ~ name \\) {print $3; exit}' "$RESONANCE_DIR/patches/BASE.txt")"
    sha="$(awk -v name="$dep" '$1 == name {print $2; exit}' "$RESONANCE_DIR/patches/BASE.txt")"
    [[ -n "$url" && -n "$sha" ]] || fail "Could not read pinned $dep source from patches/BASE.txt."
    if [[ ! -d "$PS4_BUILD_ROOT/src/$dep/.git" ]]; then
        [[ ! -e "$PS4_BUILD_ROOT/src/$dep" ]] || fail "$PS4_BUILD_ROOT/src/$dep exists but is not a Git checkout."
        git clone "$url" "$PS4_BUILD_ROOT/src/$dep"
    fi
    git -C "$PS4_BUILD_ROOT/src/$dep" fetch --quiet origin "$sha"
    git -C "$PS4_BUILD_ROOT/src/$dep" checkout --detach "$sha"
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
bash "$RESONANCE_DIR/scripts/rebuild-flycast.sh" 2>&1 | tee "$LOG_DIR/rebuild-flycast.log"
bash "$RESONANCE_DIR/scripts/package-resonance.sh" 2>&1 | tee "$LOG_DIR/package-resonance.log"

step "7/7 — Verify output"
mapfile -t PKGS < <(find "$RESONANCE_DIR" "$PS4_BUILD_ROOT" -type f -name '*.pkg' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -5 | cut -d' ' -f2-)
(("${#PKGS[@]}" > 0)) || fail "Build scripts finished but no .pkg was found. Inspect logs in $LOG_DIR."
printf 'Found package candidate(s):\n'
printf '  %s\n' "${PKGS[@]}"
printf '\nBuild logs: %s\n' "$LOG_DIR"
printf 'Note: this script can compile/package only when the local PS4 SDK/sysroot prerequisites are already installed. A successful host test alone is not a PS4 build.\n'
