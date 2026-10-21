#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/flycast-after-resonance-patch" >&2
    exit 2
fi

flycast_root="$(cd "$1" && pwd)"
patch_file="$(cd "$(dirname "$0")/.." && pwd)/dreamcast/patches/resonance-whats-next.patch"

if [[ ! -f "$flycast_root/core/rend/vulkan/vulkan_context.cpp" ||
      ! -f "$flycast_root/core/rend/vulkan/frame_gen.cpp" ]]; then
    echo "This is not the patched Flycast source tree: $flycast_root" >&2
    echo "First apply Resonance's patches/flycast-ps4.patch to its pinned Flycast checkout." >&2
    exit 2
fi

git -C "$flycast_root" apply --check "$patch_file"
git -C "$flycast_root" apply "$patch_file"
echo "WHAT'S NEXT renderer integration patch applied to: $flycast_root"
