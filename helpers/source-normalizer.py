#!/usr/bin/env python3
"""Normalize repo text files to .editorconfig and report their current encodings."""

from __future__ import annotations

import argparse
import os
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

try:
    import editorconfig
    from charset_normalizer import from_bytes
except ImportError as exc:
    raise SystemExit(
        "Missing dependency. Install with: pip install editorconfig charset-normalizer"
    ) from exc


UTF8_BOM = b"\xef\xbb\xbf"
UTF16LE_BOM = b"\xff\xfe"
UTF16BE_BOM = b"\xfe\xff"

SIMPLE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".inl",
    ".rc",
    ".rc2",
    ".idl",
    ".def",
    ".sln",
    ".vcxproj",
    ".props",
    ".targets",
    ".txt",
    ".ini",
    ".ps1",
    ".py",
    ".md",
    ".yml",
    ".yaml",
    ".json",
}
COMPOUND_SUFFIXES = (".vcxproj.filters",)


@dataclass
class FileInspection:
    """Stores the current on-disk encoding view for a file."""

    label: str
    text: str | None
    decoder: str | None
    has_bom: bool


@dataclass
class NormalizationOutcome:
    """Stores normalization results for one file."""

    relative_path: str
    encoding_label: str
    target_charset: str
    changed: bool
    written: bool


def parse_args() -> argparse.Namespace:
    """Parse command line arguments for audit and normalization modes."""

    parser = argparse.ArgumentParser(
        description="Normalize repo text files to .editorconfig and report encodings."
    )
    parser.add_argument(
        "--root",
        default=".",
        help="Root directory to scan. Defaults to the current directory.",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="Rewrite files to match .editorconfig instead of reporting only.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit with code 1 if any file would change or if any file fails.",
    )
    parser.add_argument(
        "--report-encodings",
        action="store_true",
        help="Print a detailed encoding breakdown without rewriting files.",
    )
    return parser.parse_args()


def matches_target_file(path: Path) -> bool:
    """Return True when the path is one of the tracked text file families."""

    lower_name = path.name.lower()
    if lower_name.endswith(COMPOUND_SUFFIXES):
        return True
    return path.suffix.lower() in SIMPLE_SUFFIXES


def iter_target_files(root_path: Path) -> list[Path]:
    """Collect target files under the scan root in a stable order."""

    return sorted(
        path
        for path in root_path.rglob("*")
        if path.is_file() and matches_target_file(path)
    )


def get_editorconfig_properties(path: Path) -> dict[str, str]:
    """Return EditorConfig properties for a file path."""

    return editorconfig.get_properties(os.path.abspath(path))


def detect_line_ending(text: str) -> str:
    """Infer the dominant current line ending for fallback use."""

    if "\r\n" in text:
        return "crlf"
    if "\n" in text:
        return "lf"
    if "\r" in text:
        return "crlf"
    return "lf"


def inspect_file_bytes(data: bytes) -> FileInspection:
    """Classify a file's current encoding and decode it when possible."""

    if not data:
        return FileInspection(label="empty", text="", decoder="utf-8", has_bom=False)

    if data.startswith(UTF8_BOM):
        return FileInspection(
            label="utf-8-bom",
            text=data.decode("utf-8-sig"),
            decoder="utf-8-sig",
            has_bom=True,
        )

    if data.startswith(UTF16LE_BOM):
        return FileInspection(
            label="utf-16le-bom",
            text=data.decode("utf-16"),
            decoder="utf-16",
            has_bom=True,
        )

    if data.startswith(UTF16BE_BOM):
        return FileInspection(
            label="utf-16be-bom",
            text=data.decode("utf-16"),
            decoder="utf-16",
            has_bom=True,
        )

    try:
        return FileInspection(
            label="utf-8",
            text=data.decode("utf-8"),
            decoder="utf-8",
            has_bom=False,
        )
    except UnicodeDecodeError:
        result = from_bytes(data).best()
        if result is None or not result.encoding:
            return FileInspection(
                label="legacy:undetected",
                text=None,
                decoder=None,
                has_bom=False,
            )

        encoding = result.encoding.lower().replace("_", "-")
        try:
            decoded_text = data.decode(result.encoding)
        except (LookupError, UnicodeDecodeError):
            return FileInspection(
                label=f"legacy:{encoding}",
                text=None,
                decoder=result.encoding,
                has_bom=bool(result.bom),
            )

        label = f"legacy:{encoding}"
        if encoding == "utf-8":
            label = "utf-8-bom" if result.bom else "utf-8"
        return FileInspection(
            label=label,
            text=decoded_text,
            decoder=result.encoding,
            has_bom=bool(result.bom),
        )


def normalize_text_content(
    text: str,
    trim_trailing_whitespace: bool,
    insert_final_newline: bool,
    end_of_line: str,
) -> str:
    """Apply whitespace, final newline, and EOL normalization to decoded text."""

    normalized = text.replace("\r\n", "\n").replace("\r", "\n")

    if trim_trailing_whitespace:
        normalized = "\n".join(line.rstrip(" \t") for line in normalized.split("\n"))

    if insert_final_newline:
        if normalized:
            normalized = normalized.rstrip("\n") + "\n"
    else:
        normalized = normalized.rstrip("\n")

    if end_of_line == "crlf":
        normalized = normalized.replace("\n", "\r\n")
    elif end_of_line == "lf":
        normalized = normalized.replace("\r\n", "\n")

    return normalized


def encode_text(text: str, charset: str) -> bytes:
    """Encode normalized text with the requested charset and BOM policy."""

    normalized_charset = charset.lower()
    if normalized_charset == "utf-8":
        return text.encode("utf-8")
    if normalized_charset == "utf-8-bom":
        return UTF8_BOM + text.encode("utf-8")
    if normalized_charset == "utf-16le":
        return UTF16LE_BOM + text.encode("utf-16-le")
    if normalized_charset == "utf-16be":
        return UTF16BE_BOM + text.encode("utf-16-be")
    if normalized_charset == "latin1":
        return text.encode("latin-1")
    raise ValueError(f"Unsupported target charset '{charset}'.")


def summarize_counter(counter: Counter[str]) -> list[str]:
    """Render a stable summary of encoding counts."""

    lines = []
    for key in sorted(counter):
        lines.append(f"  {key}: {counter[key]}")
    return lines


def relative_display_path(root_path: Path, file_path: Path) -> str:
    """Return a repo-relative display path."""

    return str(file_path.relative_to(root_path))


def scan_and_normalize(args: argparse.Namespace) -> int:
    """Scan files, optionally normalize them, and print summaries."""

    root_path = Path(args.root).resolve()
    files = iter_target_files(root_path)

    encoding_counts: Counter[str] = Counter()
    detailed_paths: defaultdict[str, list[str]] = defaultdict(list)
    outcomes: list[NormalizationOutcome] = []
    failure_paths: list[str] = []

    for file_path in files:
        relative_path = relative_display_path(root_path, file_path)
        data = file_path.read_bytes()
        inspection = inspect_file_bytes(data)
        encoding_counts[inspection.label] += 1

        if inspection.label != "utf-8":
            detailed_paths[inspection.label].append(relative_path)

        if inspection.text is None:
            failure_paths.append(relative_path)
            continue

        properties = get_editorconfig_properties(file_path)
        target_charset = properties.get("charset", "utf-8").lower()
        end_of_line = properties.get("end_of_line", detect_line_ending(inspection.text)).lower()
        trim_trailing_whitespace = (
            properties.get("trim_trailing_whitespace", "false").lower() == "true"
        )
        insert_final_newline = (
            properties.get("insert_final_newline", "false").lower() == "true"
        )

        normalized_text = normalize_text_content(
            text=inspection.text,
            trim_trailing_whitespace=trim_trailing_whitespace,
            insert_final_newline=insert_final_newline,
            end_of_line=end_of_line,
        )
        target_bytes = encode_text(normalized_text, target_charset)
        changed = data != target_bytes
        written = False

        if changed and not args.report_encodings:
            action = "NORMALIZED" if args.write else "WOULD-NORMALIZE"
            print(
                f"{action}: {relative_path} "
                f"({inspection.label} -> {target_charset}, eol={end_of_line})"
            )
            if args.write:
                file_path.write_bytes(target_bytes)
                written = True

        outcomes.append(
            NormalizationOutcome(
                relative_path=relative_path,
                encoding_label=inspection.label,
                target_charset=target_charset,
                changed=changed,
                written=written,
            )
        )

    print(f"Scanned {len(files)} file(s) under '{root_path}'.")
    print("Encoding breakdown:")
    for line in summarize_counter(encoding_counts):
        print(line)

    changed_count = sum(1 for outcome in outcomes if outcome.changed)
    written_count = sum(1 for outcome in outcomes if outcome.written)
    print(f"Files needing normalization: {changed_count}")
    if args.write:
        print(f"Files rewritten: {written_count}")
    print(f"Decode failures: {len(failure_paths)}")

    if args.report_encodings:
        for label in sorted(detailed_paths):
            print(f"\n[{label}] {len(detailed_paths[label])} file(s)")
            for relative_path in detailed_paths[label]:
                print(f"  {relative_path}")

    if failure_paths:
        print("\n[decode-failures]")
        for relative_path in failure_paths:
            print(f"  {relative_path}")

    if args.check and (changed_count > 0 or failure_paths):
        return 1

    return 0


def main() -> int:
    """Run the normalizer entry point."""

    args = parse_args()
    return scan_and_normalize(args)


if __name__ == "__main__":
    raise SystemExit(main())
