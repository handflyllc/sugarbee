#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cmake -DSOURCE="$repo_root/examples/wav-lab" \
    -DSTAGE="$repo_root/build/wav-lab/source" \
    -DGENERATOR="$repo_root/build/generate_wav_lab" \
    -DPACKER="$repo_root/build/sbee-pack" \
    -DOUTPUT="$repo_root/build/wav-lab/wav-lab.vcart" \
    -P "$repo_root/tests/wav-lab/prepare.cmake"
