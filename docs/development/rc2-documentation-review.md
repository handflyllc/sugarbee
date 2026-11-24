# RC2 documentation review — 2026-11-23

Reviewed against `41d43d0`, retaining the agreed `1fb9542` feature scope. This is
contract/documentation reconciliation, not a whole-engine correctness audit or
new desktop/Pi acceptance. No runtime code or packaging scripts changed.

| Area | Reconciliation |
| --- | --- |
| Status and history | README and roadmap identify RC2 preparation and the 204-test development checkpoint; RC1 acceptance retained separately |
| Version semantics | `include/vcart.h`, `src/vcart.c`, packer and version tests confirm an existing numeric minimum-runtime check; RC1 and current main both use `1.0.0`, distinct from header format 1 and release labels |
| Graphics | Removed obsolete denial of arbitrary sprite rotation; existing pivot, tint, canvas and clipping contracts remain authoritative |
| Lifecycle and architecture | Summary now includes native music, module/data ownership, text entry and graphics-state boundaries |
| Build and controls | Serial CTest guidance, current Pi status, stable source UUID workflow and Enter-versus-Escape behavior reconciled |
| API documentation | Checked registered services and relevant bounds against headers/implementations; clarified requested-member duplicate handling and timing wording |
| Release documentation | Draft RC2 notes and pending acceptance gates added; RC1 packaging evidence explicitly historical |

The compatibility finding changes the wording of the post-RC2 proposal, not its
scope: improve the existing policy rather than inventing a first version check.
The existing triple does not express which prerelease APIs a game needs. Keep
current behavior unchanged for RC2 and document tested requirements for shipped
examples. Design legacy handling and breaking-version policy separately.

## Remaining release work

- Refresh the package and fixture scripts plus their example lists together;
  current scripts still select the RC1 six-cartridge binary set.
- Select representative native-music and newer graphics/text examples, document
  their tested requirements, and verify staged licenses/notices and dependencies.
- Perform fresh native Release and independently extracted distribution checks
  on desktop and Pi; historical passing tests are not substitutes.
- Finalize release notes, build identity, matching tag/artifacts and checksums
  after acceptance. No RC2 package or publication is certified here.

The [RC2 checklist](release-checklist.md) tracks these gates. The NO CARRIER
friction report remains discussion input for post-release priorities; this review
does not turn its fifteen entries into release requirements. No new input, font,
audio or batching work was introduced.

## Review verification

Local Markdown target existence checks passed (404 links at review time), as
did `git diff --check`. Host and packer help output confirmed the documented
command forms; CTest discovery listed 204 tests. The suite was not rerun for
these documentation-only changes. Link checking verifies file targets, not
external URLs or every heading anchor.
