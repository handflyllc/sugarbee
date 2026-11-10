# Vendored yyjson

Upstream: https://github.com/ibireme/yyjson
User-supplied release tag: **0.13.0** (header version agrees).
Files supplied 2026-11-08; MIT license retained unchanged.
User source references: yyjson.c `2c9c76a`, yyjson.h `7ed1758`.
These references are not the local Git blob IDs. Network access was unavailable;
no independent upstream tag/content match has been claimed.

SHA-256 of supplied, unmodified files:

- yyjson.c: `6a33e74e136bb69816bfea785148673b8ddf3e532626430f7fcfbfab0015ea20`
- yyjson.h: `5970801d177ab775ba02b768a7bea66df92313965ab2ce77aa034960d0cea22d`
- LICENSE: `45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947`

SugarBee builds the reader with writer/utilities disabled. Strict read flags,
raw numeric tokens, and a bounded per-load allocator are selected in its loader.
No source patches, system installation, or build-time download required.
