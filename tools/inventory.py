"""Print a deterministic inventory of the pinned Video Creator ZIP."""

import argparse
import hashlib
from pathlib import Path, PurePosixPath
import sys
import zipfile


PINNED_SHA256 = "ef4622f337e4e6331fdc97d5da6ec54d35f0d39bcd28ab898ce3689b8fb36ee3"
DEFAULT_ARCHIVE = Path("inputs/vc.zip")

SOURCE_SUFFIXES = {".asm", ".c", ".h", ".i"}
ASSET_PREFIXES = (
    "vc/3dobjects/",
    "vc/data/",
    "vc/quadanims/",
    "vc/samples/",
    "vc/vcpics/",
    "vc/vecanims/",
    "vc/vectors/",
    "vc/videos/",
)
ASSET_SUFFIXES = {".3d", ".iff", ".qa", ".tn", ".vec", ".vcdir"}
REFERENCE_BINARY_SUFFIXES = {".o", ".obj"}
REFERENCE_BINARIES = {"vc/editor", "vc/editordb"}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as archive:
        for block in iter(lambda: archive.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def classify(name):
    lower_name = name.casefold()
    suffix = PurePosixPath(lower_name).suffix

    if lower_name == "vc/makefile" or suffix in SOURCE_SUFFIXES:
        return "SOURCE"
    if lower_name.startswith(ASSET_PREFIXES) or suffix in ASSET_SUFFIXES:
        return "ASSET"
    if suffix in REFERENCE_BINARY_SUFFIXES or lower_name in REFERENCE_BINARIES:
        return "REFERENCE BINARY"
    return "OTHER"


def inventory(path):
    with zipfile.ZipFile(path) as archive:
        entries = sorted(
            archive.infolist(),
            key=lambda entry: (entry.filename.casefold(), entry.filename),
        )

    directories = [entry for entry in entries if entry.is_dir()]
    files = [entry for entry in entries if not entry.is_dir()]
    sections = {label: [] for label in ("SOURCE", "ASSET", "REFERENCE BINARY", "OTHER")}

    for entry in files:
        sections[classify(entry.filename)].append(
            f"{entry.filename}\t{entry.file_size}\t{entry.CRC:08x}"
        )

    lines = [
        "VIDEO CREATOR ARCHIVE INVENTORY",
        f"SHA-256\t{PINNED_SHA256}",
        f"FILES\t{len(files)}",
        f"DIRECTORIES\t{len(directories)}",
        "",
        "DIRECTORIES",
        *(entry.filename for entry in directories),
    ]
    for label in ("SOURCE", "ASSET", "REFERENCE BINARY", "OTHER"):
        lines.extend(("", label, *sections[label]))
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Inventory the pinned Video Creator archive without extraction."
    )
    parser.add_argument("archive", nargs="?", type=Path, default=DEFAULT_ARCHIVE)
    args = parser.parse_args(argv)

    if not args.archive.is_file():
        print(f"inventory: archive is missing: {args.archive}", file=sys.stderr)
        return 2

    actual_sha256 = sha256(args.archive)
    if actual_sha256 != PINNED_SHA256:
        print(
            "inventory: SHA-256 mismatch for "
            f"{args.archive}: expected {PINNED_SHA256}, got {actual_sha256}",
            file=sys.stderr,
        )
        return 3

    try:
        output = inventory(args.archive)
    except (OSError, zipfile.BadZipFile) as error:
        print(f"inventory: cannot read verified archive {args.archive}: {error}", file=sys.stderr)
        return 4

    sys.stdout.write(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
