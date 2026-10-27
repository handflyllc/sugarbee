#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
packer="$repo_root/build/sbee-pack"

if [[ ! -x "$packer" ]]; then
    echo "Build SugarBee and sbee-pack before running this script." >&2
    exit 1
fi

fixtures="$(mktemp -d /tmp/sugarbee-fixtures.XXXXXX)"

mkdir -p "$fixtures/scan/nested"
mkdir -p "$fixtures/scroll"
mkdir -p "$fixtures/empty"
mkdir -p "$fixtures/no-entry"

# Three eligible cartridges.
"$packer" -i "$repo_root/examples/pong-lab" \
    -o "$fixtures/scan/pong.vcart" -t "Pong Lab"

"$packer" -i "$repo_root/examples/invaders-lab" \
    -o "$fixtures/scan/invaders.vcart" -t "Invaders"

"$packer" -i "$repo_root/tests/shutdown" \
    -o "$fixtures/scan/shutdown.vcart" -t "Shutdown Test"

# Invalid header: an empty file.
touch "$fixtures/scan/invalid.vcart"

# Valid package header, but no main.lua.
touch "$fixtures/no-entry/notes.txt"
"$packer" -i "$fixtures/no-entry" \
    -o "$fixtures/scan/missing-main.vcart" -t "Missing Entry Test"

# These must be ignored by the top-level .vcart scan.
touch "$fixtures/scan/notes.txt"
cp "$fixtures/scan/pong.vcart" "$fixtures/scan/nested/hidden.vcart"

# Ten entries to exercise scrolling and duplicate titles.
for number in {1..10}; do
    printf -v filename 'pong-%02d.vcart' "$number"
    cp "$fixtures/scan/pong.vcart" "$fixtures/scroll/$filename"
done

printf '\nFixtures ready: %s\n' "$fixtures"
printf '  scan:   3 eligible, 2 rejected, 3 stored\n'
printf '  scroll: 10 eligible, 0 rejected, 10 stored\n'
printf '  empty:  0 eligible, 0 rejected, 0 stored\n'
printf '\nRun from the repository root:\n'
printf './build/sugarbee --cartridges "%s/scan"\n' "$fixtures"
printf './build/sugarbee --cartridges "%s/scroll"\n' "$fixtures"
printf './build/sugarbee --cartridges "%s/empty"\n' "$fixtures"
