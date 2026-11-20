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

The Open Watcom Win32 probe has a repeatable build and a Windows 11 host
smoke test. The unchanged kit also passes on all four QEMU guest targets:
Windows 98 FE (4.10.1998), 98 SE (4.10.2222), x86 XP RTM
(2600.xpclient.010817-1148), and x86 XP Professional SP3
(2600.xpsp.080413-2111). Physical-hardware and editor-release validation
remain open. Before milestone 2, the next independent step was to decode
and display actual ILBM artwork. The viewer now passes its fixture tests,
Windows 11 smoke test and four-guest display, hide/restore and failed-open
recovery checks with the same executable. XP RTM/SP3 match the exact
canonical RGB24 frames; 98 FE/SE match every pixel after the independently
measured Cirrus display-channel conversion. That conversion merges two
background grays into one displayed color (16 displayed colors from 17
canonical colors), as allowed by issue #16; no tolerance is used. See the
[probe matrix](PROGRESS.md#runtime-evidence) and
[viewer evidence](PROGRESS.md#milestone-2--real-asset-ilbm-viewer).

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
`.probe/HOST-SMOKE.txt` for the observed results. The recorded Windows 11
run reported actual `waveOut=3`; its `/noaudio` check was simulated, not a
true zero-output-device test.

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

See [the historical milestone 0 archive baseline](docs/BASELINE.md) for the
input map, retained reference material, blockers, attribution, and pre-probe
target-status matrix. Its pending rows are historical; use the
[live progress matrix](PROGRESS.md#runtime-evidence) for current status.

## Build and test the ILBM viewer

On modern x64 Windows with Python 3.13+, run from the repository root. Put
the pinned ZIP described above at `inputs/vc.zip`. For `test`, have a host
GCC or Clang on `PATH` (`CC` can name the compiler executable). For `smoke`,
use an NTFS checkout and an unlocked desktop at 100% scale. Leave the
viewer visible and unobscured.

```powershell
python tools/probe.py bootstrap
python tools/asset.py prepare
python tools/asset.py build
python tools/asset.py test
python tools/asset.py smoke
```

`prepare` optionally takes a local ZIP path. It verifies the pinned ZIP
and copies only `vc/data/blankeditor.iff` and
`vc/vcpics/jolyons/artimages/blueface.16` to `inputs/Viewer Assets/`.
`tools/inventory.py` still only lists metadata; `prepare` is the separate
command that extracts the two files. `build` needs the same verified
Watcom cache as the probe but no artwork. `test` feeds the two fixtures and
small generated cases to the portable C decoder linked into the GUI; it
checks exact top-down RGB24 size and digests. `smoke` logs actual window
pixels, repaint, dialogs, recovery and path-rejection results to
`.asset/HOST-SMOKE.txt`, and writes two native-window BMPs to `.asset/`.
Keep the original assets, BMPs and executables local; do not commit or
publish them.

Run the viewer manually from any working directory with an explicit asset
root. For example, from the checkout:

```powershell
$root = (Resolve-Path '.\inputs\Viewer Assets').Path
& (Resolve-Path '.\.asset\VCASSET.EXE').Path $root
```

The background opens first. Use **File → Background** and **File →
Picture** to switch or retry after an error. The viewer paints at 1:1 at
the upper left of the client area; it is not the editor. Both menu choices
use an indexed DIB and realize the image palette on paint and palette
activation/change, rather than mapping true-color pixels to the default
system palette on 256-color displays. Their fixed leaf names are resolved
only under the selected absolute local drive folder, never under the
working directory.
This restriction applies to both names, not just `blueface.16`. Relative,
parent-traversing, UNC and device paths are not accepted. On NT the viewer
still rejects reparse points (including junctions), multiply-linked files
and non-regular files. Previously it checked each directory but used
absolute paths for later opens. Now it opens the directly mapped local
volume's root once per read and opens each subsequent name relative to
its checked parent handle. It refuses directory aliases (such as `SUBST`)
and does not fall back to absolute paths if safe resolution fails.
On 9x it still requires a local FAT/FAT32 drive and opens by absolute
path. Use ANSI-representable paths within `MAX_PATH`. The decoder handles
indexed 1–8 plane ILBM with raw or ByteRun1 rows (up to 1024×1024 and
4 MiB input); HAM, Extra Half-Brite, masks and other formats are not
supported here.

## First useful demo and release acceptance

The PRD's first useful demo goal is to open real editor artwork on Windows
98, add two pictures and a timed transition, save the project, and play that
file on XP with the same executable.

Release acceptance is stricter: the same portable ZIP must pass the edit,
save, reopen and playback workflow on both Windows families. A modern-host
smoke test does not qualify either target, and guest probe passes are not
editor-release or physical-hardware validation.
