"""Prepare pinned local artwork, build and verify the native ILBM viewer."""

import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

from inventory import PINNED_SHA256, sha256
from probe import INSTALLER, ROOT, SHA256 as TOOL_SHA256, TAG, extract_tools, pe_headers, run


WORK = ROOT / ".asset"
FIXTURES = ROOT / "inputs" / "Viewer Assets"
EXE = "VCASSET.EXE"
FIXTURE_PINS = (
    ("vc/data/blankeditor.iff",
     "bd76f69411a1b4eca339edc1651226d12d3c76ee76172d0d1950d2959480c86a",
     "268d851d1941790a3019c94c3a3b8319259246680808b969b8d768923fa3b9bf"),
    ("vc/vcpics/jolyons/artimages/blueface.16",
     "6d772380c998d98a923eecbbce4b6ee5bf157df661a43ff6a55ffbd1c60867ba",
     "622a27248ed4f36102cd514712c0e1a83667cb114b1afa9c153b33cced55e585"),
)
NAMES = tuple(member.rsplit("/", 1)[1] for member, _, _ in FIXTURE_PINS)
ARCHIVE_SIZE = 21193157
COMPILE = ("-bt=nt", "-bg", "-3r", "-os", "-w4", "-we", "-zld")
LINK = f"""system nt_win
runtime windows=4.0
option osversion=4.0, map=viewer.map, verbose
name {EXE}
file viewer.obj, ilbm.obj
"""


def local_dir(path):
    # Output directories must be actual checkout-local paths, not redirected.
    if path.resolve() != path:
        raise ValueError(f"refusing redirected output directory: {path}")
    path.mkdir(parents=True, exist_ok=True)
    return path


def prepare(archive):
    # Hash and extract the same snapshot; never extract paths from ZIP metadata.
    with archive.open("rb") as stream:
        payload = stream.read(ARCHIVE_SIZE + 1)
    if len(payload) != ARCHIVE_SIZE or hashlib.sha256(payload).hexdigest() != PINNED_SHA256:
        raise ValueError(f"{archive} does not match the pinned vc.zip")
    with zipfile.ZipFile(io.BytesIO(payload)) as source:
        destination = local_dir(FIXTURES)
        for (member, input_digest, _), name in zip(FIXTURE_PINS, NAMES):
            data = source.read(member)
            if hashlib.sha256(data).hexdigest() != input_digest:
                raise ValueError(f"unexpected fixture bytes for {member}")
            path = destination / name
            try:
                with path.open("xb") as out:
                    out.write(data)
            except FileExistsError:
                if path.resolve() != path or not path.is_file() or path.stat().st_size != len(data):
                    raise ValueError(f"refusing to overwrite a different fixture: {path}") from None
                if path.read_bytes() != data:
                    raise ValueError(f"refusing to overwrite a different fixture: {path}") from None
            print(f"{input_digest}  {path}")


def copy_fixture(name, digest, destination):
    path = FIXTURES / name
    if path.resolve() != path or not path.is_file():
        raise ValueError(f"missing or redirected fixture: {path} (run prepare first)")
    with path.open("rb") as stream:
        data = stream.read(4 * 1024 * 1024 + 1)
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError(f"fixture does not match the pinned member: {path}")
    target = destination / name
    target.write_bytes(data)
    return target


def build():
    if not INSTALLER.is_file():
        raise ValueError("run 'python tools/probe.py bootstrap' first")
    output = local_dir(WORK)
    with tempfile.TemporaryDirectory(dir=output) as folder:
        scratch = Path(folder)
        watcom = scratch / "watcom"
        extract_tools(watcom)
        bin_dir = watcom / "binnt64"
        system = Path(os.environ["SystemRoot"])
        env = {
            "SystemRoot": str(system),
            "PATH": f"{bin_dir};{system / 'System32'}",
            "WATCOM": str(watcom),
            "INCLUDE": f"{watcom / 'h'};{watcom / 'h' / 'nt'}",
            "TMP": str(scratch),
            "TEMP": str(scratch),
        }
        for name in ("ilbm.h", "ilbm.c", "viewer.c"):
            (scratch / name).write_bytes((ROOT / "asset" / name).read_bytes().replace(b"\r\n", b"\n"))
        (scratch / "viewer.lnk").write_bytes(LINK.encode("ascii"))
        commands = [("wcc386.exe", *COMPILE, f"-fo={unit}.obj", f"{unit}.c")
                    for unit in ("viewer", "ilbm")]
        commands.append(("wlink.exe", "@viewer.lnk"))
        logs = [run(bin_dir / tool, *args, cwd=scratch, env=env) for tool, *args in commands]

        exe = scratch / EXE
        image = bytearray(exe.read_bytes())
        pe, opt, checksum = pe_headers(image)
        if checksum or any(image[opt + 128:opt + 136]) or any(image[opt + 144:opt + 152]):
            raise ValueError("cannot normalize checksummed, signed or debug PE")
        struct.pack_into("<I", image, pe + 8, 0)
        exe.write_bytes(image)
        dump = run(bin_dir / "wdump.exe", EXE, cwd=scratch, env=env)
        dlls = re.findall(r"^\s*DLL name = <([^>]+)>", dump, re.MULTILINE)
        if set(dlls) != {"GDI32.DLL", "KERNEL32.DLL", "USER32.DLL"}:
            raise ValueError(f"unexpected direct imports: {dlls}")
        if "clib3r.lib(cstrtwwt)" not in (scratch / "viewer.map").read_text():
            raise ValueError("expected Open Watcom static WinMain startup")
        hashes = [f"{sha256(scratch / name)}  {name}" for name in
                  ("viewer.c", "ilbm.c", "ilbm.h", EXE)]
        tool_hashes = [f"{sha256(watcom / path)}  {path}" for path in (
            "binnt64/wcc386.exe", "binnt64/wccd386.dll", "binnt64/wlink.exe",
            "binnt64/wlinkd.dll", "lib386/nt/clib3r.lib", "lib386/nt/kernel32.lib",
            "lib386/nt/user32.lib", "lib386/nt/gdi32.lib",
        )]
        report = [
            "VCASSET: x86 PE32 GUI, OS/subsystem 4.0, static WinMain startup, no delay imports.",
            f"Open Watcom v2 {TAG}, installer SHA-256 {TOOL_SHA256}",
            "Direct DLLs: " + ", ".join(dlls),
            *(" ".join(command) for command in commands), LINK, *hashes, *tool_hashes, *logs, dump,
        ]
        (scratch / "PE-IMPORTS.txt").write_text("\n".join(report), encoding="utf-8")
        for name in (EXE, "viewer.map", "PE-IMPORTS.txt"):
            (scratch / name).replace(output / name)
        print(hashes[-1] + " (x86 PE32 GUI, OS/subsystem 4.0; " + ", ".join(dlls) + ")")


def tiny_cases():
    def chunk(name, data):
        return name + struct.pack(">I", len(data)) + data + b"\0" * (len(data) % 2)

    def form(chunks):
        return chunk(b"FORM", b"ILBM" + chunks)

    # Width 3 requires two bytes per plane row, even though only one is visible.
    # High bits are leftmost; plane 0 contributes the low palette-index bit.
    red, green, blue, yellow = (bytes.fromhex(s) for s in ("ff0000", "00ff00", "0000ff", "ffff00"))
    palette = chunk(b"ANNO", b"x") + chunk(b"CMAP", red + green + blue + yellow)
    expected = green + blue + yellow + blue + red + green

    def header(width, compression):
        return chunk(b"BMHD", struct.pack(">HHhhBBBBHBBhh",
                                          width, 2, 0, 0, 2, 0, compression, 0, 0, 1, 1, 3, 2))

    def picture(body, compression, width=3):
        return form(header(width, compression) + palette + chunk(b"BODY", body))

    raw = bytes.fromhex("a0ff60ff20ff80ff")
    # The first row begins with a -128 no-op and a two-byte repeat (including
    # invisible padding); the other three plane rows use literals.
    packed = bytes.fromhex("80ffa00160ff0120ff0180ff")
    yield "raw word-padded plane rows", picture(raw, 0), expected
    yield "ByteRun1 repeat, literals and no-op", picture(packed, 1), expected
    yield "ByteRun1 counted alignment byte", picture(packed + b"\x80\0", 1), expected
    yield "truncated FORM", picture(raw, 0)[:-1], "FORM length"
    yield "truncated chunk header", form(header(3, 0) + b"CM"), "chunk header"
    yield "truncated chunk data", form(header(3, 0) + b"BODY\0\0\0\x04xx"), "Truncated IFF chunk"
    missing_pad = b"ILBM" + b"ANNO\0\0\0\x01x"
    yield "missing odd chunk pad", b"FORM" + struct.pack(">I", len(missing_pad)) + missing_pad, "pad byte"
    yield "short raw BODY", picture(raw[:-2], 0), "BODY plane row"
    yield "literal crosses plane row", picture(b"\x02abc" + packed, 1), "BODY plane row"
    yield "repeat crosses plane row", picture(b"\xfea" + packed, 1), "BODY plane row"
    yield "truncated literal", picture(b"\x01a", 1), "BODY plane row"
    yield "truncated repeat", picture(b"\xff", 1), "BODY plane row"
    yield "excess BODY data", picture(raw + b"\0", 0), "BODY plane row"
    yield "excess compressed BODY data", picture(packed + b"\0", 1), "BODY plane row"
    yield "excessive dimensions", picture(raw, 0, width=65535), "dimensions"
    yield "palette index outside CMAP", form(header(3, 0) + chunk(b"CMAP", red) +
                                            chunk(b"BODY", raw)), "Palette index"


def test():
    compiler = shutil.which(os.environ.get("CC", "gcc"))
    if compiler is None:
        raise ValueError("put a host C compiler (GCC or Clang) on PATH, or set CC to its executable")
    with tempfile.TemporaryDirectory(dir=local_dir(WORK)) as folder:
        scratch = Path(folder)
        driver = scratch / "decode_test.exe"
        rgb = scratch / "output.rgb"
        source = ROOT / "asset"
        run(Path(compiler), "-std=c90", "-pedantic", "-Wall", "-Wextra", "-Werror",
            str(source / "ilbm.c"), str(source / "decode_test.c"), "-o", str(driver),
            cwd=scratch, env=None)

        def decode(path):
            return subprocess.run([str(driver), str(path), str(rgb)], capture_output=True,
                                  text=True, cwd=scratch, timeout=10)

        for (_, input_digest, digest), name in zip(FIXTURE_PINS, NAMES):
            result = decode(copy_fixture(name, input_digest, scratch))
            if result.returncode or result.stdout.strip() != "320 256":
                raise ValueError(f"{name}: expected 320x256 decode; {result.stderr.strip()} (run prepare first)")
            data = rgb.read_bytes()
            actual = hashlib.sha256(data).hexdigest()
            if len(data) != 245760 or actual != digest:
                raise ValueError(f"{name}: got {len(data)} RGB bytes, SHA-256 {actual}")
            print(f"PASS {name}: 320x256, {len(data)} bytes, top-down RGB24 SHA-256 {actual}")

        fixture = scratch / "small.16"
        for name, data, expected in tiny_cases():
            fixture.write_bytes(data)
            result = decode(fixture)
            if isinstance(expected, bytes):
                ok = result.returncode == 0 and result.stdout.strip() == "3 2" and rgb.read_bytes() == expected
            else:
                ok = result.returncode == 1 and expected in result.stderr
            if not ok:
                raise ValueError(f"{name}: unexpected decode result {result.returncode}: {result.stderr.strip()}")
            print(f"PASS {name}")


def smoke():
    from asset_smoke import exercise

    work = local_dir(WORK)
    exe = work / EXE
    digest = sha256(exe)
    pe_headers(exe.read_bytes())
    if f"{digest}  {EXE}\n" not in (work / "PE-IMPORTS.txt").read_text(encoding="utf-8"):
        raise ValueError("viewer provenance does not match executable; build first")
    with tempfile.TemporaryDirectory(dir=work) as folder:
        scratch = Path(folder)
        art = scratch / "Asset Root With Spaces"
        elsewhere = scratch / "Different Working Directory"
        art.mkdir()
        elsewhere.mkdir()
        for (_, input_digest, _), name in zip(FIXTURE_PINS, NAMES):
            copy_fixture(name, input_digest, art)
        exercise(exe, digest, art, elsewhere, NAMES, tuple(pin[2] for pin in FIXTURE_PINS), scratch)
        for name in ("HOST-SMOKE.txt", "background.bmp", "picture.bmp"):
            (scratch / name).replace(work / name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = {fn.__name__: fn for fn in (prepare, build, test, smoke)}
    parser.add_argument("command", choices=commands)
    parser.add_argument("archive", nargs="?", type=Path, help="local pinned ZIP for prepare only")
    args = parser.parse_args()
    if args.archive is not None and args.command != "prepare":
        parser.error("archive argument is only valid for prepare")
    try:
        if args.command == "prepare":
            prepare(args.archive or ROOT / "inputs" / "vc.zip")
        else:
            if sys.platform != "win32":
                raise ValueError("use a modern x64 Windows host")
            commands[args.command]()
    except (OSError, ValueError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print(f"asset: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
