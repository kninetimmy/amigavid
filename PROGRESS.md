# Progress

## Milestone 0 — archive baseline

- Baseline: [docs/BASELINE.md](docs/BASELINE.md).
- Local reference input verified: `vc.zip` is 21,193,157 bytes with SHA-256
  `ef4622f337e4e6331fdc97d5da6ec54d35f0d39bcd28ab898ce3689b8fb36ee3`.
  Its ZIP CRC check found no bad member; the central directory has 1,225 files
  and 38 directory records.
- `python tools/inventory.py` verifies the pin before listing the archive
  without extraction. The archive remains under ignored `inputs/`; no original
  source, assets, or binaries are tracked here.

## Open blockers

- `DT_STUFF.S`, vector tables/media, demo video media, and font resources are
  absent from the archive.
- Bundled third-party code and media need separate redistribution review,
  including the Commodore-Amiga notice in `vc/modules/screen.c`.
- No source-disc, CD/full-motion media, or target hardware evidence is
  available yet.

## Next milestone

Qualify the Open Watcom v2 Win32 compatibility probe. This baseline includes
no build, runtime, or target-OS test claim.
