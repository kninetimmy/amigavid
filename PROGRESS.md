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

## Milestone 1 — Open Watcom Win32 probe

Before milestone 1 the next step was to qualify the Open Watcom probe;
milestone 0 itself made no build, runtime, or target-OS test claim.
Now the probe builds and passes a modern-host smoke test. Stock target
qualification remains open. [Build and test commands](README.md#build-and-test-the-win32-probe).

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
  were source-inspected, not exercised. Verify the loader, the actual
  stock DLL exports and their transitive requirements on each target.
  Windows 11's WOW64 and API-set dependencies are not evidence for 98/XP.

### Runtime evidence

| Target | Status and evidence |
| --- | --- |
| Windows 11 Pro x64 26300.9974 (development host) | **Pass**: Ryzen 7 5700X3D, 64 GiB, Radeon RX 9070 XT, 3840×2160×32, WinMM reported three wave outputs; no optical drive enumerated. |
| Windows 98 First Edition | Pending — stock loader, Explorer/`COMMAND.COM`, DIB, input and audio-device checks. |
| Windows 98 Second Edition | Pending — same checks. |
| Windows XP RTM (x86) | Pending — same checks. |
| Windows XP SP3 (x86) | Pending — same checks. |

`.probe/HOST-SMOKE.txt`: launched the extracted EXE with a different
working directory. Read red, green, blue, red and white samples from the
actual painted DIB. Posted mouse and Space messages changed the caption;
Esc and `WM_CLOSE` each exited 0. A missing companion displayed an error
and exited 2. Empty data, trailing byte, bad magic and out-of-range
palette index each displayed an invalid-data error and exited 3.
Restoring the companion allowed relaunch and exit 0. Separately
`.probe/EXPLORER-SMOKE.txt` records Windows 11 shell open from a directory
with spaces and exit 0. No physical keystrokes or `COMMAND.COM` run were
claimed. The EXE always queries `waveOutGetNumDevs` and never opens an
audio device. `/noaudio` exercised the shared no-audio status and clean
exit with `simulated` and the true count of 3 in the caption; no real
zero-output-device run was available.

| Artifact in the local worktree | SHA-256 |
| --- | --- |
| `.probe/VCPROBE.ZIP` (sidecar `.probe/VCPROBE.ZIP.sha256`) | `a317af6fe0277a8eb9895bc24d204efeb389d84ce7a636ad15d1073b3c1b3760` |
| `.probe/build/VCPROBE.EXE` | `0c26c1b8e7582507e1a83b8bd257bfc29b12f62013c6b994cc16bc568461f050` |
| `.probe/build/VCPROBE.DAT` | `cc41816184f41d388d76455fca98f88c48137c850ec073c153d231186bb40fe4` |
| `.probe/build/PE-IMPORTS.txt` | `3c7cc60661e27fa583dad64e0250940efde5b0b8ca78aadfb6060552243e797b` |

## Open blockers

- `DT_STUFF.S`, vector tables/media, demo video media, and font resources are
  absent from the archive.
- Bundled third-party code and media need separate redistribution review,
  including the Commodore-Amiga notice in `vc/modules/screen.c`.
- No source-disc, CD/full-motion media, or target hardware evidence is
  available yet. No stock target or true zero-output-device test was run.
- The kit is for local testing. Review Open Watcom runtime redistribution
  requirements before publishing a binary; no binaries are tracked.

## Next milestone

Run the [target checklist](probe/TEST.TXT) with the same checksummed kit
on stock 98 FE/SE and x86 XP RTM/SP3, including a real zero-output-device
configuration. Start with 98 FE to check the CRT's wide imports and loader
behavior before claiming the toolchain is qualified. Independent milestone
2 ILBM decoding work can proceed without the target PCs.
