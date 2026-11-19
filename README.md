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

Before milestone 1, the archive baseline was documented but no probe had
been built or run. Now the Open Watcom Win32 probe has a repeatable build
and a modern-host smoke test. Windows 98 and XP qualification is pending.

## Build and test the Win32 probe

From a clean checkout on modern x64 Windows with Python 3.13 or later, run
these commands in order from the repository root:

```powershell
python tools/probe.py bootstrap
python tools/probe.py build
python tools/probe.py inspect
python tools/probe.py package
python tools/probe.py smoke
```

Only `bootstrap` needs the network on a clean checkout. It downloads the
[dated Open Watcom v2 C release](https://github.com/open-watcom/open-watcom-v2/releases/tag/2026-11-01-Build)
Windows x64 host asset (ID `697737741`, 128,145,854 bytes) and checks its
official SHA-256, `d4e496c593ef2acbc54d1eaeb31b31d9c62f86e39a09e61ada411ef34feaa01a`.
A mismatched download or cached file is rejected. The build rechecks the
cache before extracting tools to a temporary checkout-local folder. It
does not run the installer, make a global installation or change your
environment. No original archive or assets are needed.

The host compiles with `wcc386 -bt=nt -bg -3r` and links with `wlink` using
`system nt_win`, the ANSI `WinMain` startup and GUI subsystem version 4.0.
The build normalizes the unsigned executable's COFF timestamp, and ZIP
entries have fixed timestamps. `.probe/build/PE-IMPORTS.txt` lists the PE
headers, imports and build provenance; `probe.map` and `probe.lst` in that
same folder show the linker inputs and application-object disassembly.

On an interactive, unlocked host desktop, `smoke` runs the extracted
32-bit executable from a directory with spaces with a different working
directory. It samples the painted DIB, posts mouse and keyboard window
messages, checks the actual WinMM count and simulated no-audio status,
and checks clean exits and missing/malformed-file dialogs. See
`.probe/HOST-SMOKE.txt` for the observed results.

Verify `.probe/VCPROBE.ZIP` against `.probe/VCPROBE.ZIP.sha256` on the
modern host. On each target, follow the included [TEST.TXT](probe/TEST.TXT):
extract to a folder with spaces, then launch offline from Explorer or a
Windows `COMMAND.COM` session. The kit includes no target-side developer
tools and requires no separately installed CRT. This is a local test kit,
not the editor or a release. See [PROGRESS.md](PROGRESS.md) for results.

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
