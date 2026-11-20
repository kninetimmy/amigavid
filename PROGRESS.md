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

Before this viewer, milestone 2 real-asset ILBM decoding and display was
the next independent development step. It now has host and four-guest
evidence under #16's calibrated-output criterion. Milestone 3 is to edit, save,
reopen and replay one sequence. Obtain physical Windows 98 and XP
[probe target-checklist](probe/TEST.TXT) results before claiming native
hardware validation. Editor-release workflows remain unverified.
