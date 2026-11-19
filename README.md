# AmigaVid

AmigaVid aims to bring Almathera's Amiga CD32 Video Creator to Windows 98 and
XP as a native, offline, 32-bit x86 Win32 port. No Amiga emulator or ROM is
required at runtime.

See [Video-Creator-Win32-PRD.md](Video-Creator-Win32-PRD.md) for detailed
requirements and acceptance evidence.

## Expected phases

0. **Establish the baseline:** verify the pinned archive and inventory source
   files and assets.
1. **Prove the Windows toolchain:** show a native probe opening a window and
   exercising input.
2. **Open a real asset:** decode and display actual editor artwork.
3. **Edit, save and replay one sequence:** save, reopen and replay a
   two-image sequence with a timed transition.
4. **Port all effects and special screens:** port vectors, QuadAnims, text and
   palette effects.
5. **Complete media, editor and persistence:** finish CD audio, cue marking
   and Random Raves.
6. **Qualify and package:** qualify the same portable ZIP on both Windows
   families.

## Current focus

The archive baseline is documented. The next implementation milestone is the
Open Watcom Win32 compatibility probe; no probe has been built or run.

## Archive inventory

The original archive is a local reference input, not repository content.
Download [`vc.zip`](https://archive.org/download/vc_20221206/vc.zip) from the
pinned source in the PRD, place it at the ignored path `inputs/vc.zip`, then
run:

```text
python tools/inventory.py
```

The standard-library-only command verifies SHA-256
`ef4622f337e4e6331fdc97d5da6ec54d35f0d39bcd28ab898ce3689b8fb36ee3` before it
reads ZIP metadata. It never extracts or executes archive members, and its
source and asset listing is deterministic. A missing archive or checksum
mismatch exits nonzero with a diagnostic on stderr and produces no inventory.
The script also accepts a local ZIP path as its only argument when the archive
cannot be placed under `inputs/`.

See [the archive baseline](docs/BASELINE.md) for the input map, retained
reference material, blockers, attribution, and target-status matrix.

## First useful demo and release acceptance

The first useful demo opens real editor artwork on Windows 98, adds two
pictures and a timed transition, saves the project, and plays that file on XP
with the same executable.

Release acceptance is stricter: the same portable ZIP must pass the edit,
save, reopen and playback workflow on both Windows families. A modern-host
smoke test does not qualify either target.
