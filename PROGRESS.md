# Progress

## Milestone 0 — archive baseline

- Historical baseline: [docs/BASELINE.md](docs/BASELINE.md); its pre-probe
  compatibility matrix is not the current target status.
- Local reference input verified: `vc.zip` is 21,193,157 bytes with SHA-256
  `ef4622f337e4e6331fdc97d5da6ec54d35f0d39bcd28ab898ce3689b8fb36ee3`.
  Its ZIP CRC check found no bad member; the central directory has 1,225 files
  and 38 directory records.
- `python tools/inventory.py` verifies the pin before listing the archive
  without extraction. The archive remains under ignored `inputs/`; no original
  source, assets, or binaries are tracked here.

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
  when `MainTime >= Time*2` and calls `fx.c::DoDisplay` once per field.
  VCEDIT uses 40 ms timeline ticks; deterministic Seek samples the logical
  output after two field updates per tick. It does not reproduce Amiga
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

**Shared-marker selection (issue #20, 2026-11-20):**

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

**Ordinary-drag and captured-open review correction (issue #20):**

- The previous nearest-hit rule changed ordinary drags on distinct marker
  pixels. Clicks and drags now retain the base's first hit in timeline order
  unless the five-pixel hit window itself contains co-located cues. Only
  then does nearest-pixel disambiguation apply; distant shared groups cannot
  change the target. Existing shared-marker cycles and neighbor cases pass.
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

### Guest verification

**First implementation (before review fixes):**

QEMU 11.1.0 (`v11.1.0-12130-ge470268ff4`), offline, no added CRT, NIC or
sound card. Each guest used a fresh qcow2 overlay of the corresponding
Milestone 2 baseline. On both, `A:\SETUP.BAT` copied `VCEDIT.EXE`,
`BG.IFF` and `BLUE.16` to `C:\M3EDIT`; `FC /B` found no executable
differences from the read-only transfer media. Both guests ran
`C:\M3EDIT\VCEDIT.EXE C:\M3EDIT`, using the executable SHA-256 above.
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

## Open blockers

- `DT_STUFF.S`, vector tables/media, demo video media, and font resources are
  absent from the archive.
- Bundled third-party code and media need separate redistribution review,
  including the Commodore-Amiga notice in `vc/modules/screen.c`.
- No original source-disc or CD/full-motion media evidence is available yet.
  Physical Windows 98 and XP hardware validation is still pending; the
  passing target and true zero-output-device tests above are QEMU runs.
- The kit is for local testing. Review Open Watcom runtime redistribution
  requirements before publishing a binary; no binaries are tracked.

## Next milestone

Before milestone 2, real-asset ILBM decoding and display was the next
independent development step; the viewer now has host and four-guest
evidence under #16's calibrated-output criterion. Before milestone 3,
the next step was to edit, save, reopen and replay one sequence. The
two-picture workflow is now implemented; the next planned phase covers
the remaining effects and special screens. Obtain physical Windows 98 and XP
[probe target-checklist](probe/TEST.TXT) results before claiming native
hardware validation. Editor-release workflows remain unverified.
