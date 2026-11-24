#!/usr/bin/env bash
# Package the compact RC acceptance set using the selected build's packager.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-$repo_root/build-rc2-release}"
build_dir="$(cd -- "$build_dir" && pwd)"
packer="$build_dir/sbee-pack"
if [[ ! -x "$packer" || ! -x "$build_dir/generate_wav_lab" ]]; then
    echo "Build the host, packager and WAV generator first." >&2
    exit 1
fi
out="$build_dir/acceptance"
mkdir -p "$out" "$build_dir/acceptance-guru"
for name in pong-lab pacman-demo sprite-sheet-demo synth-lab musical-synth-lab; do
    "$packer" -i "$repo_root/examples/$name" -o "$out/$name.vcart" -t "$name"
done
"$packer" -i "$repo_root/examples/save-lab" -o "$out/save-lab.vcart" \
    -t "Save Lab" --uuid 7a09c254-7fd8-4a87-9c44-742d33028ac1
# Match the four RC2 additions and their stable distribution identities.
names=(song-change canvas-demo rotation-demo panes-demo)
uuids=(250c23a3-e794-4931-9e15-c4d9a6cf9037 7cc81559-25ee-490b-a115-6a8ae64e75b4
       8b172fb6-918d-4d55-9d0c-f675432bc216 6b58f806-8162-47bc-b220-832d93e684ce)
for i in "${!names[@]}"; do
    "$packer" -i "$repo_root/examples/${names[$i]}" \
        -o "$out/${names[$i]}.vcart" -t "${names[$i]}" --uuid "${uuids[$i]}"
done
cmake -DSOURCE="$repo_root/examples/wav-lab" \
    -DSTAGE="$build_dir/wav-lab/source" -DGENERATOR="$build_dir/generate_wav_lab" \
    -DPACKER="$packer" -DOUTPUT="$out/wav-lab.vcart" \
    -P "$repo_root/tests/wav-lab/prepare.cmake"
cat > "$build_dir/acceptance-guru/main.lua" <<'LUA'
function init() end
function update(dt) while true do end end
function draw() sugarbee.cls(0,0,0) end
LUA
"$packer" -i "$build_dir/acceptance-guru" -o "$out/execution-limit.vcart" -t "Execution Limit"
printf '\nAcceptance cartridges: %s\nSave data is preserved; this script does not remove saves.\n' "$out"
