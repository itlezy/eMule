#!/usr/bin/env python3
"""Helper utilities for the debug launch workflow."""

from __future__ import annotations

import argparse
import configparser
from pathlib import Path


DEBUG_PRESET = {
    "CreateCrashDump": "2",
    "Verbose": "1",
    "FullVerbose": "1",
    "SaveLogToDisk": "1",
    "SaveDebugToDisk": "1",
    "DebugSourceExchange": "1",
    "LogBannedClients": "1",
    "LogFileSaving": "1",
    "LogUlDlEvents": "1",
    "DebugServerTCP": "1",
    "DebugServerUDP": "1",
    "DebugServerSources": "1",
    "DebugServerSearches": "1",
    "DebugClientTCP": "1",
    "DebugClientUDP": "1",
    "DebugClientKadUDP": "1",
    "DebugSearchResultDetail": "1",
}


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="eMule debug launch helper")
    subparsers = parser.add_subparsers(dest="command", required=True)

    write_prefs = subparsers.add_parser(
        "write-debug-prefs",
        help="Create or patch preferences.ini with the debug preset.",
    )
    write_prefs.add_argument(
        "--preferences-ini",
        required=True,
        help="Absolute path to preferences.ini",
    )
    write_prefs.add_argument(
        "--preserve-existing",
        action="store_true",
        help="Preserve existing settings and only overlay the debug preset.",
    )
    return parser


def load_config(path: Path, preserve_existing: bool) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(interpolation=None)
    parser.optionxform = str
    if preserve_existing and path.exists():
        with path.open("r", encoding="utf-8-sig") as handle:
            parser.read_file(handle)
    return parser


def write_debug_prefs(preferences_ini: Path, preserve_existing: bool) -> int:
    parser = load_config(preferences_ini, preserve_existing)
    if not parser.has_section("eMule"):
        parser.add_section("eMule")
    for key, value in DEBUG_PRESET.items():
        parser.set("eMule", key, value)

    preferences_ini.parent.mkdir(parents=True, exist_ok=True)
    with preferences_ini.open("w", encoding="utf-8", newline="\n") as handle:
        parser.write(handle)
    return 0


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.command == "write-debug-prefs":
        return write_debug_prefs(Path(args.preferences_ini), args.preserve_existing)
    parser.error(f"Unsupported command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
