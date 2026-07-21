#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MEDIA_SUFFIXES = {
    ".gb", ".gbc", ".gba", ".sav", ".ss0", ".ss1", ".ss2", ".png",
    ".gif", ".jpg", ".jpeg", ".webp", ".bmp", ".wav", ".flac", ".ogg",
    ".mp3", ".wasm",
}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp"}
EXPRESSIVE_MARKERS = (
    b"original concept, design and program by",
    b"licensed to nintendo",
    b"copyright 1989 bullet-proof",
    b"copyright 1989 nintendo",
)
PRIVATE_RUNTIME_MARKERS = (
    b"native-gb-tetris-re",
    b"GBRE Scenario",
    b"gbre_live",
    b"mGBA",
)


def git_lines(*arguments):
    result = subprocess.run(
        ["git", "-C", str(ROOT), *arguments], check=True,
        capture_output=True, text=True,
    )
    return result.stdout.splitlines()


def has_git_history():
    result = subprocess.run(
        ["git", "-C", str(ROOT), "rev-parse", "--is-inside-work-tree"],
        check=False, capture_output=True, text=True,
    )
    return result.returncode == 0 and result.stdout.strip() == "true"


def fail(message):
    raise SystemExit(message)


def publication_files():
    if has_git_history():
        return git_lines("ls-files", "--cached", "--others", "--exclude-standard")
    return [
        path.relative_to(ROOT).as_posix()
        for path in ROOT.rglob("*")
        if path.is_file()
    ]


def source_names(names):
    return [
        name for name in names
        if name.startswith(("src/", "tests/", "tools/"))
        and Path(name).suffix.lower() in SOURCE_SUFFIXES
    ]


def reject_expression(data, name):
    lowered = data.lower()
    for marker in EXPRESSIVE_MARKERS:
        if marker in lowered:
            fail(f"cartridge-authored presentation text remains in {name}: {marker.decode()}")


def audit_current_tree():
    files = publication_files()
    for name in files:
        if Path(name).suffix.lower() in MEDIA_SUFFIXES:
            fail(f"publication tree contains cartridge, extracted media, or generated output: {name}")
    for name in source_names(files):
        reject_expression((ROOT / name).read_bytes(), name)


def audit_history():
    if not has_git_history():
        return

    source_objects = {}
    for record in git_lines("rev-list", "--objects", "--all"):
        fields = record.split(" ", 1)
        if len(fields) != 2:
            continue
        object_id, name = fields
        if Path(name).suffix.lower() in MEDIA_SUFFIXES:
            fail(f"cartridge or extracted media remains in Git history: {name}")
        if name in source_names([name]):
            source_objects.setdefault(object_id, name)

    for object_id, name in source_objects.items():
        result = subprocess.run(
            ["git", "-C", str(ROOT), "cat-file", "blob", object_id],
            check=True, capture_output=True,
        )
        reject_expression(result.stdout, f"Git history ({name}, blob {object_id[:12]})")


def cartridge_windows(rom, size=32):
    return {
        rom[offset:offset + size]: offset
        for offset in range(0, len(rom) - size + 1)
        if len(set(rom[offset:offset + size])) >= 8
    }


def audit_artifact(path, rom):
    data = path.read_bytes()
    lowered = data.lower()
    for marker in (*EXPRESSIVE_MARKERS, *PRIVATE_RUNTIME_MARKERS):
        if marker.lower() in lowered:
            fail(f"{path} contains a private or cartridge-authored marker: {marker.decode()}")

    if rom is None:
        return
    windows = cartridge_windows(rom)
    for offset in range(0, len(data) - 31):
        candidate = data[offset:offset + 32]
        cartridge_offset = windows.get(candidate)
        if cartridge_offset is not None:
            fail(
                f"{path} contains 32 consecutive cartridge bytes from "
                f"ROM offset 0x{cartridge_offset:04x}"
            )


def main():
    parser = argparse.ArgumentParser(description="Audit the public Tetris Modern boundary")
    parser.add_argument("--artifact", action="append", type=Path, default=[])
    parser.add_argument("--rom", type=Path)
    parser.add_argument(
        "--current-only", action="store_true",
        help="audit the publication snapshot without examining pre-rewrite Git history",
    )
    arguments = parser.parse_args()

    audit_current_tree()
    if not arguments.current_only:
        audit_history()
    rom = arguments.rom.read_bytes() if arguments.rom else None
    for artifact in arguments.artifact:
        if not artifact.is_file():
            fail(f"distribution artifact does not exist: {artifact}")
        audit_artifact(artifact, rom)
    print("Tetris Modern distribution boundary audit passed")


if __name__ == "__main__":
    main()
