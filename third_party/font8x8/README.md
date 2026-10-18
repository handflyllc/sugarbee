# Embedded font provenance

SugarBee uses the basic Latin bitmap table from Daniel Hepper's font8x8.

- Repository: https://github.com/dhepper/font8x8
- Revision: `8e279d2d864e79128e96188a6b9526cfa3fbfef9`
- Source: https://github.com/dhepper/font8x8/blob/8e279d2d864e79128e96188a6b9526cfa3fbfef9/font8x8_basic.h
- Upstream source SHA-256: `49d8df366296b203ca3211bc0672cf2a762135bf12710735b6292756b19dffd5`
- Upstream declares the font Public Domain. Its original attribution and
  declaration are retained in `src/font8x8.c` and `upstream-README.txt`.
- Upstream credits Marcel Sondaar and IBM public-domain VGA fonts. This records
  upstream's declaration and provenance; it does not establish the origin of
  SugarBee's former, unattributed template font.

Adaptations: make the table const unsigned char and reverse bits within each
row to match SugarBee's most-significant-bit-first renderer. Glyph indices
remain U+0000 through U+007F. The runtime renders printable ASCII 32 through
126, treats newline specially, and substitutes `?` for unsupported bytes.
This is not Unicode/UTF-8 text rendering.

The font data is third-party material under its upstream public-domain
declaration, not an original Handfly, LLC font claimed under the root license.
The former embedded template data was removed from the current source and
historical template; earlier Git revisions still contain it. No history was rewritten.
