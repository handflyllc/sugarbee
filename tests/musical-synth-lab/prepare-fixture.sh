#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$repo_root/build/musical-synth-lab"
"$repo_root/build/sbee-pack" -i "$repo_root/examples/musical-synth-lab" \
    -o "$repo_root/build/musical-synth-lab/musical-synth-lab.vcart" \
    -t "Musical Synth Lab" --uuid 7499fd87-b134-47f5-bdf8-0889dce5ad9d
