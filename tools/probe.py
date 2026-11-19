"""Bootstrap, build and verify the native Win32 compatibility probe."""

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
from urllib.request import urlopen
import zipfile

from inventory import sha256


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / ".probe"
TAG = "2026-11-01-Build"
ASSET = "open-watcom-2_0-c-win-x64.exe"
URL = f"https://github.com/open-watcom/open-watcom-v2/releases/download/{TAG}/{ASSET}"
SHA256 = "d4e496c593ef2acbc54d1eaeb31b31d9c62f86e39a09e61ada411ef34feaa01a"
SIZE = 128145854
INSTALLER = WORK / ASSET
BUILD = WORK / "build"
EXE = "VCPROBE.EXE"
DATA = "VCPROBE.DAT"
KIT = WORK / "VCPROBE.ZIP"
SUM = KIT.with_suffix(".ZIP.sha256")
TOOLS = ("wcc386.exe", "wccd386.dll", "wlink.exe", "wlinkd.dll", "wlink.lnk",
         "wdump.exe", "wdis.exe")
DLLS = {"GDI32.DLL", "KERNEL32.DLL", "USER32.DLL", "WINMM.DLL"}
COMPILE = ("-bt=nt", "-bg", "-3r", "-os", "-w4", "-we", "-zld", "-fo=probe.obj", "probe.c")
LINK = """system nt_win
runtime windows=4.0
option osversion=4.0, map=probe.map, verbose
name VCPROBE.EXE
file probe.obj
library winmm
"""
PICTURE = b"AVP1" + bytes(int(index) for index in (
    "00033111"
    "00033111"
    "00033111"
    "33333333"
    "33333333"
    "22233000"
    "22233000"
    "22233000"
))


def verify_installer(path):
    # Hash the same bytes the extractor will see, even if the cache is replaced.
    with path.open("rb") as stream:
        data = stream.read(SIZE + 1)
    actual = hashlib.sha256(data).hexdigest()
    if len(data) != SIZE or actual != SHA256:
        raise ValueError(
            f"rejected {path.name}: expected {SIZE} bytes and SHA-256 {SHA256}, "
            f"read {len(data)} bytes with SHA-256 {actual}. "
            "Remove the rejected file before downloading again."
        )
    return data


def bootstrap():
    if not INSTALLER.exists():
        WORK.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=WORK) as temp:
            download = Path(temp) / ASSET
            with urlopen(URL, timeout=60) as response, download.open("wb") as out:
                remaining = SIZE + 1
                while remaining and (block := response.read(min(remaining, 1024 * 1024))):
                    out.write(block)
                    remaining -= len(block)
            verify_installer(download)
            download.rename(INSTALLER)
    else:
        verify_installer(INSTALLER)
    print(f"Verified {INSTALLER} ({SIZE} bytes, SHA-256 {SHA256})")


def extract_tools(root):
    # The official SFX contains an ordinary ZIP. Never run the installer.
    tools = {f"binnt64/{tool}" for tool in TOOLS}
    with zipfile.ZipFile(io.BytesIO(verify_installer(INSTALLER))) as archive:
        for entry in archive.infolist():
            name = entry.filename
            if not (name in tools or name.startswith(("h/", "lib386/"))):
                continue
            parts = name.split("/")
            if any(not part or part in (".", "..") or "\\" in part or ":" in part
                   or os.path.isreserved(part)
                   for part in parts):
                raise ValueError(f"unsafe member path in verified toolchain: {name}")
            dest = root.joinpath(*parts).resolve()
            if not dest.is_relative_to(root):
                raise ValueError(f"toolchain member outside extraction root: {name}")
            dest.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(entry) as src, dest.open("xb") as out:
                shutil.copyfileobj(src, out)


def run(tool, *args, cwd, env):
    result = subprocess.run([str(tool), *args], cwd=cwd, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors="replace", timeout=120)
    if result.returncode:
        raise ValueError(f"{tool.name} exit {result.returncode}:\n{result.stdout}")
    return result.stdout


def pe_headers(image):
    """Read only the PE32 fields needed for the probe's release profile."""
    if len(image) < 64 or image[:2] != b"MZ":
        raise ValueError("no DOS/PE header in linked executable")
    start, = struct.unpack_from("<I", image, 0x3C)
    opt = start + 24
    if (start < 64 or opt + 224 > len(image) or image[start:start + 4] != b"PE\0\0"
            or struct.unpack_from("<H", image, opt - 4)[0] != 224):
        raise ValueError("unexpected PE header in linked executable")
    machine, = struct.unpack_from("<H", image, start + 4)
    magic, = struct.unpack_from("<H", image, opt)
    os_ver = struct.unpack_from("<HH", image, opt + 40)
    sub_ver = struct.unpack_from("<HH", image, opt + 48)
    checksum, subsystem = struct.unpack_from("<IH", image, opt + 64)
    if (machine, magic, os_ver, sub_ver, subsystem) != (0x14C, 0x10B, (4, 0), (4, 0), 2):
        raise ValueError("expected x86 PE32 Windows GUI with OS/subsystem versions 4.0")
    if any(image[opt + 200:opt + 208]):  # delay import directory, entry 13
        raise ValueError("unexpected delay imports")
    return start, opt, checksum


def build():
    if not INSTALLER.is_file():
        raise ValueError("run 'python tools/probe.py bootstrap' first")
    with tempfile.TemporaryDirectory(dir=WORK) as temp:
        scratch = Path(temp)
        watcom = scratch / "watcom"
        extract_tools(watcom)
        bin_dir = watcom / "binnt64"
        system = Path(os.environ["SystemRoot"])
        # Do not inherit global Watcom, compiler, linker or library overrides.
        env = {
            "SystemRoot": str(system),
            "PATH": f"{bin_dir};{system / 'System32'}",
            "WATCOM": str(watcom),
            "INCLUDE": f"{watcom / 'h'};{watcom / 'h' / 'nt'}",
            "TMP": str(scratch),
            "TEMP": str(scratch),
        }
        source = ROOT / "probe" / "probe.c"
        # Use a stable input name and line endings in the private build folder.
        (scratch / "probe.c").write_bytes(source.read_bytes().replace(b"\r\n", b"\n"))
        (scratch / "probe.lnk").write_bytes(LINK.encode("ascii"))
        commands = (
            ("wcc386.exe", *COMPILE),
            ("wlink.exe", "@probe.lnk"),
        )
        logs = [run(bin_dir / tool, *args, cwd=scratch, env=env)
                for tool, *args in commands]

        path = scratch / EXE
        image = bytearray(path.read_bytes())
        pe, opt, checksum = pe_headers(image)
        # WLINK stamps the wall clock. Normalize just the COFF timestamp;
        # no signed, checksummed or debug-directory images are handled here.
        if checksum or any(image[opt + 128:opt + 136]) or any(image[opt + 144:opt + 152]):
            raise ValueError("cannot normalize checksummed, signed or debug PE")
        struct.pack_into("<I", image, pe + 8, 0)
        path.write_bytes(image)
        dump = run(bin_dir / "wdump.exe", EXE, cwd=scratch, env=env)
        run(bin_dir / "wdis.exe", "-l", "probe.obj", cwd=scratch, env=env)

        BUILD.mkdir(exist_ok=True)
        (BUILD / DATA).write_bytes(PICTURE)
        for name in (EXE, "probe.lnk", "probe.map", "probe.lst"):
            shutil.copyfile(scratch / name, BUILD / name)
        (BUILD / "wdump.txt").write_text(dump, encoding="utf-8")
        provenance = [
            URL, f"GitHub release asset ID 697737741; {SIZE} bytes; SHA-256 {SHA256}",
            "Host: x64 Open Watcom binaries from the verified SFX ZIP",
            f"Python: {sys.version.split()[0]}",
            "Compiler: " + " ".join(commands[0]),
            "Linker: " + " ".join(commands[1]) + "\n" + LINK.rstrip(),
            "\n" + "\n".join(log.strip() for log in logs),
            "\nSHA-256",
        ]
        for name in (*[f"binnt64/{tool}" for tool in TOOLS], "h/nt/windows.h",
                     "h/nt/mmsystem.h", "lib386/nt/clib3r.lib",
                     *[f"lib386/nt/{lib}.lib" for lib in ("kernel32", "user32", "gdi32", "winmm")]):
            provenance.append(f"{sha256(watcom / name)}  {name}")
        for path in (scratch / "probe.c", BUILD / EXE, BUILD / DATA):
            provenance.append(f"{sha256(path)}  {path.name}")
        (BUILD / "provenance.txt").write_text("\n".join(provenance) + "\n", encoding="utf-8")
    print(f"{sha256(BUILD / EXE)}  {BUILD / EXE}")


def built_image():
    path = BUILD / EXE
    image = path.read_bytes()
    pe_headers(image)
    provenance = (BUILD / "provenance.txt").read_text(encoding="utf-8")
    if f"{sha256(path)}  {EXE}\n" not in provenance:
        raise ValueError("build provenance does not match executable; rebuild first")
    return image, provenance


def inspect():
    _, provenance = built_image()
    dump = (BUILD / "wdump.txt").read_text(encoding="utf-8")
    imports = re.findall(r"^\s*DLL name = <([^>]+)>", dump, re.MULTILINE)
    if set(imports) != DLLS or not re.search(r"\bwaveOutGetNumDevs$", dump, re.MULTILINE):
        raise ValueError(f"unexpected direct imports: {imports}")
    link_map = (BUILD / "probe.map").read_text(encoding="utf-8")
    startup = re.search(r"^cstrtwwt\s+_TEXT.*$", link_map, re.MULTILINE)
    if not startup or "__WinMain" not in link_map or "clib3r.lib(cstrtwwt)" not in link_map:
        raise ValueError("expected Open Watcom static WinMain startup missing from link map")
    report = (
        "AMIGAVID WIN32 PROBE — PE32, i386 (0x014c), GUI (2), OS/subsystem 4.0\n"
        f"\n{provenance}"
        "\nStartup: static clib3r.lib(cstrtwwt) -> __WinMain -> ANSI WinMain.\n"
        "Link map: " + startup[0] + "\n"
        "Direct DLLs: " + ", ".join(imports) + ". No delay imports or CRT DLL.\n"
        "The CRT contains wide imports and runtime lookups even though the app uses ANSI.\n"
        "Pending: stock 98/XP loader, system DLL exports/forwarders and transitive\n"
        "dependencies, CRT fallback paths and real legacy CPU verification.\n"
        "\nFull direct imports and PE fields from Watcom WDUMP follow.\n"
        "See probe.map for the startup entry point and the selected library members;\n"
        "probe.lst disassembles the app object, not the linked CRT. See PROGRESS.md\n"
        "for startup/import findings and for CPU, stock DLL and target loader limits.\n\n"
        + dump
    )
    dest = BUILD / "PE-IMPORTS.txt"
    dest.write_text(report, encoding="utf-8")
    print(f"{sha256(dest)}  {dest}")


def package():
    image, _ = built_image()
    data = (BUILD / DATA).read_bytes()
    if data != PICTURE:
        raise ValueError("built data does not match the expected picture; rebuild first")
    instructions = (ROOT / "probe" / "TEST.TXT").read_bytes().replace(b"\r\n", b"\n")
    # Stored, fixed DOS timestamps, 8.3 member names; no modern ZIP features.
    with zipfile.ZipFile(KIT, "w", allowZip64=False) as archive:
        for name, contents in ((EXE, image), (DATA, data),
                               ("TEST.TXT", instructions.replace(b"\n", b"\r\n"))):
            archive.writestr(zipfile.ZipInfo(name), contents)
    SUM.write_text(f"{sha256(KIT)}  {KIT.name}\n", encoding="ascii")
    print(SUM.read_text().strip())
    print(KIT)


def smoke():
    from probe_smoke import exercise

    checksum = SUM.read_text(encoding="ascii").split()
    if checksum != [sha256(KIT), KIT.name]:
        raise ValueError("ZIP does not match its checksum sidecar")
    with tempfile.TemporaryDirectory(prefix="Probe Test ", dir=WORK) as folder:
        extracted = Path(folder)
        with zipfile.ZipFile(KIT) as archive:
            if archive.namelist() != [EXE, DATA, "TEST.TXT"]:
                raise ValueError("unexpected members in the probe ZIP")
            archive.extractall(extracted)
        if sha256(extracted / EXE) != sha256(BUILD / EXE):
            raise ValueError("packaged executable does not match the build")
        exercise(extracted / EXE, extracted / DATA, ROOT, WORK / "HOST-SMOKE.txt", checksum[0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = {fn.__name__: fn for fn in (bootstrap, build, inspect, package, smoke)}
    parser.add_argument("command", choices=commands)
    args = parser.parse_args()
    try:
        if sys.platform != "win32":
            raise ValueError("use a modern x64 Windows host")
        commands[args.command]()
    except (OSError, ValueError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print(f"probe: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
