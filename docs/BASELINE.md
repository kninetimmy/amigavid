# Archived Video Creator baseline

This is a map of the original archive as a local reference. It records archive
paths and observed behavior without adding original source, assets, objects, or
executables to this repository.

## Verified input and provenance

The pinned source is
[Archive.org's `vc.zip`](https://archive.org/download/vc_20221206/vc.zip).
The local baseline input was 21,193,157 bytes with SHA-256
`ef4622f337e4e6331fdc97d5da6ec54d35f0d39bcd28ab898ce3689b8fb36ee3`.
Its ZIP check returned no bad CRC, and its central directory contains 1,225
files and 38 directory records. `tools/inventory.py` verifies that SHA-256
before reading ZIP metadata and lists paths, sizes, and CRC values without
extracting members.

The [Archive.org item metadata](https://archive.org/metadata/vc_20221206)
attributes the item to Almathera Systems Ltd and declares CC BY 4.0. That is
the archive's attribution and declared license; it is not a blanket clearance
to redistribute every bundled item. In particular,
`vc/modules/screen.c` has a separate Commodore-Amiga notice: executables based
on that information are limited to Commodore Amiga software and all other
rights are reserved. Third-party code, image, font, audio, and media rights
need separate clearance. The port must use independently implemented Win32
display code and include only cleared material in a release.

## Link-input baseline

`vc/makefile` has separate `Editor` and `Editordb` link lines, but both pass
the same 18 source-backed objects, `dt_stuff.o`, and four external link
inputs to `slink`. `Editor` alone adds `stripdebug`; `smallcode` and
`smalldata` are linker options, not inputs.

| Link input in both targets | Classification | Archive reference | Planned native responsibility |
| --- | --- | --- | --- |
| `sound.obj` | Source-backed | `vc/sound.asm` | Preserve sample-event behavior in portable code; put PCM-device output at the Win32 boundary. |
| `main.o` | Source-backed | `vc/main.c` | Split editor workflow and asset-catalog rules into portable code; replace its Amiga window and input calls with Win32 UI. |
| `psycho.o` | Source-backed | `vc/psycho.c` | Preserve Psycho Flicker screen/palette choices in the portable renderer. |
| `VEffects.o` | Source-backed | `vc/veffects.c` | Preserve the effect, screen, palette, and thumbnail model in portable code. |
| `Block.o` | Source-backed | `vc/block.c` | Preserve timeline block copy, move, and delete rules in portable code. |
| `neweffects.o` | Source-backed | `vc/neweffects.c` | Use `InitVEffectClasses` as the portable effect registration/defaults reference. |
| `audio.o` | Source-backed | `vc/audio.c` | Preserve CD tick and cue semantics; use a Win32 CD/MCI or test-transport adapter. |
| `screens.o` | Source-backed | `vc/screens.c` | Preserve screen and asset-selection rules; present pixels through the Win32 UI boundary. |
| `SebsGads.o` | Source-backed | `vc/sebsgads.c` | Replace the custom gadget and pointer implementation with Win32 controls and input handling. |
| `keith.o` | Source-backed | `vc/keith.c` | Implement the active `LoadVideo`/`SaveVideo` VC10 reader and writer as a portable parser and serializer. |
| `jolyon.o` | Source-backed | `vc/jolyon.c` | Preserve catalog, palette, and Random Raves behavior in portable code. |
| `mainloop.o` | Source-backed | `vc/mainloop.c` | Port VecAnim, QuadAnim, and 3D object loading into portable parsers. |
| `3dvectorcode.obj` | Source-backed | `vc/3dvectorcode.asm` | Reimplement 3D fixed-point, clipping, and raster behavior in portable C; render via a Win32 DIB. |
| `quadanim.obj` | Source-backed | `vc/quadanim.asm` | Reimplement QuadAnim decoding and rendering in portable C. |
| `fx.o` | Source-backed | `vc/fx.c` | Preserve effect execution, text, and sample contracts in portable code; route device operations to adapters. |
| `text.obj` | Source-backed | `vc/text.asm` | Reimplement special text-screen behavior in portable C after font evidence is available. |
| `vectorcode.obj` | Source-backed | `vc/vectorcode.asm` | Reimplement 2D vector animation behavior in portable C. |
| `videocreator.obj` | Source-backed | `vc/videocreator.asm` | Replace Amiga display, DMA, interrupt, and chipset work with the Win32 display boundary. |
| `dt_stuff.o` | Object-only | `vc/dt_stuff.o`; embedded strings name `DT_STUFF.S`, `_GetDTPik`, and `_Remap256to128` | Recreate image loading and 256-to-128 palette/index remapping from observable contracts; do not treat the object as portable source. |
| `lib:c.o` | External dependency | SAS/C C startup object; not in the archive | Use the selected Win32 toolchain's startup/runtime. |
| `lib:sc.lib` | External dependency | SAS/C library; not in the archive | Replace with the selected toolchain's C runtime. |
| `lib:scm.lib` | External dependency | SAS/C companion library; not in the archive | Use portable C or the selected toolchain's equivalent where behavior requires it. |
| `lib:amiga.lib` | External dependency | Amiga OS library; not in the archive | Define narrow Win32 adapters for the OS and device calls. |

The manual covers QuadAnims, text, 2D/3D vectors, Random Raves, full-motion
video, and loading/saving. `neweffects.c::InitVEffectClasses` supplies effect
dispatch/defaults, while `keith.c::LoadVideo` and `SaveVideo` are the active
VC10 format evidence. The older `vc/docs/saveformat` and `vc/defvid` are
separate historical references, not proof of the active grammar.

`main.c` directly includes `Languages.c`; `languages.o` exists in the archive
but is not on either link line and must not become a separately compiled port
module. There are no case-colliding archive paths, although the makefile's
mixed-case names such as `Keith.c`, `Block.c`, `SebsGads.c`, and `VEffects.c`
refer to lowercase archive paths. Treat archive spellings as canonical.

Historical duplicates need review rather than automatic selection:

- `vc/ilbmr.c` and `vc/modules/ilbmr.c` are byte-identical.
- `vc/3dobjects/saucer1.3d` and `vc/data/saucer1.3d` are byte-identical.
- `vc/docs/jolyon.c` is distinct from linked `vc/jolyon.c`; `vc/newjolyon.c`,
  `.old`, `.bak`, and unlinked object files are archival alternatives or
  outputs, not build inputs.

## Surviving reference material

| Surviving material | Reference use only |
| --- | --- |
| `vc/docs/videocreator.manual` and the effect documentation | Manual workflows and effect names; compare behavior rather than copy text or assets into a release. |
| `vc/data/blankeditor.iff` | First ILBM decode fixture identified by the PRD. |
| `vc/data/4pointstretch.vec`, `vc/data/vcspin.qa`, and `vc/3dobjects/bluepyramid.3d` | Initial 2D vector, QuadAnim, and 3D parser/renderer fixtures. |
| `vc/3dobjects/` (49 files), `vc/quadanims/` (111 files), and `vc/vcpics/` (765 files) | Local format and catalog references, subject to individual content-rights review. |
| `vc/samples/` (108 IFF files) and `vc/data/` catalogs | Local audio and catalog references; reachability and redistribution rights remain unverified. |
| `vc/defvid` | Candidate older VC10-like file; validate separately from the active writer. |

## Gaps and unblocked work

| Gap | Archive evidence | Exact resolving evidence | Work that can proceed now |
| --- | --- | --- | --- |
| Datatype helper source | `dt_stuff.o` names `DT_STUFF.S`, `_GetDTPik`, and `_Remap256to128`; no matching source is present. | The original `DT_STUFF.S`, a provenance-preserving source export, or independently captured input/output behavior plus a documented object analysis. | Native image-import and palette-remap contracts, with synthetic fixtures, without copying the object. |
| Vector tables and include | `vc/vectors/` is empty; `3dvectorcode.asm` includes `vc:vectors/TEST` and `SineTable.bin`, and `text.asm` includes `vc:vectors/Macros.i`. | The original `vc/vectors/` files with their byte hashes and format/scale evidence from a working reference. | Portable vector and 3D parser/rendering interfaces, clipping, and non-table-dependent tests. |
| VecAnim media | `vc/vecanims/` is empty, although `vc/data/vecanims.data` survives. | A matching `vc/vecanims/` collection from an original disc or a catalog-to-file capture from a working reference. | Catalog handling and loaders; 3D and QuadAnim work can use surviving fixtures. |
| Demo and project media | `vc/videos/` is empty; `keith.c` tries `vcvideos:1_demovideo.vid`, and the ZIP contains no `.vid` file. | The original `1_demovideo.vid` or another labeled original VC10 file, plus a reference-reader result. | Active VC10 parser/writer and synthetic regression fixtures. |
| Fonts and unfinished sample/font support | Source names `VCThin.font`, `VideoCreator.font`, and `VCSansSerif.font`; no matching font files are archived. `sample-font.todo` also records unfinished support. | Original font resources and their licenses, or captured metrics/screenshots plus cleared replacement-font decisions. | Text layout API and explicitly provisional fallback-font rendering. |
| CD/full-motion media and Amiga/SAS-C libraries | The manual describes CD/full-motion workflow; the link line refers to SAS/C and Amiga libraries outside the ZIP. | A lawful source disc/media capture, target-device behavior capture, and documented SDK/library behavior where needed. | Portable timeline semantics, no-drive behavior, and the planned Win32 probe. |

## Compatibility matrix

No Win32 executable, compatibility probe, or runtime test was attempted for
this baseline.

| Target | Status | Evidence needed |
| --- | --- | --- |
| Windows 98 First Edition | Pending | Open Watcom Win32 probe and target-hardware result. |
| Windows 98 Second Edition | Pending | Open Watcom Win32 probe and target-hardware result. |
| Windows XP RTM | Pending | Open Watcom Win32 probe and target-hardware result. |
| Windows XP SP3 | Pending | Open Watcom Win32 probe and target-hardware result. |
