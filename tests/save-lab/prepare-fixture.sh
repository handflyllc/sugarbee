#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
fixture="$repo_root/build/save-lab"
packer="$repo_root/build/sbee-pack"
if [[ ! -x "$packer" ]]; then
    echo "Build SugarBee and sbee-pack first." >&2
    exit 1
fi
mkdir -p "$fixture"
# Stable identity across rebuilds. Never remove or overwrite the save directory.
"$packer" -i "$repo_root/examples/save-lab" -o "$fixture/save-lab.vcart" \
    -t "Save Lab" --uuid 7a09c254-7fd8-4a87-9c44-742d33028ac1
printf '\nFrom the repository root, run:\n'
printf './build/sugarbee --save-dir ./build/save-lab/saves ./build/save-lab/save-lab.vcart\n'
