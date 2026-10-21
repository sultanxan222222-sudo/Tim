#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/flycast-after-resonance-patch" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$repo_root/scripts/integrate-resonance-whats-next.py" "$1"
