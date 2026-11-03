#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$repo_root/build/synth-lab"
"$repo_root/build/sbee-pack" -i "$repo_root/examples/synth-lab" \
    -o "$repo_root/build/synth-lab/synth-lab.vcart" -t "Synth Lab" \
    --uuid 5f5040f9-6d44-485d-a923-f977d76cd2de
