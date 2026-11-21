# Progress

## Milestone 0 — archive baseline

- Historical baseline: [docs/BASELINE.md](docs/BASELINE.md); its pre-probe
  compatibility matrix is not the current target status.
- Pinned source ZIP: [`vc.zip`](https://archive.org/download/vc_20221206/vc.zip)
  from Archive.org item [`vc_20221206`](https://archive.org/details/vc_20221206)
  is 21,193,157 bytes with SHA-256
  `ef4622f337e4e6331fdc97d5da6ec54d35f0d39bcd28ab898ce3689b8fb36ee3`.
  Its ZIP CRC check found no bad member; the central directory has 1,225 files
  and 38 directory records.
- `python tools/inventory.py` verifies the pin before listing the archive
  without extraction. The archive remains under ignored `inputs/`; no original
  source, assets, or binaries are tracked here.

### Recovered and missing files

**European retail disc — inspected 2026-11-20:** Archive.org item
[`commodore_amiga_cd32`](https://archive.org/details/commodore_amiga_cd32),
package `Video Creator (Europe).zip`, SHA-256
`95943f5c817482548b8186b46edca45f76b28a12c287117303d1efabf991f13f`.
Its SHA-1 matched the item metadata
(`c5a99369895256aa0dc412b2b08cfa106ab514e5`), and the ZIP CRC check passed.
This is a separate retail reference input, not the pinned developer source ZIP.

Recovered retail content:

- **13 `.vid` projects with VC10 headers**, including `1_demovideo.vid`
  (3,037 bytes). `MPEG_Example.vid` is a VC10 project, not a full-motion stream.
  Header recognition does not establish reader compatibility or playback.
- **Three font descriptors and their bitmap strikes:** `VCSansSerif.font`
  with `VCSansSerif/25`, `VCThin.font` with `VCThin/12`, and
  `VideoCreator.font` with `VideoCreator/12`.
- **Vector filename coverage: 30/30 retail-disc catalog entries; 41/44
  source-ZIP catalog entries.** This comparison is case-insensitive filename
  only, not path matching, byte identity, format validation or rendering evidence.

**Two compiled payloads recovered:** `vc:vectors/TEST` (an 84-byte `VECT`
quadrilateral) and `SineTable.bin` (6,144 bytes) were recovered from the
source ZIP's `vc/3dvectorcode.obj`. Each matches byte-for-byte in all four
editors: source ZIP `vc/editor` and `vc/editordb`, plus retail `editor` and
`editord`. `TEST`'s final zero may be assembler `EVEN` alignment padding
rather than part of the standalone file. Standalone originals remain welcome
for provenance.

All 918,572 bytes of retail `vec.cdfastload` are accounted for: it is
byte-identical to the concatenation of all 41 known animations, with no
additional bytes.

Five files remain missing from the inspected source ZIP and retail disc:

- Datatype helper source: `DT_STUFF.S` (the source ZIP has `dt_stuff.o` only).
- Developer include: `vc:vectors/Macros.i`.
- Source-catalog VecAnims: `dancer2.vec`, `dancer5.vec`, `dancer7.vec`.

The retail image has **Track 01 MODE1/2352 data** and **Track 02 AUDIO**.
These are retail runtime media, not a developer source disc or evidence of
CD transport, optional MPEG-device behavior or full-motion playback.
Original-reader loading/playback of recovered projects and acceptance of
Windows-written projects still need reference runs. Font metrics and vector
format/rendering fidelity also need verification; locating files alone does
not close those checks.

All original inputs remain local under ignored `inputs/`; none is published
here. Recovery does not grant redistribution rights. The
[missing-file notice](README.md#have-a-video-creator-disk-or-backup) explains
how to report a lead without uploading anything.

## Milestone 1 — Open Watcom Win32 probe

Before milestone 1 the next step was to qualify the Open Watcom probe;
milestone 0 itself made no build, runtime, or target-OS test claim.
The probe now builds and passes the Windows 11 host smoke test and the
four QEMU guest checks below (Windows 98 FE/SE and x86 XP RTM/SP3).
Physical-hardware and editor-release qualification remain open.
[Build and test commands](README.md#build-and-test-the-win32-probe).

### Build and PE evidence

- Open Watcom v2 `2026-11-01-Build`, asset `697737741`: the 128,145,854-byte
  x64 Windows host download matched its published SHA-256
  `d4e496c593ef2acbc54d1eaeb31b31d9c62f86e39a09e61ada411ef34feaa01a`.
  Both `bootstrap` and `build` rejected a same-size copy with a changed
  byte (exit 1, no build). A fresh download and build in a second directory
  with spaces reproduced the EXE, ZIP and PE report byte for byte.
- `.probe/build/PE-IMPORTS.txt` contains the complete direct import list,
  PE fields, build flags, tool versions and hashes; `.probe/build/probe.map`
  identifies the linked library members. `WDUMP` and GNU `objdump` 2.46.1
  agree on x86 PE32, Windows GUI subsystem 2, OS/subsystem 4.0, entry RVA
  `0x168a`, and direct imports from `GDI32`, `KERNEL32`, `USER32` and `WINMM`.
  No CRT DLL or delay imports. The entry is `clib3r.lib(cstrtwwt)`, which
  leads to `__WinMain` and the probe's ANSI `WinMain`; the C startup/runtime
  is static, not replaced with a custom entry point.
- The compiler's `-3r` applies to all functions in the probe C unit, not
  to the prebuilt CRT or system DLLs. `.probe/build/probe.lst` shows app
  object instructions. A linked-image `objdump -d -M intel` spot check
  found no SIMD register use or selected later-x86 instructions (`cmov`,
  `cpuid`, `rdtsc`, `bswap`, `cmpxchg`, `xadd`); this is not a CPU-floor test.
- The application uses ANSI Win32 entry points for all text/path calls,
  including companion lookup (ANSI-representable paths under `MAX_PATH`).
  That is **not** an ANSI-only import rule for the whole EXE: the CRT
  statically imports both `GetCommandLineW` and `GetModuleFileNameW`.
  [CRT startup source](https://github.com/open-watcom/open-watcom-v2/blob/2026-11-01-Build/bld/clib/startup/c/mainwnt.c)
  checks for null from the former; the [CRT module-name wrapper](https://github.com/open-watcom/open-watcom-v2/blob/2026-11-01-Build/bld/clib/libwin32/c/getmodfn.c)
  uses ANSI plus conversion on 9x, but that does not remove the wide
  import from the loader's requirements. The CRT also looks up the
  environment-string APIs at runtime and optionally resolves
  `USER32!GetActiveWindow` on an exception path; these fallback paths
  were source-inspected, not separately exercised. The guest launches
  below establish loader success on those installations, not a complete
  stock-DLL export/transitive-dependency audit or physical CPU-floor test.
  Windows 11's WOW64 and API-set dependencies are not evidence for 98/XP.

### Runtime evidence

| Target | Status and evidence |
| --- | --- |
| Windows 11 Pro x64 26300.9974 (development host) | **Host smoke pass**: Ryzen 7 5700X3D, 64 GiB, Radeon RX 9070 XT, 3840×2160×32, WinMM reported three wave outputs; no optical drive enumerated. Not a target-OS test. |
| Windows 98 First Edition (4.10.1998) | **QEMU guest probe pass** — Explorer/`COMMAND.COM`, DIB/repaint, input, actual `waveOut=0`, closure and companion recovery as detailed below. |
| Windows 98 Second Edition (4.10.2222) | **QEMU guest probe pass** — same checks. |
| Windows XP RTM (x86, 2600.xpclient.010817-1148) | **QEMU guest probe pass** — same checks. |
| Windows XP Professional SP3 (x86, 2600.xpsp.080413-2111) | **QEMU guest probe pass** — same checks. |

**Windows 11 host smoke (not target evidence)**

`.probe/HOST-SMOKE.txt`: launched the extracted EXE with a different
working directory. Read red, green, blue, red and white samples from the
actual painted DIB. Posted mouse and Space messages changed the caption;
Esc and `WM_CLOSE` each exited 0. A missing companion displayed an error
and exited 2. Empty data, trailing byte, bad magic and out-of-range
palette index each displayed an invalid-data error and exited 3.
Restoring the companion allowed relaunch and exit 0. Separately
`.probe/EXPLORER-SMOKE.txt` records Windows 11 shell open from a directory
with spaces and exit 0. No physical keystrokes or `COMMAND.COM` run were
claimed on this host. The EXE queries `waveOutGetNumDevs` and never opens an
audio device. `/noaudio` exercised the shared no-audio status and clean
exit with `simulated` and the true count of 3 in the caption; no real
zero-output-device run was available on the host.

**QEMU target-OS observations**

All four guests used the unchanged checksummed kit identified by the ZIP
digest below, extracted to a guest folder with a space, with no added CRT.

- Windows 98 FE and SE: QEMU TCG, Pentium, 64 MiB RAM, Cirrus graphics at
  640×480 with 256 colors, no NIC or sound card, empty IDE CD-ROM.
- x86 XP RTM: QEMU TCG, Pentium III, 256 MiB RAM, standard VGA at
  640×480×32, no NIC or sound card, IDE CD-ROM with the installation CD.
- x86 XP Professional SP3: `winver` 5.1, build
  `2600.xpsp.080413-2111 : Service Pack 3`; a separate overlay of the RTM
  baseline was upgraded with Microsoft-signed SP2 and SP3. QEMU TCG,
  Pentium III, 256 MiB RAM, standard VGA at 640×480×32, no NIC or sound
  card, empty IDE CD-ROM (unlike the RTM run). All three files on the
  read-only transfer floppy matched the pinned kit byte for byte.
- On each guest, offline launches from Explorer and a Windows
  `COMMAND.COM` session passed. Framebuffer pixel sampling confirmed the
  red/green/blue/red quadrants and white cross at launch, after repaint
  and after companion recovery.
- On SP3, Show Desktop hid and restored the window. Host framebuffer
  samples were exact red/green/blue/red/white at launch, after restore
  and after DAT recovery; samples while hidden differed.
- Mouse click and Space changed the caption to `Mouse` and `Space`.
  Esc and the window's X both closed the probe. Numeric target process
  exit codes were not captured.
- Each guest reported actual, unsimulated `No audio (WinMM waveOut=0)`
  without `/noaudio`. This verifies the zero-device status path, not
  audio playback; the probe only counts devices.
- Missing `VCPROBE.DAT` displayed the missing-companion dialog; a
  zero-length DAT displayed the invalid-companion dialog. Restoring the
  original DAT allowed relaunch from Explorer with the expected image.
- After SP3 guest shutdown, `qemu-img check` found no errors; the RTM
  backing disk's hash was unchanged.

These are guest probe passes, not physical-hardware or editor-release
validation. The Windows 11 `/noaudio` check above remains simulated.

| Recorded local artifact (not tracked) | SHA-256 |
| --- | --- |
| `.probe/VCPROBE.ZIP` (sidecar `.probe/VCPROBE.ZIP.sha256`) | `a317af6fe0277a8eb9895bc24d204efeb389d84ce7a636ad15d1073b3c1b3760` |
| `.probe/build/VCPROBE.EXE` | `0c26c1b8e7582507e1a83b8bd257bfc29b12f62013c6b994cc16bc568461f050` |
| `.probe/build/VCPROBE.DAT` | `cc41816184f41d388d76455fca98f88c48137c850ec073c153d231186bb40fe4` |
| `.probe/build/PE-IMPORTS.txt` | `3c7cc60661e27fa583dad64e0250940efde5b0b8ca78aadfb6060552243e797b` |

## Milestone 2 — real-asset ILBM viewer

- **Host decode pass:** both original fixtures decoded as 320×256
  top-down RGB24, 245,760 bytes apiece, with the independent FFmpeg hashes
  below. The numeric `.16` extension does not change how the decoder
  identifies the format. `blueface.16` counts one final zero alignment byte
  in its compressed BODY; the decoder accepts it only after all plane rows.
  Generated cases cover raw and compressed word-padded rows, plane order,
  literal/repeat/no-op handling, truncation, row overruns, excess BODY data,
  oversized dimensions and an invalid palette index.
- **Viewer build:** the pinned Open Watcom v2 `2026-11-01-Build` produced
  x86 PE32 GUI, OS/subsystem 4.0, static `WinMain` startup, and direct
  GDI32, KERNEL32 and USER32 imports (no WinMM, CRT DLL or delay imports).
  `VCASSET.EXE` SHA-256:
  `c77277f639030c9bcd5e4022e88a1563ed9043507e3c81eb362c92ad5b14d163`.
  `.asset/PE-IMPORTS.txt` and `.asset/viewer.map` are local build evidence.
  The palette correction did not change the direct DLL list. `NtOpenFile` and
  `QueryDosDeviceW` are resolved at runtime on NT only.
- **Viewer host smoke pass:** Windows 11 Pro x64 build 26300.9974. The
  viewer opened and switched images from a root with spaces and a
  different working directory. Five painted client pixels per image and
  the full 320×256 client crop in each native-window screenshot matched
  the FFmpeg reference. Invalidating the blacked-out client area restored
  the sampled pixels. Missing and corrupt opens showed filename-specific
  dialogs; repaint kept the current picture, and the next valid open worked
  in the same window. The smoke also rejected parent traversal, an ancestor
  junction, a leaf junction, a multiply-linked file and a regular file in
  a directory position.
  `.asset/HOST-SMOKE.txt`, `background.bmp`, and `picture.bmp` are local
  output under `.asset/`, not release assets.
- **NT containment by source inspection:** after opening and checking the
  directly mapped volume root, the resolver passes only one name and the
  already checked parent handle to each `NtOpenFile` call. It opens
  reparse points without following them and rejects them by handle before
  descending or reading. The checked chain stays open through the read;
  no subsequent absolute lookup or fallback occurs on NT. This applies
  to both menu choices. See Microsoft's [native file-open semantics](
  https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile).
  The smoke checks ordinary paths and static rejection, not concurrent
  changes or every filesystem and filter combination.

| Fixture | Top-down RGB24 SHA-256 |
| --- | --- |
| `blankeditor.iff` | `268d851d1941790a3019c94c3a3b8319259246680808b969b8d768923fa3b9bf` |
| `blueface.16` | `622a27248ed4f36102cd514712c0e1a83667cb114b1afa9c153b33cced55e585` |

### Four-guest palette comparison — 2026-11-19

**Issue #16 calibrated-output pass, reverified 2026-11-20 UTC:** the viewer
passes display, hide/restore and failed-open recovery on all four guests.
Windows 98 matches the separately calibrated display conversion exactly;
XP matches the canonical RGB24 references exactly. This is viewer guest
qualification, not physical-hardware or full-editor validation.

- **Original failure:** executable
  `d2fbc0c3a1ab8213fb238fcf0b5669bce8334973dc008a88b96cff95de2c4d85`
  displayed only 4 background colors (reference: 17) and 2 portrait colors
  (reference: 16) on both 98 editions. XP was exact. The old 32-bit
  `StretchDIBits` path supplied no image palette. Realizing a palette alone
  still let the RGB conversion merge nearby colors.
- **Correction:** carried forward `asset/viewer.c` unchanged from commit
  `869ab33ef764ccc10a0815cd58405fd47caaf5fe`. Both fixed asset choices share
  an 8-bit, DWORD-row-padded DIB with logical palette indices
  (`DIB_PAL_COLORS`). The viewer creates non-collapsing entries from the
  decoded colors, selects/realizes the palette while painting, and handles
  `WM_QUERYNEWPALETTE`/`WM_PALETTECHANGED`. Replaced palettes are released;
  failed reads, decodes or display allocation leave the previous bitmap and
  palette intact. Decoder and path rules are unchanged.
- **One executable on all guests:** the fresh #16 build reproduced the
  current viewer-build hash above and was byte-identical to the preserved
  read-only transfer kit's `VCASSET.EXE`. Both pinned inputs and all kit
  manifest hashes were rechecked; the four guest `FC /B` captures show no
  difference between installed EXEs and the kit. Existing guest evidence
  was therefore reused, not rerun. Each guest used a fresh overlay of its
  previously qualified baseline. QEMU 11.1.0 (`v11.1.0-12130-ge470268ff4`),
  TCG, no NIC or sound card, no installation CD; assets were copied to
  local `C:\M2VIEW`.
- **Display configurations:** 98 FE 4.10.1998 and SE 4.10.2222: Pentium,
  64 MiB, Cirrus 640×480×256, reported `BITSPIXEL=8`, `COLORRES=18`.
  XP RTM 2600.xpclient.010817-1148 and SP3 2600.xpsp.080413-2111:
  Pentium III, 256 MiB, standard VGA 640×480×32, reported
  `BITSPIXEL=32`, `COLORRES=24`. A preliminary unrestricted-TCG FE boot
  hit an NDIS Windows protection error before the viewer ran. Final 98
  overlays used `-icount shift=3,align=off,sleep=on`; graphics configuration
  was unchanged.

Each foreground crop is 320×256, top-down RGB24, 81,920 pixels. Fresh
independent FFmpeg decodes reproduced both canonical fixture hashes above.
Both initial foreground frames and both Show Desktop hide/restore frames
were compared in full, not just sampled or color-counted; hidden crops
differed. The restored frames and failed-open retention frames also match
every pixel. No pixel tolerance, artwork adjustment or display-mode change
was used.

| Guest | Background: canonical / display-converted matching pixels; displayed colors | Portrait: canonical / display-converted matching pixels; displayed colors | After hide/restore |
| --- | --- | --- | --- |
| 98 FE | 848 / 81,920; 16 (17 canonical) | 0 / 81,920; 16 (16 canonical) | Both byte-identical to initial calibrated frames |
| 98 SE | 848 / 81,920; 16 (17 canonical) | 0 / 81,920; 16 (16 canonical) | Both byte-identical to initial calibrated frames |
| XP RTM | 81,920 / 81,920; 17 | 81,920 / 81,920; 16 | Both exact canonical hashes |
| XP SP3 | 81,920 / 81,920; 17 | 81,920 / 81,920; 16 | Both exact canonical hashes |

**Independent channel calibration:** a separate synthetic Win32 brush probe,
not the viewer or artwork, painted all 256 grayscale input values in four
64-entry palette pages. Uniform-tile assertions rechecked every R, G and B
channel on both 98 guests against
`T(c) = 4 * (c >> 2) + 3 * ((c >> 2) & 1)`; both XP guests were identity
for all 256 values. Applying the measured per-channel lookup to each
canonical fixture accounts for every 98 pixel difference, with zero residual
mismatches. The 98 framebuffer hashes, identical on FE and SE, are:

- Background: `fea8d0674c7ccd1234123cf4eea7ce2882f33aeaa5eb60b1a3571b9009f91338`.
- Portrait: `9cec74bf88ae16ed810504c2f51e0b9b7dd6582b46121c5b768d2d64525b2550`.

Canonical background colors `(41,41,43)` and `(43,43,43)` both convert to
`(40,40,40)`, leaving 16 distinct displayed RGB colors. This is the one
calibration-explained collision; there is no unexplained color loss.
Issue #15 required 17 distinct displayed background colors, so its original
`RESULTS.txt` and checker correctly remain a failure of that obsolete gate.
The approved #16 criterion counts the two calibrated grays as one displayed
color. The new qualification does not rewrite the old result.

**Recovery on each of FE, SE, RTM and SP3:** requesting the initially absent
`blueface.16` showed “Missing asset. Check the selected asset root.”
After dismissal, the complete background crop was unchanged. Copying the
original picture into place allowed File → Picture in the same window.
Replacing only the overlay's background with its first 100 bytes produced
“blankeditor.iff: Incorrect FORM length.” The complete portrait crop survived
dismissal unchanged; restoring the original background and selecting
File → Background in the same window reproduced the initial background hash.
All eight filename-specific error dialogs and four binary-comparison captures
were reviewed again for #16.

**Local verification for #16:** `python tools/asset.py build`, `test` and
`smoke` all passed; the decoder checked both canonical hashes and 16
generated cases. The Windows 11 smoke retained its full-pixel, repaint,
recovery and path-rejection checks. `git diff --check` passed. The full
README was reviewed and its viewer status updated; probe, archive, build,
path-security and release instructions remain applicable. Historical
`qemu-img check` results passed for all four shut-down overlays, with
unchanged backing-disk hashes in `bases-before.json`/`bases-after.json`;
those disks were not booted again for this re-verification.

**Private evidence (not tracked):**

- `F:\AmigaVid\inputs\issue-15-869ab33\m2-pal03\`: original `RESULTS.txt`,
  manifest, kit, per-guest commands/captures/`pixels.json`, combined
  `comparisons.json`, overlays and backing-hash records. The parent preserves
  the calibration source, build/capture scripts and old `verify_pal03.py`.
- This #16 checkout's ignored `inputs/verify_issue16.py` independently
  decodes the pinned inputs with FFmpeg, rechecks all calibration tiles and
  full guest crops against #16, and asserts equality with the original
  per-guest/combined records. It passed without changing preserved evidence;
  `inputs/issue16-evidence-hashes.json` fingerprints the preserved scripts,
  captures and kit (not the disks or old QMP sockets).
- This checkout's `.asset/` holds the byte-identical rebuilt EXE,
  `PE-IMPORTS.txt`, `HOST-SMOKE.txt`, native-window BMPs, decoder-test log and
  `ISSUE16-GUEST-VERIFY-PASS.txt`. Original artwork, captures, executables and
  guest disks remain local.

### Before/after inventory

| Element | Before → after |
| --- | --- |
| `README.md` | Probe, archive and release instructions still apply. The formerly pending real-asset development step now has viewer instructions and host/guest evidence, distinguishing calibrated 98 output from canonical XP output. |
| `PROGRESS.md` | Archive baseline and four probe passes still apply, with the same limits. Formerly pending viewer target runs now pass #16's calibrated-output gate; the original #15 gate failure is retained explicitly. |
| `.gitignore` | Existing local roots still ignored; `.asset/` is now ignored too. |
| `asset/ilbm.h`, `asset/ilbm.c` | No portable decoder → bounded indexed ILBM to top-down RGB24 with a matching release function. |
| `asset/viewer.c` | No real-asset window → Win32 viewer with two fixed menu choices. NT directory checks followed by absolute opens → an initial volume-root open followed only by one-name opens through checked parent handles, or refusal. True-color DIB with default palette → indexed DIB with image-palette realization and activation/change handling. The NT link checks, 9x FAT/FAT32 path, bounded read, decode and failed-open preservation remain. The shared path, parser and presentation rules cover **both** fixed leaf names. |
| `asset/decode_test.c` | No host decoder driver → native CLI exercising the GUI's decoder. |
| `tools/asset.py` | No viewer workflow → local fixture preparation, build, tests and smoke commands. Reuses the probe's pin/extraction and PE checks without changing them. |
| `tools/asset_smoke.py` | No viewer smoke harness → actual window tests with local-only captures and a log. All prior cases remain; a regular file in a directory position is now checked too. |
| `probe/probe.c`, `probe/TEST.TXT`, `tools/probe.py`, `tools/probe_smoke.py`, `tools/inventory.py` | Unchanged tools and probe kit. The probe still uses its original executable-relative synthetic DAT; viewer root selection is separate. The archive inventory still does no extraction. |

## Milestone 3 — two-picture editor

### Source-derived contract

- **Editor:** the original manual's first tutorial and `main.c`'s timeline
  drag use a horizontal marker to change an effect's start time.
  `GetTopWindowUnits` maps horizontal position to ticks; the move checks
  `veffects.c::CanEffectGoHere`, updates `Time`, and calls `SortEffects`.
  VCEDIT keeps the start-time drag, non-overlap and chronological order.
  This minimal timeline refuses a blocked drop rather than snapping to a
  neighbor. Both shipped effects are start/stop effects; this restriction
  is not attributed to all original effects (one-shot effects differ).
- **Clock and fields:** `audio.c::GetCurrentCDTime` divides track-relative
  75 Hz sectors by three. `mainloop.c::MainLoop` activates a timed effect
  when `MainTime >= Time*2`, but dispatches only one queued effect per
  field: startup `Bodge2` consumes field 0, delaying a first supported cue
  at tick 0 until field 1. It calls `fx.c::DoDisplay` once per field.
  VCEDIT uses 40 ms timeline ticks; deterministic Seek samples the logical
  output after both fields of each tick (only one slide update at tick 0
  for a slide starting there). It does not reproduce Amiga
  copper-list buffering or sync playback to a CD.
- **Effects:** `neweffects.c::InitVEffectClasses` registers `VET_Cut=1`
  and `VET_SlideOnUp=2`, both `OC_SCREEN`. `CutInit`/`CutFunc` in `fx.c`
  replace the base immediately (`CalculateLength` gives 1 tick).
  `SlideOnUpInit` starts at y=255; `CalculateParam` gives a field step
  of `ceil(256 / (2*a - 1))` for the duration parameter `a`.
  `DoDisplay` steps **before** rendering; `SlideOnUpFunc` places the
  incoming picture at `TOP + Value`, and `SlideOnUpEnd` replaces the
  base at zero. `videocreator.asm::CopLoopNoSquash` switches the displayed
  buffer at the split; incoming source row 0 enters at the bottom over
  the unchanged outgoing picture. The timeline placement length is
  separately `floor(ceil(256 / ceil(256 / (2*a))) / 2) + 1` ticks
  (`CalculateLength`). The `a` range 1–255 is the original duration
  slider's bound for Slide On Up and the other effects that share that
  parameter-control case in `neweffects.c`; it is not a rule for all
  effect IDs. Cut has no duration option. For `a=19` and start=50,
  the split is 241 at tick 50, 129 at 58, 3 at 67 and full incoming at 68.
- **Active VC10:** `keith.c::SaveVideo` emits the single `0xA9` header
  byte, author and track, ordered picture names with each byte advanced
  by one, `ZIP` for an absent second bitmap, vector/quad/3D counts, and
  cues (tick, effect, screen, `a`–`e`, object, shifted text or `NOTEXT`).
  `veffects.c::GetScreenNum`/`GetScreenStruct` reserve IDs 1–5 for special
  screens; ordinary and paired screen records start at 6. `LoadVideo`
  finds both halves of a paired screen among pictures loaded earlier in
  the listing. The zero object-number rule is shared by both supported
  `OC_SCREEN` effects (the original writer emits zero for that class),
  not all original object classes. Neither `docs/saveformat` nor `defvid`
  defines this active grammar. The archived `defvid` was checked
  separately: it lists seven unshifted names without an author, track or
  second-picture line, then three empty external-object tables and seven
  cues. None of its seven referenced paths exists in the pinned ZIP;
  the active parser rejects the historical fields rather than ignoring
  them.
- **Shifted byte correction:** the first port's reader rejected `0x7f`
  even though the original `+1` writer produces it for `~`. The active
  reader now decodes it as `~` in both names of every screen and in text
  on both supported effects. Filename, line and field bounds, other
  invalid-byte checks and the unshifted sentinels are unchanged.

### Host verification

**First implementation (before review fixes):**

- `python tools/editor.py build`: pinned Open Watcom
  `2026-11-01-Build`; PE32 x86 GUI, OS/subsystem 4.0, static WinMain,
  direct GDI32/KERNEL32/USER32 only, no delay imports. `VCEDIT.EXE` SHA-256:
  `29bcde7bbb6bd1e64c7f95bf693b11f970462835ddab8fe5c68ccecbf08e0196`.
- `python tools/editor.py test`: hand-encoded, synthetic active VC10
  fixture with two ordinary records and one paired record, including
  nested/space-containing names, nondefault metadata and effect parameters.
  LF and CRLF inputs reserialize to identical active-writer LF bytes.
  Frame geometry and 26 rejection cases passed.
- `python tools/editor.py smoke`: Windows 11 Pro x64 26300.9974. The
  native controls opened the two pinned ILBMs, placed Cut at 0 and Slide
  at 25 with `a=19`, dragged Slide to 50, saved and quit. The first
  process and the restarted process produced the same complete RGB24
  previews at ticks 0, 49, 50, 58, 67, 68 and 79, assembled independently
  from the canonical images and the source-derived split rows. Play on
  reload reached the final picture. The first save matched independent
  VC10 bytes (SHA-256
  `6d37e55f898bee12f2e7e11a73348b7725287b705f9b8851ec01121840324ec3`).
  The ordinary/paired fixture also round-tripped through the GUI byte
  for byte. Truncated and missing-asset projects, a junction reference
  and a hard-linked picture left the open sequence intact. A sharing
  violation blocked replacement without changing the previous bytes;
  an unlocked retry succeeded. A simulated interruption between backup
  and publish kept the old `.bak` recoverable when a retry was refused.
- The unchanged `python tools/asset.py build`, `test`, and `smoke` also
  passed: byte-identical viewer, both original decodes, 16 generated
  cases, full painted frames, repaint, failed-open retention and path
  rejection. No CI workflow was added.

**Corrected implementation:**

- `python tools/editor.py build`: same Watcom pin and PE32 x86 GUI 4.0,
  static WinMain and direct GDI32/KERNEL32/USER32 only. `VCEDIT.EXE`
  SHA-256 `347ef6482cadc59b23253ac72e13c5754beaf4c1a359bf8603725a46244ede17`.
- `python tools/editor.py test`: the original fixture and frame geometry
  still pass; a separate independently encoded LF/CRLF fixture uses
  `WARM~1.IFF` in an ordinary record and a paired reference and `Star Light~`
  as effect text. Both serialize to the exact expected `0x7f` bytes.
  All 27 rejection cases pass. The NT scratch test changes only the
  launch-path spelling in memory after pinning; the alternate scratch
  directory's contents are unchanged. The driver also forces publish
  failure after backup, tests successful rollback and retained backup
  when rollback fails, checks that retry does not consume that backup,
  and tests a successful publication with failed backup deletion.
  The failure injections are host-test-only, not editor options.
- `python tools/editor.py smoke`: same Windows 11 host, seven full-frame
  comparisons before/after restart, native authoring with `BG~1.IFF`,
  fixture round-trip, playback and failed-open retention. Replacements of
  multiply-linked and sharing-locked projects were refused without changing
  their bytes; the eligible retry succeeded and cleaned up. Independent
  first-save SHA-256:
  `8515a3d79c3a45a488cf6c5bf7ab899149e88cac66c42f70e7908e50bbb1a102`.
- Viewer `build`, `test`, and `smoke` were rerun on the host and passed
  with the same viewer EXE hash, two original decodes and 16 generated
  cases. The existing guest probe and viewer qualification was not rerun.

**Replacement protection and canonical-name fix:**

- `python tools/editor.py build`: same pin, PE32 x86 GUI 4.0, static
  WinMain, direct GDI32/KERNEL32/USER32, no delay imports. `VCEDIT.EXE`
  SHA-256 `7dbf2fc820d0776263763e6d130d3240f30cd4bfb3a4f19b4d7a21e78511e464`.
- `python tools/editor.py test`: both active LF/CRLF fixtures, exact
  output and frame geometry, and 27 rejection cases passed. Native NT
  scratch checks rejected a different DACL with empty staging and no
  rename, preserved that file's bytes and DACL, and preserved an
  ordinary project's inherited DACL on successful replacement. A real
  8.3 alias was refused; full long and literal `~` names replaced
  successfully. Missing metadata APIs refused safely. The existing
  pinned-directory, rollback, retained-backup and cleanup-warning cases
  still passed.
- `python tools/editor.py smoke`: Windows 11 repeated native authoring,
  seven full-frame comparisons before and after restart, paired fixture
  round-trip, failed-open retention and the linked, locked, eligible
  and interrupted replacement cases with this EXE.

**Named-stream retention fix:**

- `python tools/editor.py build`: same Watcom pin, PE32 x86 GUI 4.0,
  static WinMain, direct GDI32/KERNEL32/USER32, no delay imports.
  `VCEDIT.EXE` SHA-256
  `2b82625815aa1ffded5f36f12619ba181d3d243bb9abad8e7974c22575f15640`.
- `python tools/editor.py test`: the two LF/CRLF fixtures, exact VC10
  output and frame geometry, and 27 rejection cases passed. An NTFS scratch
  project with a benign `annotations` stream was refused before staging
  or renaming. Its main and auxiliary bytes were unchanged and no backup
  or temp file appeared. A missing stream API refused replacement but did
  not block a first save. Ordinary replacements and the preceding DACL,
  alias, pinned-directory and rollback controls passed.
- `python tools/editor.py smoke`: Windows 11 native authoring, exact
  first save, paired fixture round-trip, seven full-frame comparisons
  before and after restart, playback, failed-open retention, and linked,
  locked, eligible and interrupted replacement checks passed.

**Shared-marker selection (issue #20, 2026-11-20; historical):**

The issue #20 results below describe the preserved PR #21 builds and their
local logs, not the current hit rule. Issue #22 supersedes their target
expectations where a drawn body contains the pointer.

- The first implementation made stationary clicks cycle through cues at an
  isolated marker in timeline order and wrap. The status and tick box
  identify the selected start tick; selection does not edit cue data or
  mark the document unsaved. A drag keeps the selected cue instead of
  cycling on press. With no selection, an ordinary drag still takes the
  first hit.
- Initial implementation verification:
  `python tools/editor.py build`, `python tools/editor.py test` and
  `python tools/editor.py smoke` each passed (exit 0), locally, not in CI.
  Host: Windows 11 x64 build 26300, NTFS, Python 3.14.4 and GCC 16.1.0.
  Build: verified Open Watcom `2026-11-01-Build`, x86 PE32 GUI 4.0,
  static WinMain, direct GDI32/KERNEL32/USER32 only, no delay imports.
  `VCEDIT.EXE` SHA-256:
  `238705b45bee080783485c15ddd31b337434084b0ce45013883b0921335649ba`.
- That smoke checked three isolated Cut cues at ticks 10, 11 and 12 at
  pixel x=17, selection through 10/11/12/10, a stationary hit-area-edge click,
  byte-identical saved selection and clean close without an unsaved prompt.
  It compared a direct first-cue drag and a selected cue 11 → 503 drag
  against independently encoded VC10 bytes, retaining all other cues and
  parameters. Reordering, single-marker drags, rejected overlap and
  empty-track seek pass, as do all previous preview, persistence and
  recovery checks. The model fixtures, 27 rejection cases and NT save
  protection/fault tests also pass.
- Local build/import evidence and native smoke log are in `.editor/`.
  The full README was reviewed and the selection gesture documented;
  probe, archive, viewer, save-protection and release guidance still apply.
  This changed executable was exercised only on the modern host, not on
  Windows 98, XP, physical target hardware or an original Amiga reader.
  Earlier guest evidence remains historical; the next milestone is
  unchanged.

**First nearby-marker review correction (issue #20; refined below):**

- Hit testing now chooses the nearest marker within five pixels, with
  equal-distance ties resolved in timeline order. Only cues at that exact
  rendered x share a selection cycle or retain selection for a drag;
  selection at another pixel cannot steal the drag. Empty-track seeks
  clear the selected-tick status as well as the selection.
- The expanded smoke on the pre-review binary above failed with
  `shared marker expected tick 10, got 44` (exit 1), captured in
  `.editor/ISSUE20-REVIEW-BASELINE.txt`. This replaces the earlier
  unretained pre-selection baseline failure claim.
- After correction, `python tools/editor.py build`,
  `python tools/editor.py test` and `python tools/editor.py smoke` all
  passed locally (exit 0), not in CI, on the same Windows 11 x64 host.
  The verified Watcom pin, PE profile and direct imports are unchanged.
  `VCEDIT.EXE` SHA-256:
  `d3f0c5d64a7579724a9aa91c58418cfc39468d1efe6faad965390fe2b947f49c`.
- Native smoke now checks span 4025 with ticks 10/11/12 at x17 and tick
  44 at x22: cycling wraps to 10, excluding 44. Selection-only saves are
  byte-identical. The selected non-first cue 11 → 503 move, nearby cue
  44 → 22 move followed by an exact-x17 cue 10 → 503 drag, and an exact-x22
  cue 44 → 503 drag while cue 11 is selected all match independent VC10
  bytes. Nearest hits, both inclusive hit-area edges, outside seeks,
  cleared status, equidistant ties and clean selection-only close pass.
  The previous model, NT-save, preview and recovery checks also pass.
- An intermediate smoke attempt dropped only two pixels from the nearby
  marker and failed its move oracle; the test now drops three pixels,
  preserving the existing two-pixel no-move behavior. That attempt is in
  `.editor/ISSUE20-REVIEW-smoke.txt`; passing command logs are
  `.editor/ISSUE20-REVIEW-build.txt`, `.editor/ISSUE20-REVIEW-test.txt` and
  `.editor/ISSUE20-REVIEW-smoke-final.txt`. Build/import evidence and the
  final native results remain in `.editor/PE-IMPORTS.txt` and
  `.editor/HOST-SMOKE.txt`.
- The full README was reviewed again and its gesture description clarified;
  the remaining guidance is current. Only the modern host was exercised:
  no Windows 98/XP guest, physical target or original Amiga-reader run
  was performed for this correction.

**Ordinary-drag and captured-open review correction (issue #20; refined below):**

- The previous nearest-hit rule changed ordinary drags on distinct marker
  pixels. This revision retained the base's first hit unless the five-pixel
  window contained co-located cues. Distant shared groups had no effect,
  but an unrelated shared pixel inside the window could still change the hit.
- Before the fix, the expanded native smoke reproduced replacement during
  mouse capture: exit 1, `release after project replacement reused stale
  drag/selection`, retained in `.editor/ISSUE20-CYCLE2-BASELINE.txt`.
  Successful project opens now cancel capture and clear pending drag and
  selection; cue-index reads are guarded. A short no-op drag also reports
  the newly selected cue's tick instead of leaving the previous status.
- `python tools/editor.py build`, `python tools/editor.py test` and
  `python tools/editor.py smoke` each passed locally (exit 0), not in CI,
  on Windows 11 x64 build 26300, NTFS, Python 3.14.4 and GCC 16.1.0.
  The verified Watcom pin, PE profile and imports are unchanged.
  `VCEDIT.EXE` SHA-256:
  `3f9e9a6885eada57d9bd12aef5bcba4a535a9d756428879f23ac6be60bd09eef`.
- New saved-byte checks cover a no-selection x22 → x85 drag with separate
  unshared x17/x22 markers, both without and with a distant shared group:
  cue 10 at x17 moves to tick 503, as on base. Click selection agrees.
  Captured project replacement checks stationary and moving releases of
  an invalid old index and a reused valid index, with unchanged replacement
  bytes. A short x22 → x23 no-op drag reports tick 44 in both controls and
  saves unchanged bytes. All prior model, NT-save, preview, shared-marker,
  reordering, overlap, seek and recovery checks still pass.
- Command logs: `.editor/ISSUE20-CYCLE2-build.txt`, `-test.txt` and
  `-smoke.txt`; native results and build evidence remain in
  `.editor/HOST-SMOKE.txt` and `.editor/PE-IMPORTS.txt`. The full README was
  reviewed and its gesture description corrected; other guidance remains
  current. Only the modern host was exercised, not Windows 98/XP guests,
  physical target hardware or the original Amiga reader.

**Candidate-pixel and tick-field review correction (issue #20):**

- Clicks and drags keep the legacy first hit unless that candidate or the
  nearest candidate shares its exact pixel. Only then does the nearest
  candidate win, with ties kept in timeline order. Selection refreshes the
  tick box even when the selected tick already equals the playhead.
- The new smoke cases failed separately on `2f9baf0` (exit 1): selecting
  tick 10 left `999` in the tick box, and the x22 → x85 drag failed to
  move only tick 10 to 503 with an unrelated shared x27 marker. Logs:
  `.editor/ISSUE20-CYCLE3-base-tick.txt` and `-base-drag.txt`.
- `python tools/editor.py build`, `python tools/editor.py test` and
  `python tools/editor.py smoke` all passed locally (exit 0), not in CI.
  Windows 11 Pro x64 26300.9974, NTFS, Python 3.14.4, GCC 16.1.0.
  The Watcom pin, PE profile and imports are unchanged. `VCEDIT.EXE`
  SHA-256 `edf2f03587c8a2c24b3a5f4a3dcc377e9bb7c9d143bf993fecfe47a79af5066a`.
- Smoke compared exact saved VC10 bytes across six layouts: unshared
  candidates (no, distant, or incidental shared pixel), shared first
  candidate, shared nearest candidate, and both shared. It confirmed that
  typing `999` without submitting leaves it in the edit field, until
  clicking cue 10 at the existing playhead restores 10 in the field and
  identifies the cue in status. Shared 10/11/12/10 cycling, the non-first
  cue move, ties, adjacent markers, capture cancellation and earlier
  checks still passed.
- Local command logs: `.editor/ISSUE20-CYCLE3-build.txt`, `-test.txt`
  and `-smoke.txt`. The full README was reviewed and its gesture and
  guest-evidence wording updated. The new build was tested only on the
  modern host, not on Windows 98/XP guests, physical targets, or the
  original Amiga reader.

**Visible-marker targeting (issue #22, 2026-11-20):**

- **Original-source evidence:** In the SHA-256-pinned `inputs/vc.zip`,
  `vc/main.c:937–952,973–989,4586–4610` ties marker drawing and list
  append order together. `CheckTopWindowSquares`
  (`vc/main.c:1142–1204,1353–1358,1501–1504`) scans backward and tests
  the pointer against the square rectangle, so the last drawn hit wins.
  The rectangle is half-open (`vc/sebsgads.c:1214–1223`).
  `vc/main.c:1182–1185,1212–1214` captures and preserves the horizontal
  grab offset. The manual (`vc/docs/videocreator.manual:317–325`)
  describes dragging the numbered square sideways to change its start.
- **Win32 behavior:** Painting and input share the full marker rectangle.
  With no rectangle hit, the existing five-pixel first-hit fallback
  still applies. Cycling cues that share the winning start pixel is a
  deliberate Win32 extension, not a claim about the original editor.
- **Host evidence:** The new smoke failed against the preserved `95cbf79`
  build (exit 1): x20 chose tick 44 instead of the sole visible hit at
  tick 10. The corrected `python tools/editor.py build`,
  `python tools/editor.py test` and `python tools/editor.py smoke` passed
  locally (exit 0), not in CI, on Windows 11 Pro x64 26300.9974.
  `VCEDIT.EXE` SHA-256:
  `17a7c76f77b13b7283103a218846493f307959a2856bb61a64fa72b885fafe7c`.
- Native smoke verifies x20 vs. x22 across both mixed layouts and the
  singleton layout, first-hit fallback and empty seeks outside the row,
  and exclusive right/bottom edges of a long Slide. It checks matching
  status and tick text, wraparound, byte-identical selection saves,
  the grab offset, and independent VC10 bytes after moving only the
  selected non-first cue (11 → 503). Isolated moves, overlap refusal,
  capture cancellation on project replacement and existing authoring,
  preview, replay and save checks still pass. Local evidence is in
  `.editor/PE-IMPORTS.txt`, `.editor/HOST-SMOKE.txt` and
  `.editor/ISSUE22-BASELINE-smoke.txt`.
- The full README was reviewed. It documents the gesture and separates
  this host check from earlier guest evidence. No Windows 98/XP guest,
  physical target or original-reader test was run for this build.

**Vertical-drag selection (issue #24, 2026-11-20):**

- A stationary click now requires unchanged x and y throughout observed
  mouse movement and at release. Vertical movement retains the selected
  cue, including an excursion back to the press point or a release at a
  different y without a move message. Horizontal movement, grab offsets
  and the existing two-pixel horizontal no-move rule are unchanged.
- The expanded native smoke failed on the preserved issue #22 executable
  (SHA-256
  `17a7c76f77b13b7283103a218846493f307959a2856bb61a64fa72b885fafe7c`):
  exit 1, `vertical retains the selected non-first cue: expected selected
  cue at tick 11, got tick field '12'`. Failure evidence:
  `.editor/ISSUE24-BASELINE-smoke.txt`.
- `python tools/editor.py build`, `python tools/editor.py test` and
  `python tools/editor.py smoke` each passed locally (exit 0), not in CI,
  on Windows 11 x64 build 26300, NTFS, Python 3.14.4 and GCC 16.1.0.
  The verified Open Watcom `2026-11-01-Build` pin, x86 PE32 GUI 4.0,
  static WinMain, direct GDI32/KERNEL32/USER32 imports and absence of
  delay imports are unchanged. `VCEDIT.EXE` SHA-256:
  `3391163baa22b944936a4a5a9bf1f6de3978085bc43970c76499900aeefadb4e`.
- At constant x17, smoke selects cue 11 among ticks 10/11/12, then
  checks y320 → y330, y320 → y330 → y320, and y320 → release y330
  without any move message. Each retains cue 11 with agreeing tick/status,
  allows the next stationary click to cycle to 12, and saves unchanged
  independent VC10 bytes. Existing horizontal drags, hit targeting,
  captured-open cancellation, preview, playback and save/recovery cases
  still pass. Model fixtures, 27 rejection cases and NT save checks pass.
- Command logs are `.editor/ISSUE24-build.txt`, `-test.txt` and
  `-smoke.txt`; native results and build evidence are
  `.editor/HOST-SMOKE.txt` and `.editor/PE-IMPORTS.txt`. Existing verified
  local toolchain and archive inputs sufficed; no setup download or
  fixture publication was needed.
- The full README was reviewed; its gesture and technical instructions
  remain applicable. The overview/screenshots update stays with dependent
  issue #25. This revision was not run on Windows 98/XP guests, physical
  targets or an original Amiga reader; milestone 4 remains next.

### README illustration — issue #25

- `docs/images/editor-slide-on-up.png` is an actual, unretouched capture of
  the running editor on Windows 11 Pro x64 `26300.9974`, not a guest capture.
  The screen capture is cropped to the visible window bounds and encoded
  losslessly as a 594×411 PNG. The executable is the preserved issue #24
  build, SHA-256
  `3391163baa22b944936a4a5a9bf1f6de3978085bc43970c76499900aeefadb4e`;
  its checksum and normalized source hashes matched this checkout.
- The two 320×256 geometric ILBM test cards were created from scratch for
  this documentation, without archive imagery or third-party assets, and
  are dedicated to the public domain under [CC0-1.0](
  https://creativecommons.org/publicdomain/zero/1.0/). Publication of this
  demo content is cleared; original archive assets and guest captures stay
  private.
- Native controls loaded both cards, added Straight Cut at 0 and Slide On
  Up with duration 19, dragged Slide from 25 to 50, saved and reopened
  `demo.vid`, then sought to tick 58. All 81,920 captured preview pixels
  matched the independently composed cards at split row 129. PNG SHA-256:
  `7c73822055183ae226496db1ea4de297191019a5eed46ff2c3a3bcff2bc802dc`.
- The PNG decoded successfully with no metadata. A local README HTML
  rendering in installed Edge decoded the linked image at 594×411 with
  nonempty alt text; both the PNG and rendered introduction were visually
  checked.
- The full README was reviewed against the shipped commands and recorded
  evidence. Its introduction now separates the earlier guest-tested
  workflow from the later host-only timeline fixes; probe, archive, viewer,
  editor and save-protection instructions remain applicable. Milestone 4
  remains next. No runtime code or target-OS qualification changed here.

### Guest verification

**First implementation (before review fixes):**

QEMU 11.1.0 (`v11.1.0-12130-ge470268ff4`), offline, no added CRT, NIC or
sound card. Each guest used a fresh qcow2 overlay of the corresponding
Milestone 2 baseline. On both, `A:\SETUP.BAT` copied `VCEDIT.EXE`,
`BG.IFF` and `BLUE.16` to `C:\M3EDIT`; `FC /B` found no executable
differences from the read-only transfer media. Both guests ran
`C:\M3EDIT\VCEDIT.EXE C:\M3EDIT` with `VCEDIT.EXE` SHA-256
`29bcde7bbb6bd1e64c7f95bf693b11f970462835ddab8fe5c68ccecbf08e0196`.
The artwork bytes match the same two pinned host inputs despite their
8.3 aliases.

- **Windows 98 Second Edition 4.10.2222:** Pentium, 64 MiB, Cirrus
  640×480×256, `-icount shift=3,align=off,sleep=on`. Opened `BG.IFF` and
  `BLUE.16` in the GUI, added Cut at 0 and Slide with duration 19, dragged
  the Slide marker from 25 to 50, saved `DEMO.VID` to local FAT32, quit,
  and relaunched. Ctrl+O opened the saved project; Seek restored the same
  frames, and F5 played through to the final portrait.
- **Cross-guest transfer:** floppy export did not complete. After clean 98
  shutdown, the verification helper read the actual `C:\M3EDIT\DEMO.VID`
  from a read-only conversion of the 98 overlay. Its 133 bytes exactly
  matched independent expected VC10 output, with SHA-256
  `2c15b59562bcfbfc9aabbfd24355c88f9663e9d00b9363dfc323cb9e492995a3`.
  That file (not a reserialization) went onto the read-only XP floppy.
- **Windows XP Professional SP3 x86, 2600.xpsp.080413-2111:** Pentium III,
  256 MiB, standard VGA 640×480×32. Used
  `copy /b A:\DEMO.VID C:\M3EDIT\DEMO.VID` and
  `fc /b A:\DEMO.VID C:\M3EDIT\DEMO.VID` (no differences). Ctrl+O loaded
  the 98 save, each Seek rendered the expected frame, and F5 reached
  “Playback complete” with the final portrait.

Each capture below matched all 81,920 pixels of the independently decoded
and composited image. On 98 the expected RGB was transformed by the prior
measured Cirrus conversion from #16; on XP it was canonical RGB24. These
are deterministic Seek captures, not measurements of playback jitter or
physical-display timing.

| Tick | Windows 98 SE (same before quit and after reopen) | x86 XP SP3 (from 98 save) |
| --- | --- | --- |
| 49 (before Slide) | `fea8d0674c7ccd1234123cf4eea7ce2882f33aeaa5eb60b1a3571b9009f91338` | `268d851d1941790a3019c94c3a3b8319259246680808b969b8d768923fa3b9bf` |
| 58 (during Slide) | `c62114de49b671489aad6f1a0ea5efc8580482903f0ffd9afc7b3575e8cd98a5` | `3fa27dffbcca225ac50f6c049393713a2fb0a1c4b038fd7d65b472dbc69b50d0` |
| 68 (after Slide and at Play completion) | `9cec74bf88ae16ed810504c2f51e0b9b7dd6582b46121c5b768d2d64525b2550` | `622a27248ed4f36102cd514712c0e1a83667cb114b1afa9c153b33cced55e585` |

Private evidence under `.editor/guest/`: edition and binary comparisons,
launch commands, transfer bytes, per-guest input/capture logs, framebuffer
comparisons and base-disk hashes. `qemu-img check` passed for the 98 and XP
overlays after shutdown; their base disks and the XP RTM backing disk were
unchanged. The unchanged `xp-transfer.json` is labeled preliminary by its
sidecar; its `DEMO.VID` hash identifies a zero-filled copy, not the final
imported save. Final first-pass evidence is `xp-transfer.FINAL.json`,
`guest98.vid`, `kit/DEMO.VID`, `sp3/xp-identical-project.png` and
`comparisons.json`. The ignored `inputs/` holds local evidence helpers.
`KEEP.txt` marks the evidence directories and helpers for preservation
before local build cleanup or worktree removal.
The archived Amiga executables are present, but no working original-reader
environment was available. Original-reader compatibility, Windows 98 FE
and XP RTM editor runs, physical hardware, audio, the rest of the effects,
and release qualification remain unverified or out of scope.

**Corrected implementation (fresh guests, not reused captures):**

- Windows 98 SE `4.10.2222` and x86 XP Professional SP3
  `2600.xpsp.080413-2111` used the same QEMU versions, display modes and
  clean baselines as above, with fresh overlays. On both guests
  `A:\SETUP.BAT` installed the corrected EXE and original artwork bytes;
  `fc /b` matched the EXE against the read-only kit. The corrected EXE
  hash is `347ef6482cadc59b23253ac72e13c5754beaf4c1a359bf8603725a46244ede17`.
  On both guests `copy /b C:\M3EDIT\BG.IFF C:\M3EDIT\BG~1.IFF` supplied
  a filename containing `~`, and the editor command was
  `C:\M3EDIT\VCEDIT.EXE C:\M3EDIT`.
- On 98 SE, authored the two cues with `BG~1.IFF` and `BLUE.16`, dragged
  Slide from tick 25 to 50 with `a=19`, and saved `DEMO.VID` on FAT32.
  Quit, reopened the project and played to completion. After shutdown,
  read the actual 135-byte file from a read-only conversion of the
  overlay. Its bytes match the independent VC10 expectation, including
  the encoded `0x7f`. SHA-256:
  `3d04d2a152738c4c2e8c984acf1325b163c2c5d55489595009911968deeb3686`.
- On XP SP3, ran `copy /b A:\DEMO.VID C:\M3EDIT\DEMO.VID` and
  `fc /b A:\DEMO.VID C:\M3EDIT\DEMO.VID` (no differences), then
  opened the exact 98 save with Ctrl+O and replayed it to completion.
  All 11 full crops (ticks 49, 58 and 68 before/after restart on 98;
  the same ticks on XP; and Play completion on both) matched all 81,920
  expected pixels per crop, with the same hashes as the table above.
  The checker independently decodes the source artwork and applies the
  measured display conversion only on 98.
- In a separate overlay, XP SP3 also saved as `CHECK.VID` and replaced
  that copy with a second Save. `fc /b C:\M3EDIT\DEMO.VID C:\M3EDIT\CHECK.VID`
  reported no differences; the directory listing showed no `.bak` or
  `.tmp` remaining.
- Keep the private evidence at `.editor/guest-fix/` (`KEEP.txt`):
  `xp-transfer.FINAL.json`, `guest98.vid`, `kit/DEMO.VID`, per-guest
  commands, captures and framebuffer comparisons. `qemu-img check` found
  no errors in the two workflow overlays; all three baseline/backing
  disk hashes remained unchanged. The private `inputs/` verification
  helpers for this run have the `-fix.py` suffix. Neither the original
  Amiga reader nor physical Windows hardware was exercised.

**Replacement protection and canonical-name fix (fresh guests):**

- The same QEMU configurations and clean baselines were used on Windows
  98 SE `4.10.2222` and x86 XP Professional SP3 `2600.xpsp.080413-2111`.
  Both used the host build above. `A:\SETUP.BAT` installed the EXE and
  pictures; `FC /B` matched each guest EXE with the read-only transfer
  copy. Both copied `BG.IFF` to `BG~1.IFF` using `copy /b` and ran
  `C:\M3EDIT\VCEDIT.EXE C:\M3EDIT`.
- On 98 SE (FAT32), authored Cut at 0 and Slide at 25 with `a=19`, dragged
  Slide to 50, saved `DEMO.VID`, restarted, reopened, sought and played
  to completion. Read 135 bytes from the shut-down overlay, matching the
  independently encoded project and SHA-256
  `3d04d2a152738c4c2e8c984acf1325b163c2c5d55489595009911968deeb3686`.
- FAT32 replacement controls: `dir C:\M3EDIT` identified `LONGPR~1.VID`
  as the alias for `long project file.vid`. Open by alias succeeded; Save
  refused. Replacement of the literal `lit~1.vid` moved its Slide from
  tick 50 to 60 and left no `.bak` or `.tmp`. After placing a known `.bak`
  with `copy /b`, another Save refused. Offline FAT32 inspection verified
  the unchanged long name and original bytes, the updated literal-name
  bytes, and the separate backup's original bytes. This was not a forced
  mid-rename failure or a power-loss test.
- On XP SP3 (NTFS), `copy /b A:\DEMO.VID C:\M3EDIT\DEMO.VID` followed by
  `fc /b A:\DEMO.VID C:\M3EDIT\DEMO.VID` transferred the exact 98 save.
  Ctrl+O and F5 opened and replayed it to completion. The two guests'
  11 fresh full crops matched all 81,920 pixels per crop at the same
  ticks and hashes as above. The 98 crops use the independently
  measured Cirrus conversion; XP crops use canonical RGB24.
  XP also saved and then replaced an ordinary `CHECK.VID`; `fc /b`
  matched it to `DEMO.VID`, and `dir /a` showed no `.bak` or `.tmp`.
- Preserve `.editor/guest-acl/` (`KEEP.txt`, `MANIFEST.json`), including
  the final transfer manifest, original guest-written file, host logs,
  guest captures, commands and offline FAT verification. `qemu-img check`
  passed for both shut-down overlays; all three base/backing hashes were
  unchanged. Keep both earlier guest evidence sets and dependent private
  inputs. Original Amiga reader, 98 FE and XP RTM editor runs, physical
  hardware and release qualification remain unverified.

**Named-stream retention fix (fresh guests):**

- The same offline QEMU 11.1.0 configurations and clean baselines were
  used on Windows 98 SE `4.10.2222` and x86 XP Professional SP3
  `2600.xpsp.080413-2111`. On both, `A:\SETUP.BAT` installed the new
  checksummed EXE and pictures; `FC /B` found no EXE differences against
  the read-only kit. Both used `copy /b C:\M3EDIT\BG.IFF C:\M3EDIT\BG~1.IFF`
  and ran `C:\M3EDIT\VCEDIT.EXE C:\M3EDIT`.
- On 98 SE (FAT32), authored Cut at tick 0 and Slide at 25 with duration
  19, dragged Slide to 50, saved `DEMO.VID`, quit, reopened and replayed.
  After shutdown, read the actual 135 bytes from the overlay. They match
  the independent VC10 expectation, SHA-256
  `3d04d2a152738c4c2e8c984acf1325b163c2c5d55489595009911968deeb3686`.
- On XP SP3 (NTFS), `copy /b A:\DEMO.VID C:\M3EDIT\DEMO.VID` and
  `fc /b A:\DEMO.VID C:\M3EDIT\DEMO.VID` transferred and verified that
  exact save. Ctrl+O opened it, Seek checked ticks 49, 58 and 68, and
  F5 reached playback completion. All 11 fresh guest captures
  (including 98 before and after restart) matched every one of their
  81,920 pixels, with the same calibrated 98 and canonical XP hashes
  as in the table above.
- XP NTFS ordinary replacement: made a separate `CHECK.VID` with
  `copy /b`, opened it in VCEDIT, dragged Slide from 50 to 57, saved
  over that copy and reopened it at the new cue position. `fc /b`
  showed exactly the expected byte change (`30` to `37` at offset
  `0x6d`); the transferred `DEMO.VID` still matched the floppy and
  `dir /a /b` showed no `.bak` or `.tmp` files.
- Preserve `.editor/guest-stream/` (`KEEP.txt`, `MANIFEST.json`)
  separately from the three earlier guest evidence sets and their
  dependencies. Both overlays passed `qemu-img check` after shutdown
  and the base/backing hashes were unchanged. Named-stream refusal was
  tested on the host; XP verified eligible replacement. The earlier
  FAT32 replacement controls were not repeated in this cycle. Original
  Amiga reader, physical hardware, FAT power loss, and editor release
  qualification remain unverified.

### Before/after inventory

| Element | Before → after |
| --- | --- |
| `.gitignore` | Existing local outputs still ignored; `.editor/` is now ignored too. |
| `editor/vc10.h`, `editor/vc10.c` | No editor model or serializer → bounded VC10 author/track, screen order and ordinary/paired references, Cut/Slide cue order, ticks, parameters, text and sentinels. Unknown asset assigns, special screens, other effects and nonempty external-object tables are explicitly unsupported. `vc_place` rejects overlaps for both supported IDs; `vc_at` provides deterministic preview geometry. |
| `editor/local.h`, `editor/local.c` | No editor workspace I/O → bounded local reads and staged replacement. NT project and picture opens descend through checked, held directories; 9x uses local FAT. Save uses same-folder staged and backup names; callers must explicitly select an existing project to replace. Review fix: only NT reads were pinned → metadata checks and saves also use the pinned directory and exact file handles. Neither path overwrites a preexisting backup. The preexisting viewer's different file rules are unchanged. |
| `editor/editor.c` | No authoring UI → filename field, screen selector, effect buttons, drag/seek and Play/Stop, palette-aware preview, open/save/close and unsaved-change protection. The standalone viewer still does not edit. Paired VC10 references are retained but previewed as their first picture, not original `BitMap2` field alternation. |
| `editor/model_test.c`, `tools/editor.py`, `tools/editor_smoke.py` | No editor checks → portable model/fixture cases, pinned toolchain editor build and real native control/pixel/recovery checks. Existing viewer and probe commands remain unchanged. |
| `editor/local_test.c` | No native I/O fault tests → isolated checks for pinned directory identity, rollback and publication with a cleanup warning. Not linked into the editor. |
| `README.md`, `PROGRESS.md` | Milestone 3 was the next independent development step → editor build/run/workflow instructions and separate host/guest evidence. Full README reviewed; viewer, archive and probe instructions and release gates still apply. |
| `asset/ilbm.c`, `asset/ilbm.h`, `asset/viewer.c`, `tools/asset.py`, `tools/asset_smoke.py`, probe and archive inventory | No changes. The editor links the existing ILBM decoder, uses the same indexed DIB technique and reuses the existing pins, tool extraction and PE checks; the viewer's file-path behavior, size limits and guest evidence still apply only to the viewer. |

**Replacement-only follow-up:** Previously NT replacement wrote project
bytes with whatever permissions the stage inherited. Now every NT
replacement uses `NtQuerySecurityObject` on the held original and empty
stage before writing, and refuses when the access metadata differs or
cannot be read (owner, group, DACL, and on applicable NT versions,
integrity label and resource/scope information). Encrypted originals
are refused rather than copied to a potentially unencrypted stage.
The comparison is intentionally conservative, not an ACL editor, audit
policy migration, or a guarantee against concurrent workspace changes.
First saves still use normal inherited protection.

Previously either platform could publish or roll back to a typed 8.3
alias instead of the prior long name. Now every existing destination
must pass the real-leaf check before the editor moves the original:
NT matches the pinned directory entry and open file ID; 9x checks the
local FAT long name using `FindFirstFileA`. Genuine aliases can still
be read but not used for replacement. A literal `~` name remains valid.
Directory identity, link and sharing checks, no-overwrite backup,
rollback and cleanup behavior are otherwise unchanged. The host driver
and `tools/editor.py test` add scratch checks only; no guest or test
hook is linked into the editor. GUI, model, original format, decoder,
probe and viewer behavior are unchanged by this follow-up.

**Named-stream retention follow-up:** The NT save boundary in
`editor/local.c` previously discarded named data streams with the old
file after publication. Now every NT replacement calls
`NtQueryInformationFile(FileStreamInformation)` on the held original and
proceeds only on a complete result with one unnamed data stream. Extra
streams, unavailable or unsupported queries, overflow and invalid
results refuse before staging. No path is reopened for that check, and
the existing handle, sharing, name, link, DACL, backup and rollback rules
remain. This is not a guarantee against concurrent workspace changes.
Reads, first saves and the 9x FAT path are unchanged.
`editor/local_test.c` and `tools/editor.py test` keep the earlier tests and
add a scratch-only retention check; no test code is linked into the
editor. The GUI, model, original format, decoder, probe and viewer are
unchanged. The full README was reviewed: the save guidance now describes
the refusal; probe, archive, viewer and release instructions still apply.

## Milestone 4 batch 1 — vertical slides (issue #28)

### Source and fidelity

- Rechecked the pinned primary `inputs/vc.zip` hash before source inspection;
  no original source or artwork is added to the repository.
  `vc/veffects.h:18–21` identifies Slide On Up/Down and Slide Off Up/Down
  as IDs 2–5. `neweffects.c::InitVEffectClasses`/`SetVEffectClass`
  register all four as `OC_SCREEN`, start/stop, new-screen effects with
  `a=25` and other defaults zero. The shared duration slider bounds are
  1–255. The active VC10 fields and zero screen-class object reference
  retain the milestone 3 grammar.
- `neweffects.c::CalculateParam` gives `ceil(256/(2*a-1))` rows per field;
  `CalculateLength` separately reserves
  `floor(ceil(256/ceil(256/(2*a)))/2)+1` ticks.
  `mainloop.c:489–516,556–595` queues startup `Bodge2` ahead of the first
  supported cue and dispatches at most one queued effect per field.
  Thus a first slide at tick 0 activates at field 1, not field 0.
  A first cue at a positive tick and later cues still activate at
  `Time*2`; `fx.c::DoDisplay` steps before Func/End for all four slides,
  including the existing Slide On Up (2).
  Preview samples after both logical fields of each 40 ms tick.
- `fx.c::SlideOnDownInit/Func/End`, `SlideOffUpInit/Func/End` and
  `SlideOffDownInit/Func/End` supply the screen swap, split and source-row
  offsets. `videocreator.asm::CopLoopNoSquash` switches at `Start2`;
  `buildscreen` applies each region's `SkipLines`. For a moving frame:

  | Effect | Split row | Top picture / first source row | Bottom picture / first source row |
  | --- | --- | --- | --- |
  | Slide On Down (3) | `Value` | incoming / `256-Value` | outgoing / `Value` |
  | Slide Off Up (4) | `Value` | outgoing / `256-Value` | incoming / `Value` |
  | Slide Off Down (5) | `Value` | incoming / 0 | outgoing / 0 |

  Down values increase from 0; Off Up decreases from 255. At `a=25`,
  a tick-zero slide moves 6, 126 and 246 rows at ticks 0, 10 and 20;
  tick 21 is settled. Its tick-zero split is 6 for IDs 3/5 and 249 for
  IDs 2/4. A slide starting later still moves 12, 132 and 252 rows at
  relative ticks 0, 10 and 20, settling at relative tick 21.
  At `a=65`, a tick-zero slide is still moving at tick 63 (254 rows),
  settling at 64; a later slide settles at relative tick 63.
  At `a=1`, speed 256 reaches End
  before Func: On Down and Off Up select the target, but **Off Down keeps
  the outgoing picture**, since its End only removes screen 2 and its
  first Func never swaps the base. This source quirk is deliberately
  preserved, including later seeks.
- Expectations are source-derived, not original-Amiga captures. The
  independently generated two-plane row cards distinguish all 256 rows
  and use disjoint picture colors. They test logical picture composition,
  not chipset scanout, copper buffering, the depth-dependent blank-line/
  palette tricks in `buildscreen`, physical field timing or timer jitter.
  Paired screens still preview only their first member. No new Windows
  98/XP guest or physical-hardware qualification is claimed; milestone 3
  guest evidence predates this executable.

### Host verification

- Initial implementation at `6310ebe` (before the startup repair below):
  local commands each **passed (exit 0)**, not run by CI:
  `python tools/editor.py build`, `python tools/editor.py test` and
  `python tools/editor.py smoke`. Command logs are
  `.editor/ISSUE28-build-final.txt`, `ISSUE28-test-final.txt` and
  `ISSUE28-smoke-final.txt`; native results (`HOST-SMOKE.txt`) and build
  provenance (`PE-IMPORTS.txt`) are preserved in the primary checkout under
  `inputs/issue-28-6a32881/.editor/issue28-startup-old-6310ebe/`, not alongside
  the repaired build's records.
- Host: Windows 11 Pro x64 `26300.9974`, NTFS, Python 3.14.4 and
  GCC 16.1.0. The build uses the unchanged verified Open Watcom
  `2026-11-01-Build` pin: x86 PE32 GUI 4.0, static WinMain,
  GDI32/KERNEL32/USER32 only and no delay imports. `VCEDIT.EXE` SHA-256:
  `fe8c9496ff9fc3a584964b859608a639bd462615da43ff4add726ac9ad3dacdd`.
- Model checks cover exact LF/CRLF round-trips for all four slides at
  durations 1, 2, 19, 25 and 255, including nondefault `a`–`e`, shifted
  text and metadata. Independent field samples verify split and both
  source offsets. The 27 existing rejection cases remain; unsupported
  ID coverage now uses 6 rather than newly supported 3. Added checks
  reject durations 0/256/65535 and verify failed placements leave the
  project byte-identical, with legal adjacent placement at both endpoints.
- Native smoke authors and restarts each of those 20 cases. Independent
  320×256 painted-frame comparisons cover before, first, intermediate,
  last moving and settled ticks where applicable, with backward seeks,
  exact saved bytes, paired-target imports and all other supported cue
  fields retained. Invalid UI/import durations, unsupported IDs and
  conflicting imports/drags preserve the open sequence. Existing Cut,
  original-artwork Slide On Up, timeline gestures, NT containment and
  save/recovery checks remain in the same smoke.
- Exact Play samples supplement coalescible `WM_TIMER` notifications
  using the existing handler and real `GetTickCount` clock; no test-only
  renderer or runtime option is added. Ordinary, unassisted SetTimer
  playback is also run to completion for every case. An initial smoke
  failed because an ordinary timer skipped requested tick 33. A temporary
  cross-process KillTimer approach also failed and was removed; the
  supplemented-handler sampling avoids assuming guaranteed timer delivery.
- The full README was reviewed: probe, archive, viewer, path/save protection
  and release guidance remains applicable. Current slide controls and the
  source quirk are documented; the older illustration and guest evidence
  are explicitly distinguished. Original inputs, builds and captures stay
  private under ignored local directories.

### Startup timing review repair

- The first implementation counted two slide steps at the end of tick 0.
  The shared `vc_at` timing now subtracts the single startup field only
  for tick-zero slides, covering IDs 2–5 without shifting positive-start
  cues or changing placement lengths. Duration-1 Slide Off Down still
  retains the outgoing picture (startup black for a first cue).
- The independent first-slide regression failed against the preserved
  `6310ebe` executable: native smoke exited 1 at ID 2, duration 2, tick 0.
  The model check also exited 1: split 83 instead of the source-derived
  169. Logs: `.editor/ISSUE28-STARTUP-old-smoke.txt` and `-old-test.txt`.
  The old executable, import report and original smoke results remain in
  `.editor/issue28-startup-old-6310ebe/`. The pinned original-source
  inspection is `.editor/ISSUE28-STARTUP-source.txt`; these are private.
  A separate old-head model probe confirms the wrong tick-zero splits at
  duration 25 and premature settlement at duration 65/tick 63 for every
  ID 2–5: `.editor/ISSUE28-STARTUP-old-boundaries.txt` (eight mismatches).
- First-slide expectations are independent literal field samples at
  durations 1, 2, 19, 25, 65 and 255. Native controls author the first
  slide at tick 0; full 81,920-pixel black/row-card comparisons check
  first, intermediate, last-moving and settled ticks as applicable,
  before/after save and restart, with backward Seek and clock-paced Play.
  Model checks also cover a first cue at tick 1 and a later Cut at 400.
  The existing Cut-then-slide cases now include duration 65.
- Local `python tools/editor.py build`, `python tools/editor.py test`
  and `python tools/editor.py smoke` each **passed (exit 0)** on the same
  Windows 11 host, not CI. Logs are `.editor/ISSUE28-STARTUP-build.txt`,
  `ISSUE28-STARTUP-test.txt` and `ISSUE28-STARTUP-smoke.txt`; full native
  results (`HOST-SMOKE.txt`) and build provenance (`PE-IMPORTS.txt`) for the
  repaired build are preserved in the primary checkout under
  `inputs/issue-28-6a32881/.editor/`. All 24 first-slide and 24 Cut-then-slide
  cases passed, along with the existing artwork, metadata, rejection,
  timeline, containment and save/recovery checks. The PE profile and
  Watcom pin are unchanged. `VCEDIT.EXE` SHA-256:
  `d8a5240a5f0498b3ccb40a352a8217eceae29b49c201a7faf9de483146091a6d`.
- The expanded Play sampling stops before reading the tick used for its
  frame comparison. A skipped sample can restart Play at most twice and
  is logged; a frame mismatch at the requested stopped tick still fails
  immediately. This bounds scheduling/race failures without changing
  the editor clock or claiming jitter qualification. The nonblocking
  struct-assignment/padding snapshot in `model_test.c` was not changed;
  replacing that snapshot with `memcpy` remains a separate test cleanup.
- The full README was reviewed again and remains current: controls,
  commands, source quirk and host/guest limits match the shipped editor.
  No original-source or capture publication, Windows 98/XP guest run,
  physical-hardware test or original-reader qualification was added.
  The next-batch candidate remains Scroll/Reveal, subject to separate approval.

### Timed Play retry diagnostics — issue #30

- Exhausted timed Play sampling now reports the target tick and the three
  observed stop ticks in attempt order on stderr; `None` identifies an
  attempt with no observed stop. Both native sampling call sites use the
  same retry check. A wrong frame at the target still fails immediately;
  a skip followed by a correct frame keeps its retry entry in the smoke log.
- Local `python -m unittest discover -s tools -p test_play_capture.py`
  **passed (3 tests, exit 0)**, not run by CI. Controlled stops at 11/12/13
  targeting tick 10 verify the smoke command's failure diagnostic and exit
  status; a wrong frame at 10 verifies no retry, and 11 then 10 verifies
  successful recovery with the skip retained. No host timing/load is used.
- `python tools/probe.py bootstrap` and `python tools/editor.py build`
  **passed (exit 0)** using the existing SHA-256-verified Watcom cache.
  Smoke fixtures were prepared from the pinned local archive.
  The rebuilt executable is unchanged:
  `d8a5240a5f0498b3ccb40a352a8217eceae29b49c201a7faf9de483146091a6d`.
- `python tools/editor.py smoke` **passed (exit 0)** locally, not in CI,
  on Windows 11 x64 build 26300 with Python 3.14.4. All 24 first-slide and
  24 Cut-then-slide cases passed, along with existing artwork, metadata,
  timeline, containment and save/recovery checks. No timed sample needed
  a retry in this run. Local evidence: `.editor/ISSUE30-unit.txt`,
  `ISSUE30-build.txt`, `ISSUE30-smoke.txt` and `.editor/HOST-SMOKE.txt`.
- The full README was reviewed and remains accurate without changes,
  including setup, controls, save protection and host/guest limits.
  No editor runtime, Windows 98/XP qualification or next-batch scope changed;
  original inputs and generated artifacts remain private and ignored.

### Documentation and test follow-ups — issues #32–#34 (2026-11-20)

- Issue #32 / PR #35 replaced the placement test's struct-assignment
  snapshot with `memcpy`, removing padding dependence from its byte comparison.
  The executor's `python tools/editor.py test` **passed (exit 0)**.
- Issue #33 / PR #36 changed the exhausted-retry fixture to non-monotonic
  stops 13/11/12 targeting tick 10, so sorting cannot satisfy the attempt-order
  assertion. The executor's
  `python -m unittest discover -s tools -p test_play_capture.py`
  **passed (3 tests, exit 0)**. An in-memory sorting mutation failed only
  the order assertion. Both PRs passed independent review; the reviews did
  not rerun tests. Neither check was run by CI.
- Issue #34 corrected the preserved initial issue #28 evidence pointers,
  separately from the startup-repaired build, and added the deterministic
  retry command to README. Locally on Windows with Python 3.14.4,
  `python -m unittest discover -s tools -p test_play_capture.py`
  **passed (3 tests, exit 0)**; `git diff --check HEAD^ HEAD`
  **passed (exit 0)** for this documentation commit. Neither is run by CI.
- The full README was reviewed against the shipped tools and recorded
  evidence: probe, archive, viewer, editor controls, save protection and
  release guidance remain current. The new retry instructions explicitly
  distinguish controlled tests from native smoke and Windows 98/XP
  qualification. Earlier build, native smoke and guest results above remain
  historical; none was rerun for this docs-only change. No private artifacts
  were copied or published. Scroll/Reveal remains the next candidate batch,
  subject to separate approval.

## Milestone 4 batch 2 — Scroll and Reveal (issue #42)

### Source and fidelity

- Reverified primary `inputs/vc.zip` against SHA-256
  `ef4622f337e4e6331fdc97d5da6ec54d35f0d39bcd28ab898ce3689b8fb36ee3`
  before reading the reference. `vc/veffects.h:22–25` assigns Scroll
  Up/Down IDs 6/7 and Reveal Up/Down IDs 8/9.
  `vc/neweffects.c:93–153,1114–1144,1223–1251,1935–1938` supplies the shared
  1–255 duration control, default 25, zero other defaults, screen object
  class, start/stop placement and the same field step and reserved length
  as the four slides. The active VC10 grammar is unchanged.
- `vc/fx.c:358–392,610–731` supplies the four Init/Func/End contracts.
  `videocreator.asm::CopLoopNoSquash` switches pictures at `Start2`;
  `buildscreen` applies the corresponding `SkipLines`. For an active
  frame, `m` is the total stepped rows, with `m < 255`:

  | Effect | Split row | Top picture / first source row | Bottom picture / first source row |
  | --- | --- | --- | --- |
  | Scroll Up (6) | `255-m` | outgoing / `m+1` | incoming / 0 |
  | Scroll Down (7) | `m` | incoming / `256-m` | outgoing / 0 |
  | Reveal Up (8) | `255-m` | outgoing / 0 | incoming / `255-m` |
  | Reveal Down (9) | `m` | incoming / 0 | outgoing / `m` |

- The existing shared timing path remains: field step
  `ceil(256/(2*a-1))`, update before Func/End, and one initial update at
  tick 0 because `mainloop.c:489–516,556–595` dispatches startup `Bodge2`
  first. A first cue at tick 1 and later cues receive two updates per
  tick. Seek samples after both logical fields. At duration 25, a later
  cue steps 12/132/252 rows at relative ticks 0/10/20 and settles at 21;
  a tick-zero cue steps 6/126/246 and also settles at 21.
- At duration 1, Scroll Down and Reveal Down reach End before their
  first Func can swap pictures; like Slide Off Down, they retain the
  outgoing picture, including startup black. Their End resets the row
  offset and removes the second region. The other new effects select
  the target. Held output becomes the base of the following cue.
- These are independently implemented, source-derived logical picture
  expectations, not original-Amiga captures. The existing independently
  generated row cards distinguish all 256 rows and both pictures.
  Chipset scanout, copper buffering, depth-dependent blank-line/palette
  tricks, physical field timing and timer jitter remain unqualified.
  Paired screens still preview only their first member.
- Four named buttons reuse the selected-picture and duration workflow;
  two extra rows extend the client height from 384 to 438 pixels without
  changing the preview size or horizontal timeline mapping.

### Host verification — 2026-11-20

- All four required local commands **passed (exit 0)**, not run by CI:
  `python tools/editor.py test`, `python tools/editor.py build`,
  `python tools/editor.py smoke` and
  `python -m unittest discover -s tools -p test_play_capture.py`
  (**3 tests**). Host: Windows 11 Pro x64 `26300.9974`, NTFS,
  Python 3.14.4 and GCC 16.1.0.
- Build: verified Open Watcom `2026-11-01-Build`, x86 PE32 GUI 4.0,
  static WinMain, direct GDI32/KERNEL32/USER32 imports only, no delay
  imports. The checkout had no Watcom cache; `python tools/probe.py
  bootstrap` downloaded and verified the pinned official asset before use.
  `python tools/asset.py prepare F:\AmigaVid\inputs\vc.zip` verified the
  archive and prepared only the two local smoke fixtures.
  Final `VCEDIT.EXE` SHA-256:
  `4ccbb0be9d544bef909f429e6b18bcd51d3b0b259805552f31b1384ccb56cf76`.
- Model checks cover IDs 2–9 at durations 1/2/19/25/65/255, exact LF/CRLF
  round-trips to canonical LF with nondefault metadata, target, `a`–`e`
  and shifted text, independent split/source-row expectations, first cues
  at ticks 0/1, completion, held output and following Cut/Slide cues.
  Placement failures for invalid durations, unsupported IDs, overlaps
  and timeline bounds leave a byte-identical project; adjacent cues on
  both sides and a cue ending at the timeline limit succeed. The 27
  malformed/unsafe cases and existing native NT save/fault checks pass.
- Native smoke passes **72 first-transition cases** (the existing 24
  slides at tick 0 plus 48 new-effect cases at ticks 0 and 1) and
  **48 Cut-then-transition cases**, all at the six durations above.
  Each authors through the native buttons, checks exact saved VC10 bytes,
  quits, reopens and compares every one of 81,920 painted pixels per
  sampled frame using independent row-card slices. Backward Seek,
  clock-paced Play and ordinary timer-driven completion agree, including
  direction-sensitive intermediate rows and duration-1 downward retention.
  Eight additional sequences place a following Slide immediately adjacent
  to each new effect at duration 1 or 25 and check its Seek/Play output and
  saved fields. Nondefault paired-target imports, default duration 25,
  refused UI/import durations, unsupported IDs and conflicting imports/
  drags preserve the document. Existing artwork, Cut, all slides, marker
  gestures, containment and save/recovery checks also pass.
- No timed Play sample needed a retry in the final smoke. The expanded
  model test failed against the pre-implementation model (exit 1 at
  `vc_place` for newly supported ID 6); `.editor/ISSUE42-test-before.txt`
  retains that result. An initial smoke was deliberately stopped after
  correcting a button mnemonic; only the rebuilt executable's complete
  final smoke is used for the result above.
- Local evidence in this issue worktree: `.editor/ISSUE42-build-final.txt`,
  `ISSUE42-test-final.txt`, `ISSUE42-smoke-final.txt`,
  `ISSUE42-unit-final.txt`, `PE-IMPORTS.txt` and `HOST-SMOKE.txt`.
  Preserve these before removing the worktree. The full README was reviewed
  and updated for all four effects; probe, archive, viewer, save protection
  and release guidance remains applicable. Original code/artwork, binaries
  and captures are not published.
- This executable was exercised only on the modern host. Windows 98 FE/SE
  and x86 XP RTM/SP3 editor qualification, physical target hardware,
  original-Amiga reader/rendering comparison and editor-release acceptance
  remain outstanding; earlier guest evidence predates these effects.

### Whole-field duration review repair — 2026-11-20

- The shared native duration read now requires one to three ASCII digits
  representing 1–255 before placement. It checks the full control length
  before copying. This does not prevent the native edit control's three-character
  input limit from truncating a real paste before validation (see below).
  Cut still ignores the duration field.
- Native smoke sets each of `"2.5"`, `"2x"`, `" 2"`, `"+2"`, `"2 "`, empty,
  `0`, `256`, `"255x"` and 100 digits in the actual Ticks control and confirms
  the text was not truncated before submission. For each of IDs 2–9, a
  placement with room to succeed must show the duration error and save
  byte-identical project data after every refusal: **80 rejection checks**.
- Red evidence: the expanded native check against the preserved `8ad81dd`
  executable exited 1 at `invalid UI duration ' 2' changed saved bytes`.
  The focused run skipped first-transition cases and kept duration 25 via
  in-memory sample selection; no old executable or source was modified.
  `"2.5"` and `"2x"` already refused on this host, so that specific decimal
  failure was not reproduced. The old EXE, import report and smoke results
  remain in `.editor/issue42-review1-old-8ad81dd/`; diagnostics are
  `.editor/ISSUE42-REVIEW1-old-smoke.txt` and `-old-smoke-checked.txt`.
- The adjacent following Slide now targets a visibly distinct card after
  duration-1 Scroll Down and Reveal Down. Its Seek and timed Play comparisons
  therefore detect a skipped following cue instead of accepting an unchanged
  picture. All eight adjacent sequences retain exact saved-field checks.
- All four required local commands **passed (exit 0)**, not run in CI:
  `python tools/editor.py test`, `python tools/editor.py build`,
  `python tools/editor.py smoke` and
  `python -m unittest discover -s tools -p test_play_capture.py` (**3 tests**).
  The full smoke retains all 120 author/reopen cases, eight adjacent
  sequences and existing regression checks; no Play sample needed a retry.
  Host: Windows 11 Pro x64 `26300.9974`, NTFS, Python 3.14.4, GCC 16.1.0.
  The verified Watcom pin and PE/import profile above are unchanged.
  Repaired `VCEDIT.EXE` SHA-256:
  `ce053193ab0454d7f3f66e56bd357822437e9e328fb5b81115cc75e829dcaacb`.
- Logs: `.editor/ISSUE42-REVIEW1-test.txt`, `-build.txt`, `-smoke.txt` and
  `-unit.txt`, plus `.editor/HOST-SMOKE.txt` and `PE-IMPORTS.txt`.
  The full README was reviewed again and remains accurate without changes.
  Original code/artwork and generated artifacts remain private and ignored.
  No Windows 98/XP guest, physical target or original-Amiga reader was run;
  their qualification and editor-release acceptance remain outstanding.

### Native paste review repair blocked — 2026-11-20

- Review cycle 2 identifies a gap in the preceding evidence: smoke uses
  `WM_SETTEXT`, which bypasses the edit limit. Its passing results do not
  establish that a real paste of `1999` is refused rather than accepting
  a truncated `199`. That behavior was not reproduced in this session.
- On Windows 11 Pro x64 `26300.9974`, attempted an isolated window station
  before any clipboard operation. `CreateWindowStationW` with a unique name
  returned `WinError 5` (access denied); named creation requires administrator
  rights. The documented unnamed form with `CWF_CREATE_ONLY` returned
  `WinError 183` (already exists). Reusing that existing station would not
  establish an isolated clipboard, so the check stopped. No clipboard was
  read, emptied, overwritten or logged; no editor paste or saved-byte
  comparison ran. No elevation or destructive clipboard fallback was used.
- Preserved the pre-fix `10c4d2413701383f449fa5dd25c6c9fe73d2f2c1` executable,
  import report and earlier smoke log in
  `.editor/issue42-review2-old-10c4d24/`, plus the unfinished, unexecuted-worker
  harness sources as `attempted-editor_smoke.py` and `attempted-editor.py`.
  EXE SHA-256 remains
  `ce053193ab0454d7f3f66e56bd357822437e9e328fb5b81115cc75e829dcaacb`.
  Isolation failures (both exit 1) are recorded in
  `.editor/ISSUE42-REVIEW2-old-paste.txt` and
  `ISSUE42-REVIEW2-old-paste-isolated.txt`. These are local-only artifacts.
- Removed the unfinished harness from tracked changes; no runtime repair was
  made. The four required checks were not rerun: there is no repaired source
  to verify, and the existing smoke does not exercise native paste. Criterion
  3 remains unsatisfied. The Architect must provide a safely isolated native
  paste environment before reproduction, repair and all four checks resume.
- Reviewed the full README; its commands, effect documentation and explicit
  host/guest qualification limits remain current. No original code/artwork
  was published. Windows 98/XP, physical-target, original-reader and release
  qualification remain outstanding.

### Native paste limit repair

- **2026-11-20, block 1 resolved:** the Architect supplied a disposable
  offline XP SP3 overlay. The Ticks edit now uses `EM_LIMITTEXT(0)` and
  `ES_AUTOHSCROLL`, rather than a short input or visible-width cap. Overlong
  input, including selection replacements, remains invalid for the existing
  whole-field validation. Cut, the 1–255 range and default 25 are unchanged.
- **Observed red:** the preserved `10c4d24` executable
  (`ce053193ab0454d7f3f66e56bd357822437e9e328fb5b81115cc75e829dcaacb`)
  received real `WM_PASTE` inside XP SP3. `1999` became `199`, including
  replacement/insertion within existing text; 100 digits became `222`.
  Each invalid prefix was accepted at tick 25 after a Cut at 0, changing an
  independently encoded 88-byte project to 116 bytes. The completed driver
  reported **40 failures / 104 cases, exit 1**, across IDs 2–9. Leading-space
  ` 2` and decimal `2.5` were rejected on this guest; valid pastes passed.
- **Observed green:** the same guest driver on the repaired executable
  reported **0 failures / 104 cases, exit 0**: 72 refusals left exact baseline
  bytes unchanged; 32 valid pastes (1/25/255 and selection replacement to
  255) authored exact expected cues. `1999` and all 100 digits remained in
  the field rather than becoming valid prefixes. An independent host reader
  rechecked all 104 captured saved projects and the old accepted-199 save.
  `python tools/editor.py paste_test` builds this optional guest-only driver;
  neither it nor the ordinary host checks access the host clipboard.
- Guest: Windows XP Professional SP3 x86 `5.1.2600`, QEMU TCG Pentium III,
  256 MiB, standard VGA (640×480 captures). The supplied writable overlay is
  `C:\Users\Kninetimmy\AppData\Local\Temp\opencode\issue42-xp-8rnd2dof\paste-test.qcow2`.
  Each boot used a fresh private AF_UNIX QMP socket, no display frontend,
  NIC, clipboard sharing or guest agent, and only a dedicated disposable
  FAT16 transfer disk. Both backing files' SHA-256 values remained unchanged;
  the powered-off overlay passed `qemu-img check`. No other VM was controlled.
- Private evidence: `.editor/issue42-xp-paste-20261120/` holds `final-results/`
  (`OLD.LOG` red, `NEW.LOG` green, exit codes and saved projects), the final
  driver/source/import hashes, QMP commands, backing hashes, transfer disks
  and `comparison.json`. `old3-results/` and `green-results/` retain earlier
  completed runs; the final run uses the committed driver's fixed filename
  pairs and workspace-length guard. `old-results/` and `old2-results/`
  preserve preliminary harness runs with dialog-dismissal races; they are
  not the completed 104-case result. The supplied overlay retains guest
  workspaces. Preserve these ignored artifacts before removing the worktree.
- All four required local checks **passed (exit 0)**, not run by CI:
  `python tools/editor.py test`, `python tools/editor.py build`,
  `python tools/editor.py smoke` and
  `python -m unittest discover -s tools -p test_play_capture.py` (**3 tests**).
  Full smoke retained all 120 author/reopen cases, eight adjacent sequences
  and prior regressions. Native selection replacement and character messages
  now cover 112 duration refusals with unchanged saved bytes, plus eight
  valid replacements to 255 with exact saved cues, without any clipboard API.
  No timed Play sample needed a retry.
  The first smoke invocation hit its outer 120-second timeout (exit 143),
  not a reported assertion failure; its log is retained separately.
  Repaired `VCEDIT.EXE` SHA-256:
  `eb3c2d8302e1d560d2fc16082260c212816f65def497e5d3f30632f0d413fc78`.
  Host: Windows 11 Pro x64 `26300.9974`, Python 3.14.4, GCC 16.1.0; the
  verified Watcom `2026-11-01-Build` pin and PE/import profile are unchanged.
- Command logs are `.editor/ISSUE42-REVIEW2-test.txt`, `-build.txt`,
  `-smoke.txt`, `-unit.txt`, `-paste-build-final.txt`,
  `-guest-verify-final.txt` and `-overlay-check.txt`; `.editor/HOST-SMOKE.txt`
  and `PE-IMPORTS.txt` hold the complete native/build results.
- Reviewed the full README and documented the optional guest-only test,
  distinguishing focused XP paste evidence from full guest qualification.
  This does not qualify guest preview, playback timing, layout, Windows 98,
  physical hardware, the original Amiga reader or an editor release. No
  original source, artwork, executable, disk or capture is published.

### Native multiline paste repair

- **2026-11-20, issue #42 criterion 3:** real `WM_PASTE` on the isolated XP
  SP3 guest confirmed the review hypothesis on `317b0f2`. With an eligible
  cue at tick 25 after a Cut, replacing `7` with `25\r\n256` kept `25`;
  replacing the last two digits of `125` with `2\r\n256` kept `12`. Both
  appended a cue, changing the exact 88-byte baseline to 115 bytes. Bare CR
  and trailing CRLF also admitted prefixes; bare LF was already refused on
  this guest. The driver exited **1: 40 failures / 160 cases** across IDs 2–9.
- **Repaired:** Ticks uses a stock multiline EDIT in the same bounds, with
  horizontal and vertical auto-scrolling so neither a line break nor the
  visible size can clip a paste to a valid prefix. The existing whole-field
  check still requires 1–255, default 25. No custom clipboard handler, new
  API imports, timing or placement changes.
- **Observed green:** the identical guest driver exited **0: 0 failures /
  160 cases**. The 128 refusals left exact baseline bytes unchanged,
  including whole-field and partial selection replacement with CRLF, LF
  and CR, trailing CRLF, `1999` and the other single-line negatives. The 32
  positives saved exact cues for 1/25/255 and a selection replacement to
  255. An independent host reader reconstructed and compared all 320 red and
  green projects. The driver built by `python tools/editor.py paste_test`
  repeated the green result. The earlier 104-case evidence above is retained.
- Same disposable Windows XP Professional SP3 x86 `5.1.2600` QEMU overlay
  as above, using separate private QMP sockets and FAT16 transfers, without
  NIC, clipboard sharing, host clipboard access or login secrets. Both
  backing hashes stayed unchanged and `qemu-img check` found no overlay
  errors. Private evidence: `.editor/issue42-xp-multiline-20261120/`
  (`red/`, `green/` and `try2/` contain `final-results/PASTE.LOG`, `EXIT.TXT`
  and the exact `.VID` files; `comparison.json` and `verify.py` independently
  bind and check the saved bytes). `C613.VID` and `C614.VID` are the Scroll Up
  full and partial CRLF replacement witnesses. Preserve these ignored
  artifacts before removing the worktree.
- Windows 11 Pro x64 `26300.9974`, Python 3.14.4, GCC 16.1.0. Open Watcom
  `2026-11-01-Build`, x86 PE32 GUI 4.0, static CRT and identical direct
  imports to the prior executable. Repaired `VCEDIT.EXE` SHA-256:
  `b6a91e997f3a59f7b5032825e1bf2ab416c2b86545e9c41b8f0fd8059999bdf3`.
- All four required local checks passed (exit 0): `python tools/editor.py
  test`, `python tools/editor.py build`, `python tools/editor.py smoke`
  and `python -m unittest discover -s tools -p test_play_capture.py`
  (3 tests). The completed smoke covered 120 author/reopen cases, eight
  adjacent sequences, 112 invalid duration refusals and eight valid
  selection replacements. The first smoke command hit its 240-second
  outer timeout (exit 143) without a reported assertion failure. Logs:
  `.editor/ISSUE42-REVIEW3-test.txt`, `-build.txt`, `-smoke-complete.txt`,
  `-unit.txt`; `-smoke.txt` is the interrupted run. `HOST-SMOKE.txt`
  captures the completed smoke and `PE-IMPORTS.txt` details the build.
- Reviewed the full README and updated the guest-only driver and duration
  instructions. Focused XP paste checks do not qualify guest preview,
  playback timing, 640×480 layout, Windows 98, physical hardware, the
  original Amiga reader or an editor release. No original code or artwork,
  guest disk or capture is published.

## Milestone 4 batch 3 — Peel Up and Peel Down (issue #44)

### Source and fidelity

- Reverified the primary `inputs/vc.zip` against SHA-256
  `ef4622f337e4e6331fdc97d5da6ec54d35f0d39bcd28ab898ce3689b8fb36ee3`.
  `vc/veffects.h:26–27` assigns IDs 10/11;
  `neweffects.c:108–109,1136–1144,1245–1251,1599–1615,1946–1947`
  supplies the duration control, shared length/field step, dispatch and
  defaults (25, other parameters zero), with the same screen object class
  and start/stop placement as the preceding transitions.
- `fx.c:1950–2046` supplies both Init/Func/End contracts. Up decreases
  Value from 255 to 0; Down increases it from 0 to 255 and swaps the two
  pictures on its first Func. Both set reflection to Value, enabling
  screen 2 only when `Value < 128`, at output/source row `2*Value`.
  Up reflects the outgoing picture; Down reflects the incoming picture.
- The active `videocreator.asm::CopLoopNoSquash/CopLoop2:1266–1379`,
  `COPWAIT:825–842` and `buildscreen:1396–1511` determine the row timing,
  not the inactive commented reflection code in the squash loop. The wait
  at horizontal `$01` precedes row fetching. For the 40-byte row cards,
  modulo -80 reverses the next source-row advance: output rows through
  Value use the same source row; subsequent reflected rows use
  `2*Value-y`. The pivot appears once. Screen 2 starts at its skipped row
  and resets the modulo to zero.
- Explicit boundary: at Value 128, rows 0–128 are upright and rows
  129–255 read source 127–1, with no second picture. At Value 127,
  rows 0–127 are upright, rows 128–253 read source 126–1, and rows
  254–255 come from the second picture's rows 254–255.
  With duration 255, tick-zero relative tick 63 has Up=128/Down=127;
  a positive-start relative tick 63 has Up=127/Down=128.
- The existing `ceil(256/(2*a-1))` field step, update-before-Func/End,
  startup field exception, reserved placement length and 1–255 duration
  range are unchanged. Duration-1 Peel Up selects the target immediately;
  **duration-1 Peel Down retains the outgoing picture**, including startup
  black, because End runs before the first Func/swap. Both Ends remove
  reflection and the second region. Following cues start from the held
  output, without reflected rows.
- Two named buttons reuse the selected screen and whole-field duration
  validation. Play/Stop/Seek move down one row and status uses the space
  below the preview; client size, preview size and timeline mapping are
  unchanged. VC10 IDs, target, tick, duration, remaining parameters,
  shifted text and metadata round-trip without changing the grammar.
- These are independently implemented, source-derived logical picture-row
  expectations, not original-Amiga captures. Paired screens still preview
  only their first picture. Chipset scanout, copper buffering, depth-dependent
  blank-line/palette tricks, physical field timing and timer jitter are not
  qualified. No original code or artwork is published.

### Host verification — 2026-11-20

- `python tools/editor.py build` and `python tools/editor.py test` each
  **passed (exit 0)** locally, not in CI. The deterministic retry check,
  `python -m unittest discover -s tools -p test_play_capture.py`,
  **passed (3 tests, exit 0)**.
  Host: Windows 11 Pro x64 `26300.9974`, NTFS, Python 3.14.4, GCC 16.1.0.
- Reused and hash-verified the existing local Open Watcom
  `2026-11-01-Build` cache; no download. Build remains x86 PE32 GUI 4.0,
  static WinMain, direct GDI32/KERNEL32/USER32 only, no delay imports.
  `VCEDIT.EXE` SHA-256:
  `b635eb8f4de09cbec9ba3a473cc92a6c841094a9ec9d31aea35d5eb8134d0684`.
  The pinned local archive supplied only the two prepared smoke fixtures.
- Model checks cover IDs 2–11 at durations 1/2/19/25/65/255, exact LF/CRLF
  round-trips with nondefault metadata, paired target, parameters and shifted
  text; first cues at ticks 0/1; both reflection boundaries and cleanup;
  following Cut/Slide output; invalid durations and atomic placement refusal.
  All 27 existing malformed/unsafe cases and NT save/fault checks pass.
- `python tools/editor.py smoke` **passed (exit 0)** on the complete rerun,
  locally, not in CI: **96 first-transition and 60 Cut-then-transition
  row-card cases**, including 36 Peel cases at the six durations above.
  Native controls author, save, quit, reopen and compare all 81,920 pixels
  at each sampled tick, with backward Seek, clock-paced Play and ordinary
  timer-driven completion. Both sides of 128, exact Values 127/128,
  duration 1 and completion pass for each direction.
  **12 adjacent sequences** include each Peel followed immediately by a
  visibly distinct Slide at durations 1/25, with exact saved fields and
  Seek/Play output free of prior reflection. All ten transitions retain
  paired-target/metadata round-trips, 140 native duration refusals and ten
  valid replacements to 255. Existing Cut, slides, Scroll/Reveal, original
  artwork, marker gestures, containment and save/recovery checks pass.
  No timed Play sample needed a retry.
- The first native smoke command hit its 1,200-second outer timeout
  (exit 143) during Cut-then-Reveal Down, before reaching the Cut-then-Peel
  cases. No assertion failure was reported; it is not a completed smoke
  result. `.editor/ISSUE44-smoke.txt` retains that interrupted log.
- Private evidence in this worktree's `.editor/`: `ISSUE44-build.txt`,
  `ISSUE44-test.txt`, `ISSUE44-smoke-complete.txt`, `ISSUE44-unit.txt`,
  `PE-IMPORTS.txt`, `HOST-SMOKE.txt`, pinned excerpts in `ISSUE44-source.txt`,
  150 `peel-*.rgb` captures (320×256 top-down RGB24), 36 Peel saves and four
  adjacent-cue saves. `python .editor/verify_peel.py` **passed (exit 0)**,
  separately simulating field updates and row-pointer strides without
  importing the port or smoke oracle, and checking all captures and 40
  hand-encoded saves; results are `ISSUE44-capture-review.txt`.
  `ISSUE44-MANIFEST.json` fingerprints the retained evidence.
  Preserve these ignored files before worktree cleanup; none is published.
- The full README was reviewed, including probe, archive, viewer, editor,
  path/save protection and release instructions. It now describes Peel and
  limits the older optional XP paste driver's 160 cases to IDs 2–9; those
  guest clipboard cases were not rerun or extended to Peel. Windows 98/XP
  guest preview, physical hardware, original-reader and editor-release
  qualification remain outstanding; prior guest evidence predates this build.

## Host tooling — per-run editor smoke evidence (issue #46)

- **Before:** editor smoke stored retained RGB captures, copied VID saves
  and `HOST-SMOKE.txt` directly in `.editor/`, the shared build directory
  recorded in the historical runs above. **After:** each invocation that
  starts the native checks announces a fresh `.editor/smoke-*` directory.
  Every retained RGB and VID name is created exclusively there; prior runs
  and legacy names are not reused or updated. Partial evidence stays in its
  own directory. `HOST-SMOKE.txt` is published only after the checks and
  disposable workspace cleanup, not as a shared latest copy.
- The native assertion matrix, separate disposable editor workspace,
  subprocess working directory and temporary junction test target are
  unchanged. `.editor/VCEDIT.EXE`, `.editor/editor.map` and
  `.editor/PE-IMPORTS.txt` retain their build locations. Probe and viewer
  commands and shortcut handling are unchanged.
- `python -m unittest discover -s tools -p "test_*.py"` **passed (4 tests,
  exit 0)**. The isolation regression leaves partial RGB and VID evidence
  on failure, then checks distinct destinations, unchanged earlier bytes
  and two untouched scratch hardlink targets after the next invocation.
  The same test failed against the old runner because the interrupted
  evidence included the legacy success report.
- `python tools/asset.py prepare` and `python tools/editor.py build` both
  **passed (exit 0)** using the pinned archive and verified Open Watcom
  cache on Windows 11 Pro x64 `26300.9974` (NTFS, Python 3.14.4).
  `VCEDIT.EXE` matches the previous native smoke build byte for byte:
  `b635eb8f4de09cbec9ba3a473cc92a6c841094a9ec9d31aea35d5eb8134d0684`.
- **Native host only, 2026-11-20 UTC:** an ignored local driver ran the
  smoke on an unlocked desktop and stopped it between the first two Peel
  cases (expected exit 1 after 7m 15s). `.editor/smoke-3ybzr90o/` retained
  two actual 320×256 RGB24 captures and one byte-checked VID, with no
  completion report. The second invocation, `python tools/editor.py smoke`,
  **passed (exit 0, 23m 30s)**. It covered 96 first-transition cases,
  60 Cut-then-transition cases, 12 adjacent sequences, and the existing
  duration, marker, replay, containment and save/recovery checks, with no
  timed Play retries. `.editor/smoke-n74j2k0w/` retained 150 RGBs, 40 VID
  copies and `HOST-SMOKE.txt` (SHA-256
  `8560bf2b595564c55316b138cc04e0e19091e8a7dc3cad8665f65fcda0e53bd4`).
- The local evidence check confirmed both announced paths, exact file
  inventories, all 150 complete-run capture hashes, and unchanged bytes
  in the interrupted directory after the passing run. No shared evidence,
  disposable directory or editor process was left by either run. Preserve
  the two ignored run directories and `inputs/issue46/` (verification
  helpers, logs and `evidence-manifest.json`) before worktree cleanup.
- Reviewed the full README and documented where to find evidence and how
  to recognize an incomplete run. The required checks ran locally, not in
  CI. No Windows 98/XP guest, physical hardware, original-reader or editor
  release qualification was run; the next milestone is unchanged.

## Editor keyboard shortcuts — issue #48 (2026-11-21 UTC)

- Kept **Alt+U → Slide On Up (2)** and **Alt+D → Slide On Down (3)**.
  Moved only the two Peel button ampersands: **Alt+E → Peel Up (10)**
  and **Alt+A → Peel Down (11)**. Visible wording, layout, other shortcuts,
  effect behavior and VC10 grammar are unchanged.
- Added `python tools/editor.py shortcut_test`, a focused native Windows
  check using generated row cards and the existing independent saved-byte
  expectation. Each fresh editor starts with a Cut and two pictures.
  The check verifies the editor thread's filename focus, posts one
  `WM_SYSCHAR` with lowercase ASCII and the Alt context bit, then queues
  Save behind it. The executable's ordinary `IsDialogMessageA` path
  activates the transition; the test never clicks or sends a command for
  that transition. Exact saved bytes require precisely one added cue with
  the expected ID, target, tick and default duration. Button captions also
  prove each of these four keys is unique and its wording is unchanged.
- No global keyboard injection, clipboard API or foreground-window change
  is used. Deadline-bounded polling waits for observed focus and completed
  Save, rather than assuming a fixed delay. Earlier harness attempts stopped
  at an unnecessary foreground-window precondition; the diagnostic confirmed
  the editor thread already had the correct focus. Those attempts are
  retained separately and are not the completed baseline result.
- **Red:** a clean pre-fix build reproduced SHA-256
  `b635eb8f4de09cbec9ba3a473cc92a6c841094a9ec9d31aea35d5eb8134d0684`.
  The new command **failed (exit 1)**: U/D saved exactly Cut plus IDs 2/3
  but had duplicate button bindings; E/A saved only the original Cut,
  with no Peel cue or binding. Actual and expected files and the completed
  report remain in `.editor/shortcuts-ed8kj41x/`; the executable and build
  provenance are in `.editor/baseline/`.
- **Green:** required local `python tools/editor.py build` and
  `python tools/editor.py shortcut_test` both **passed (exit 0)**, not in
  CI. All **4/4** keys saved exact two-cue projects with IDs `[1, 2]`,
  `[1, 3]`, `[1, 10]` and `[1, 11]`, with unique button bindings.
  Host: Windows 11 x64 `26300.9974`, NTFS, Python 3.14.4.
  Bootstrap downloaded and verified the pinned Open Watcom
  `2026-11-01-Build` asset; the PE32 x86 GUI 4.0/static WinMain/direct
  GDI32/KERNEL32/USER32 profile is unchanged. Fixed `VCEDIT.EXE` SHA-256:
  `f2239d1afd333bf719722e7021418fb291fbd3040ecc3a62888710360802bb2c`.
- `python -m unittest discover -s tools -p "test_*.py" -v`
  **passed (4 tests, exit 0)**. `python -m py_compile tools/editor.py
  tools/editor_shortcuts.py` and `git diff --check` passed. Narrow checks
  rejected an unknown command (exit 2) and mismatched build provenance
  (exit 1, before native exercise/evidence creation). An independent
  literal-byte comparison rechecked all eight baseline/fixed saves; an
  entire-source comparison confirmed only the two C ampersands changed.
- Private evidence: `.editor/ISSUE48-*.txt` command/diagnostic logs,
  `.editor/shortcuts-57qw88vl/` actual/expected saves and `SHORTCUT-TEST.txt`,
  plus `.editor/PE-IMPORTS.txt` and `editor.map`. Preserve these ignored
  artifacts before removing the worktree; no binaries or pictures are
  published. The unrelated full playback matrix was not rerun.
- Reviewed the full README and documented the four keys, focused command,
  evidence and desktop requirements. The remaining probe, archive, viewer,
  editor, save-protection and release guidance remains current. This is
  modern-host keyboard evidence, not Windows 98/XP, physical-hardware,
  original-reader, playback or editor-release qualification. The next
  milestone remains a separately approved small effect family.

## File and playback keyboard shortcuts — issue #50 (2026-11-21 UTC)

- Moved only two production ampersands: **Open p&icture → Alt+I** and
  **See&k → Alt+K**. **Alt+P → Play** and **Alt+S → Save VID** now have
  unique button bindings. All visible words, other captions/mnemonics, layout,
  command handlers and the Ctrl+I/Ctrl+O/Ctrl+S/F5/Esc accelerator table
  remain unchanged.
- Extended the existing `python tools/editor.py shortcut_test`, keeping
  all four Slide/Peel saved-byte cases. Four additional fresh processes
  post queued `WM_SYSCHAR` to the editor thread's observed focused filename
  control, never a direct command or click for the action under test.
  Generated row cards and the existing independent VC10 fixture helper
  supply the inputs; there is no archive, global input or clipboard access.
  Open picture must add the named screen-list entry; Play must report
  Playing and advance the real clock in a sequence whose second cue starts
  at tick 4000; Save must match all 140 expected bytes; Seek must rewrite
  unsubmitted `00025` to `25` while retaining Project opened status and
  both pictures, without Play. Deadlines make absent actions ordinary failed
  cases so the baseline report completes.
- **Red:** `python tools/editor.py build` **passed (exit 0)** on unchanged
  production source, reproducing `VCEDIT.EXE` SHA-256
  `f2239d1afd333bf719722e7021418fb291fbd3040ecc3a62888710360802bb2c`.
  `python tools/editor.py shortcut_test` **failed as expected (exit 1,
  4/8 checks passed)**. U/D/E/A retained their exact two-cue saves.
  I opened nothing; P opened a third picture instead of playing (tick 0);
  S saved the expected VID but had duplicate Save/Seek bindings;
  K left `00025` unsubmitted. Completed report and saves:
  `.editor/shortcuts-9_wkltlw/`. Separate executable, PE report, map,
  source/harness snapshot and command logs: `.editor/red/`.
- **Green:** both required build and shortcut commands **passed (exit 0)**.
  All **8/8** keys had unique bindings and correct results: I opened
  `6: ROWSA.IFF`; P reported Playing at tick 1 with two pictures;
  S saved the exact 140-byte fixture (SHA-256
  `9610010663ed3685875812bae16fdf14c261a061bcacc561a0993ff1bb1ee29c`);
  K submitted tick 25 with unchanged Project opened status and two pictures.
  U/D/E/A still saved exactly IDs `[1, 2]`, `[1, 3]`, `[1, 10]`, `[1, 11]`.
  Completed report and saves: `.editor/shortcuts-3wb9q8kv/`.
  Separate build artifacts and command logs: `.editor/green/`.
  Fixed executable SHA-256:
  `69d3913d1f3476853ab0768fee73f498fae844510bb907446f94081fd4d25008`.
- Required `python -m unittest discover -s tools -p test_*.py -v`
  **passed (4 tests, exit 0)**; `python -m py_compile
  tools/editor_shortcuts.py` and `git diff --check` passed. These checks
  and both native commands ran locally, not in CI.
- Host: Windows 11 x64 build 26300, Python 3.14.4, NTFS. The documented
  bootstrap verified the pinned Open Watcom `2026-11-01-Build` checksum.
  Both builds retain x86 PE32 GUI 4.0, static WinMain startup and direct
  GDI32/KERNEL32/USER32 imports. Preserve ignored reports, saves and
  `.editor/` build/command logs before worktree cleanup; no private assets
  or binaries are committed.
- Reviewed the full README against shipped behavior and recorded evidence;
  updated the four bindings and focused test, with the remaining guidance
  current. This is Windows host keyboard evidence only, not Windows 98/XP,
  physical-hardware, original-reader or editor-release qualification.
  No full playback matrix or guest gate was run. Next: a separately approved
  small milestone 4 effect family.

## Shortcut failure diagnostics — issue #52 (2026-11-21 UTC)

- Shortcut diagnostics now go directly to the fresh run's exclusive
  `SHORTCUT-TEST.txt`, flushed before printing each line. Both editor-close
  paths retain the key and cleanup exception before propagating the error.
  Handled errors mark the run incomplete; an overall summary is written only
  after all eight cases and disposable workspace cleanup finish. The native
  shortcut actions, comparison oracles and process-kill fallback are unchanged.
- **Deterministic red/green:** the new comparison regression failed before
  the repair (exit 1, missing `SHORTCUT-TEST.txt`). With mocked Win32/process
  APIs, the real comparison observes IDs `[1, 3]` instead of `[1, 2]`, then
  close raises `TimeoutExpired`. The repaired command returns 1 and the
  persisted report retains Alt+U, observed/expected IDs, cleanup timeout and
  incomplete status, with no overall summary. A second regression checks
  that workspace cleanup failure prevents a passing summary even when the
  exercise returns zero failures. Neither uses real input, windows or sleeps.
- All three required local checks **passed (exit 0)**, not run by CI:
  `python -m unittest discover -s tools` (**6 tests**),
  `python tools/editor.py build` and `python tools/editor.py shortcut_test`.
  The native run reports **PASS 8/8** with unique, unchanged button wording,
  exact Slide/Peel and Save bytes, the named picture entry, Play at tick 1
  and Seek at tick 25. Input remains queued messages to the test editor's
  PID-checked controls; no global input or clipboard API was added.
- Host: Windows 11 x64 build 26300, NTFS, Python 3.14.4. Bootstrap downloaded
  and verified the pinned Open Watcom `2026-11-01-Build` asset. The rebuilt
  x86 PE32 GUI 4.0/static WinMain/direct GDI32/KERNEL32/USER32 executable is
  byte-identical to issue #50:
  `69d3913d1f3476853ab0768fee73f498fae844510bb907446f94081fd4d25008`.
  Python compilation and `git diff --check` also passed.
- Preserve this worktree's ignored `.editor/shortcuts-sqpsdzx_/` (13 VID
  evidence files and `SHORTCUT-TEST.txt`), `.editor/controlled-failure-v59mtagr/`
  (mocked failure report and actual/expected saves, not a native editor run),
  `.editor/ISSUE52-*.txt` command logs, `PE-IMPORTS.txt` and `editor.map` before
  worktree cleanup. The passing report SHA-256 is
  `0d2b766c36aadc0c57b404ee501ed07804fed14f7e3dc98ae37bd262d51931e0`.
  All generated evidence and binaries remain private and untracked.
- Reviewed the full README; corrected the focus claim to explain that normal
  editor launches can take foreground focus, and documented partial/failure
  reports and the deterministic checks. Other probe, archive, viewer, editor,
  save-protection and release guidance remains current. This is Windows host
  evidence only: no full playback matrix, Windows 98/XP guest, physical
  hardware, original-reader or release qualification was run. The next
  milestone remains a separately approved small effect family.

## Milestone 4 batch 4 — Squeeze On Up and Down

- The pinned archive was reverified before inspecting `vc/veffects.h:28–29`
  and `vc/neweffects.c:1122–1144,1231–1251,1503–1518,1939–1940`.
  IDs 12/13 use the shared screen class, placement length, field step and
  duration range 1–255 (default 25). VC10 preserves their timing, screen,
  parameters and text. IDs above 13 are still refused.
- `vc/fx.c:775–857` sets Up's split and outgoing height to `Value`, with
  incoming row 0 at the split. Down sets the split to `Value`, starts the
  incoming picture at source row `256 - Value`, and squashes the outgoing
  picture into the bottom `256 - Value` rows. Both End routines select the
  target and remove the temporary row state, even when duration 1 reaches
  End before the first Func. A first cue at tick 0 still gets one field
  update at that tick; later starts get two.
- `vc/videocreator.asm:1033–1052,1114–1217` divides 32768 by the outgoing
  region height, truncates that quotient, and accumulates it with seven
  fractional bits. The squashed region fetches source row 0 before its
  first advance. At a positive start with duration 25 the heights are 243 (Up)
  and 244 (Down). Outgoing region row 19 (Up) reads source row 19, not 20;
  region row 21 (Down, output row 33) reads source row 21, not 22.
  These are logical row selections, not chipset scanout or field-timing
  measurements on the original hardware.
- The new Tab-reachable buttons use the existing screen and duration
  controls. Their positions below the preview leave playback, timeline
  geometry and existing mnemonics unchanged.

### Host verification — 2026-11-21 UTC

- `python tools/editor.py build` and `python tools/editor.py test` passed
  (exit 0) on Windows 11 Pro x64 `26300.9974`, NTFS, Python 3.14.4 and
  GCC 16.1.0. The build reverified the pinned Open Watcom cache, with no
  download. x86 PE32 GUI 4.0, static WinMain, direct GDI32/KERNEL32/USER32,
  no delay imports. `VCEDIT.EXE` SHA-256:
  `9676cb7a7330f2a51ba3787d294570ed676a5119a98074e85242711c280ab022`.
- Model checks pass for all supported IDs, including LF/CRLF round-trips,
  preserved nondefault parameters, placement and field timing, both
  Squeeze On increments and adjacent-cue reset. All 27 malformed/unsafe
  cases and existing NT save protection checks pass.
- `python tools/editor.py smoke` passed (exit 0): 120 first-cue cases
  and 72 Cut-then-transition cases, including 36 Squeeze On cases at
  durations 1/2/19/25/65/255. First cues start at tick 0 or 1; cues after
  Cut start at 25. Seek and timed Play matched all 81,920 pixels at each
  sampled tick, with no Play retries. Of 18 adjacent sequences, six
  followed Squeeze On at durations 1/25/255 and showed no residual
  scaling or offset. Refused opens (including ID 14) left the preview
  and saved bytes unchanged. Earlier native regression cases passed.
- `python tools/editor.py shortcut_test` passed (exit 0, 8/8).
  A separate native check confirmed both buttons are enabled, reachable
  in Tab order and hit-testable without overlapping the preview or other
  visible controls. All six Python unit tests passed.
- An independent check matched 264 retained Squeeze On RGB captures and
  80 exact VC10 saves, including the split boundaries and non-divisor
  rounding samples in both Seek and Play. The ignored `.editor/`
  directory holds `smoke-bql6vcav/`, `shortcuts-fz2ddr7h/`, the build
  and command logs, and `ISSUE54-manifest.json` with 563 verified artifact
  hashes. Preserve that evidence before worktree cleanup; no original
  material or native captures are published.
- Reviewed the full README and documented the new controls, behavior and
  scope of verification. All four required checks ran locally, not in CI.
  No Windows 98/XP guest, physical hardware, original-reader or release
  qualification was performed for this build.

## Open blockers

- The [five missing files](#recovered-and-missing-files) still need locating;
  the two compiled payloads are recovered, though standalone originals remain
  welcome for provenance. Recovered retail projects, fonts, vector filenames
  and payloads do not resolve the separate reader/fidelity checks.
- Bundled third-party code and media need separate redistribution review,
  including recovered retail content and the Commodore-Amiga notice in
  `vc/modules/screen.c`.
- Retail data/audio tracks are inventoried above, but developer source-disc,
  CD-transport/device behavior and full-motion playback evidence remain open.
  Physical Windows 98 and XP hardware validation is still pending; the
  passing target and true zero-output-device tests above are QEMU runs.
- The kit is for local testing. Review Open Watcom runtime redistribution
  requirements before publishing a binary; no binaries are tracked.

## Next milestone

Before milestone 2, real-asset ILBM decoding and display was the next
independent development step; the viewer now has host and four-guest
evidence under #16's calibrated-output criterion. Before milestone 3,
the next step was to edit, save, reopen and replay one sequence. The
two-picture workflow is now implemented, followed by the vertical slide,
Scroll Up/Down (6/7), Reveal Up/Down (8/9), Peel Up/Down (10/11) and
Squeeze On Up/Down (12/13) families above. The rest of milestone 4 and
special screens remain outstanding; choose the next small effect family
through a separately approved plan. Obtain physical Windows 98 and XP
[probe target-checklist](probe/TEST.TXT) results before claiming native
hardware validation. Editor-release workflows remain unverified.
