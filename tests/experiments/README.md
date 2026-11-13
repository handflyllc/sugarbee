# Historical pitch feasibility experiments

These scripts and the saved Pi measurements belong to investigation checkpoint
`de04c52`, before runtime integration. Reproduce the temporary-source candidate
experiments from that revision in a separate checkout. Their checked textual
patches intentionally refuse source that has already integrated the new range;
do not weaken those guards or overwrite the preserved Pi evidence to make them
run against a later runtime.

Current runtime verification uses the CMake/CTest checks and
`build/sbee-pitch-test --piano`, documented in
[the runtime checkpoint](../audio-piano-runtime.md).

The Python files require NumPy except the standalone benchmark and its candidate
helper. Generated captures, binaries and bundles live under ignored `build/`.
