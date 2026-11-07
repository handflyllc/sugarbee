#!/usr/bin/env bash
# Native Linux packages. Final artifacts require a clean committed checkout.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "Usage: bash tools/package-release.sh BUILD_DIR VERSION [--preview]" >&2
    exit 1
fi
build_arg=$1
version=$2
preview=${3:-}
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?$ ]] || { echo "Invalid version" >&2; exit 1; }
[[ -z "$preview" || "$preview" == --preview ]] || { echo "Unknown option" >&2; exit 1; }
if [[ -z "$preview" && -n "$(git status --porcelain)" ]]; then
    echo "Commit readiness changes before final packaging, or use --preview." >&2
    exit 1
fi
case "$(uname -m)" in
    x86_64) arch=x86_64 ;;
    aarch64|arm64) arch=arm64 ;;
    *) echo "Only native Linux x86_64 and ARM64 packages are supported." >&2; exit 1 ;;
esac
[[ "$(uname -s)" == Linux ]] || { echo "Linux required" >&2; exit 1; }
cmake -S "$repo_root" -B "$build_arg" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON -DSUGARBEE_BUILD_HOST=ON -DSUGARBEE_BUILD_PACKER=ON
build_dir="$(cd -- "$build_arg" && pwd)"
cmake --build "$build_dir" -j4 --target sugarbee sbee-pack generate_wav_lab
suffix=""
[[ -z "$preview" ]] || suffix="-preview"
name="sugarbee-$version-linux-$arch$suffix"
output="$(mktemp -d "$build_dir/package.XXXXXX")"
stage="$output/$name"
mkdir -p "$stage/bin" "$stage/cartridges" "$stage/examples" "$stage/docs/release" "$stage/third_party/font8x8"
cp "$build_dir/sugarbee" "$build_dir/sbee-pack" "$stage/bin/"
cp LICENSE RELEASE_NOTES.md "$stage/"
cp docs/release/BINARY_README.md "$stage/README.md"
# Keep current API references and their linked repository documentation together.
while IFS= read -r -d '' path; do
    mkdir -p "$stage/$(dirname "$path")"
    cp "$path" "$stage/$path"
done < <(git ls-files -z docs third_party)
cp docs/release/BINARY_README.md docs/release/PACKAGING.md "$stage/docs/release/"
cp src/font8x8.c "$stage/third_party/font8x8/font8x8.c"
# Retain source and documentation for each distributed cartridge. Fixed IDs
# keep Save Lab identity and example identities stable between package builds.
names=(pong-lab synth-lab musical-synth-lab save-lab sprite-sheet-demo)
uuids=(b9ab687f-a074-47d6-9836-a5ec95591e11 5f5040f9-6d44-485d-a923-f977d76cd2de 7499fd87-b134-47f5-bdf8-0889dce5ad9d 7a09c254-7fd8-4a87-9c44-742d33028ac1 1a4a0877-3042-4fad-a51e-9144ce296ec8)
copy_example() {
    while IFS= read -r -d '' path; do
        mkdir -p "$stage/$(dirname "$path")"
        cp "$path" "$stage/$path"
    done < <(git ls-files -z "examples/$1")
}
for i in "${!names[@]}"; do
    example=${names[$i]}
    copy_example "$example"
    "$stage/bin/sbee-pack" -i "$stage/examples/$example" \
        -o "$stage/cartridges/$example.vcart" -t "$example" --uuid "${uuids[$i]}"
done
copy_example wav-lab
cmake -DSOURCE="$repo_root/examples/wav-lab" -DSTAGE="$output/wav-source" \
    -DGENERATOR="$build_dir/generate_wav_lab" -DPACKER="$stage/bin/sbee-pack" \
    -DOUTPUT="$stage/cartridges/wav-lab.vcart" -P tests/wav-lab/prepare.cmake
cp -R "$output/wav-source/assets" "$stage/examples/wav-lab/"
{
    printf 'Version: %s\nArchitecture: %s\nCommit: %s\n' "$version" "$arch" "$(git rev-parse HEAD)"
    if [[ -n "$preview" ]]; then
        printf 'PREVIEW: local working-tree changes may be included; not a published RC.\n'
    else
        printf 'Source: clean committed checkout\n'
    fi
    printf 'Built UTC: %s\n' "$(date -u +%FT%TZ)"
    cat /etc/os-release
    getconf GNU_LIBC_VERSION
    cmake --version | head -1
    grep -E '^CMAKE_C_COMPILER:FILEPATH=' "$build_dir/CMakeCache.txt"
    printf '\nRuntime libraries (resolved on build host, not bundled):\n'
    for executable in sugarbee sbee-pack; do
        printf '\n%s\n' "$executable"
        ldd "$stage/bin/$executable"
    done
} > "$stage/BUILD-INFO.txt"
if grep -q 'not found' "$stage/BUILD-INFO.txt"; then
    echo "Unresolved runtime library; packaging stopped." >&2; exit 1
fi
(cd "$stage" && sha256sum bin/* cartridges/* > CONTENTS.sha256)
tar -C "$output" -czf "$output/$name.tar.gz" "$name"
if [[ -z "$preview" ]]; then
    git archive --format=tar.gz --prefix="sugarbee-$version-source/" \
        -o "$output/sugarbee-$version-source.tar.gz" HEAD
fi
(cd "$output" && sha256sum ./*.tar.gz > SHA256SUMS)
printf '\nArtifacts and checksums: %s\n' "$output"
printf 'Extract outside this checkout and complete docs/release/BINARY_README.md checks.\n'
