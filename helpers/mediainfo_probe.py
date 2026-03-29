#!/usr/bin/env python3
"""Probe MediaInfo.dll against sample media files using eMule's queried fields."""

from __future__ import annotations

import argparse
import ctypes
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any
import winreg


MEDIAINFO_STREAM_GENERAL = 0
MEDIAINFO_STREAM_VIDEO = 1
MEDIAINFO_STREAM_AUDIO = 2
MEDIAINFO_STREAM_TEXT = 3
MEDIAINFO_STREAM_MENU = 6

MEDIAINFO_INFO_NAME = 0
MEDIAINFO_INFO_TEXT = 1

MINIMUM_VERSION = (23, 0, 0, 0)

GENERAL_FIELDS = [
    "Format",
    "Format/String",
    "Format/Extensions",
    "Duration",
    "VideoCount",
    "AudioCount",
    "TextCount",
    "MenuCount",
    "Title",
    "Title_More",
    "Performer",
    "Author",
    "Copyright",
    "Comments",
    "Comment",
    "Date",
    "Encoded_Date",
]

VIDEO_FIELDS = [
    "Format",
    "Format/String",
    "Width",
    "Height",
    "FrameRate",
    "BitRate_Mode",
    "BitRate",
    "AspectRatio",
]

AUDIO_FIELDS = [
    "Format",
    "Format/String",
    "Format/Info",
    "Channel(s)",
    "SamplingRate",
    "BitRate_Mode",
    "BitRate",
    "Language/String",
    "Language",
    "Language_More",
]

TEXT_FIELDS = [
    "Format",
    "Language/String",
    "Language",
    "Language_More",
]


@dataclass(frozen=True)
class Version:
    """Windows DLL version information."""

    major: int
    minor: int
    patch: int
    build: int

    def as_tuple(self) -> tuple[int, int, int, int]:
        return (self.major, self.minor, self.patch, self.build)

    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}.{self.build}"


class VS_FIXEDFILEINFO(ctypes.Structure):
    """Version structure returned by the Windows version APIs."""

    _fields_ = [
        ("dwSignature", ctypes.c_uint32),
        ("dwStrucVersion", ctypes.c_uint32),
        ("dwFileVersionMS", ctypes.c_uint32),
        ("dwFileVersionLS", ctypes.c_uint32),
        ("dwProductVersionMS", ctypes.c_uint32),
        ("dwProductVersionLS", ctypes.c_uint32),
        ("dwFileFlagsMask", ctypes.c_uint32),
        ("dwFileFlags", ctypes.c_uint32),
        ("dwFileOS", ctypes.c_uint32),
        ("dwFileType", ctypes.c_uint32),
        ("dwFileSubtype", ctypes.c_uint32),
        ("dwFileDateMS", ctypes.c_uint32),
        ("dwFileDateLS", ctypes.c_uint32),
    ]


class MediaInfoLibrary:
    """Thin ctypes binding for the MediaInfo C API used by eMule."""

    def __init__(self, dll_path: Path) -> None:
        self.dll_path = dll_path
        self.version = get_file_version(dll_path)
        if self.version.as_tuple() < MINIMUM_VERSION:
            raise RuntimeError(
                f"{dll_path} version {self.version} is below required "
                f"{MINIMUM_VERSION[0]}.{MINIMUM_VERSION[1]:02d}"
            )

        self._lib = ctypes.WinDLL(str(dll_path))
        self._lib.MediaInfo_New.restype = ctypes.c_void_p
        self._lib.MediaInfo_Delete.argtypes = [ctypes.c_void_p]
        self._lib.MediaInfo_Open.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p]
        self._lib.MediaInfo_Open.restype = ctypes.c_size_t
        self._lib.MediaInfo_Get.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_size_t,
            ctypes.c_wchar_p,
            ctypes.c_size_t,
            ctypes.c_size_t,
        ]
        self._lib.MediaInfo_Get.restype = ctypes.c_wchar_p
        self._lib.MediaInfo_GetI.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_size_t,
            ctypes.c_size_t,
            ctypes.c_size_t,
        ]
        self._lib.MediaInfo_GetI.restype = ctypes.c_wchar_p
        self._validate_exports()

    def _validate_exports(self) -> None:
        """Ensure the minimum export set required by the app exists."""
        required_exports = (
            "MediaInfo_New",
            "MediaInfo_Delete",
            "MediaInfo_Open",
            "MediaInfo_Close",
            "MediaInfo_Get",
        )
        for export_name in required_exports:
            if not hasattr(self._lib, export_name):
                raise RuntimeError(f"{self.dll_path} is missing export {export_name}")

    def open(self, file_path: Path) -> ctypes.c_void_p:
        """Open one file and return the MediaInfo handle."""
        handle = self._lib.MediaInfo_New()
        if not handle:
            raise RuntimeError("MediaInfo_New returned NULL")
        if self._lib.MediaInfo_Open(handle, str(file_path)) == 0:
            self._lib.MediaInfo_Delete(handle)
            raise RuntimeError(f"MediaInfo_Open failed for {file_path}")
        return handle

    def close(self, handle: ctypes.c_void_p) -> None:
        """Release the MediaInfo handle."""
        self._lib.MediaInfo_Delete(handle)

    def get(self, handle: ctypes.c_void_p, stream_kind: int, stream_number: int, parameter: str) -> str:
        """Fetch one text field exactly as eMule requests it."""
        value = self._lib.MediaInfo_Get(
            handle,
            stream_kind,
            stream_number,
            parameter,
            MEDIAINFO_INFO_TEXT,
            MEDIAINFO_INFO_NAME,
        )
        return value or ""

    def get_i(self, handle: ctypes.c_void_p, stream_kind: int, stream_number: int, parameter_index: int) -> dict[str, str]:
        """Fetch a menu/chapter field by numeric index."""
        name = self._lib.MediaInfo_GetI(
            handle,
            stream_kind,
            stream_number,
            parameter_index,
            MEDIAINFO_INFO_NAME,
        ) or ""
        text = self._lib.MediaInfo_GetI(
            handle,
            stream_kind,
            stream_number,
            parameter_index,
            MEDIAINFO_INFO_TEXT,
        ) or ""
        return {"name": name, "text": text}


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Probe MediaInfo.dll with eMule's queried fields")
    parser.add_argument(
        "inputs",
        nargs="+",
        help="Files or directories to probe",
    )
    parser.add_argument(
        "--dll-path",
        help="Explicit MediaInfo.dll path. Defaults to the first app-compatible candidate.",
    )
    parser.add_argument(
        "--output",
        help="Write the JSON report to this path.",
    )
    parser.add_argument(
        "--include-ffprobe",
        action="store_true",
        help="Attach an ffprobe summary when ffprobe.exe is available.",
    )
    return parser


def get_file_version(dll_path: Path) -> Version:
    """Read a DLL file version with the Win32 version APIs."""
    version_dll = ctypes.WinDLL("version.dll")
    get_size = version_dll.GetFileVersionInfoSizeW
    get_size.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_uint32)]
    get_size.restype = ctypes.c_uint32

    dummy = ctypes.c_uint32(0)
    size = get_size(str(dll_path), ctypes.byref(dummy))
    if size == 0:
        raise RuntimeError(f"GetFileVersionInfoSizeW failed for {dll_path}")

    buffer = (ctypes.c_byte * size)()
    get_info = version_dll.GetFileVersionInfoW
    get_info.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    get_info.restype = ctypes.c_int
    if not get_info(str(dll_path), 0, size, ctypes.byref(buffer)):
        raise RuntimeError(f"GetFileVersionInfoW failed for {dll_path}")

    query_value = version_dll.VerQueryValueW
    query_value.argtypes = [
        ctypes.c_void_p,
        ctypes.c_wchar_p,
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_uint32),
    ]
    query_value.restype = ctypes.c_int

    value_ptr = ctypes.c_void_p()
    value_len = ctypes.c_uint32()
    if not query_value(ctypes.byref(buffer), "\\", ctypes.byref(value_ptr), ctypes.byref(value_len)):
        raise RuntimeError(f"VerQueryValueW failed for {dll_path}")

    fixed = ctypes.cast(value_ptr, ctypes.POINTER(VS_FIXEDFILEINFO)).contents
    return Version(
        major=(fixed.dwFileVersionMS >> 16) & 0xFFFF,
        minor=fixed.dwFileVersionMS & 0xFFFF,
        patch=(fixed.dwFileVersionLS >> 16) & 0xFFFF,
        build=fixed.dwFileVersionLS & 0xFFFF,
    )


def collect_candidate_paths(explicit_dll: str | None) -> list[Path]:
    """Mirror the app's deterministic MediaInfo candidate search order."""
    candidates: list[Path] = []

    def add_candidate(candidate: Path | None) -> None:
        if candidate is None:
            return
        try:
            resolved = candidate.resolve(strict=False)
        except OSError:
            return
        normalized = Path(os.path.normcase(str(resolved)))
        if any(os.path.normcase(str(path)) == str(normalized) for path in candidates):
            return
        candidates.append(resolved)

    if explicit_dll:
        add_candidate(Path(explicit_dll))
        return candidates

    for hive in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
        try:
            with winreg.OpenKey(hive, r"Software\MediaInfo") as key:
                install_root = winreg.QueryValueEx(key, "Path")[0]
                add_candidate(Path(install_root) / "MediaInfo.dll")
        except OSError:
            continue

    program_files = os.environ.get("ProgramFiles")
    if program_files:
        add_candidate(Path(program_files) / "MediaInfo" / "MediaInfo.dll")

    return candidates


def resolve_library(explicit_dll: str | None) -> MediaInfoLibrary:
    """Return the first candidate matching the app's version contract."""
    failures: list[str] = []
    for candidate in collect_candidate_paths(explicit_dll):
        if not candidate.exists():
            failures.append(f"{candidate}: missing")
            continue
        try:
            return MediaInfoLibrary(candidate)
        except Exception as exc:  # pragma: no cover - exercised by local runs
            failures.append(f"{candidate}: {exc}")
    failure_text = "\n".join(failures) if failures else "No MediaInfo candidates found"
    raise RuntimeError(failure_text)


def expand_inputs(inputs: list[str]) -> list[Path]:
    """Expand input directories into a sorted list of media files."""
    files: list[Path] = []
    for raw_input in inputs:
        path = Path(raw_input)
        if path.is_dir():
            files.extend(sorted(item for item in path.iterdir() if item.is_file()))
        elif path.is_file():
            files.append(path)
    return sorted(files, key=lambda item: item.name.lower())


def get_count(field_map: dict[str, str], key: str) -> int:
    """Parse a MediaInfo count field."""
    value = field_map.get(key, "").strip()
    try:
        return int(value)
    except ValueError:
        return 0


def collect_stream_fields(
    library: MediaInfoLibrary,
    handle: ctypes.c_void_p,
    stream_kind: int,
    stream_count: int,
    field_names: list[str],
) -> list[dict[str, str]]:
    """Collect a list of stream dictionaries for one stream kind."""
    streams: list[dict[str, str]] = []
    for stream_index in range(stream_count):
        stream_fields = {
            field_name: library.get(handle, stream_kind, stream_index, field_name)
            for field_name in field_names
        }
        streams.append(stream_fields)
    return streams


def collect_menu_fields(library: MediaInfoLibrary, handle: ctypes.c_void_p, menu_count: int) -> list[dict[str, Any]]:
    """Collect chapter/menu information using the indexed API."""
    menus: list[dict[str, Any]] = []
    for menu_index in range(menu_count):
        begin = int(library.get(handle, MEDIAINFO_STREAM_MENU, menu_index, "Chapters_Pos_Begin") or "0")
        end = int(library.get(handle, MEDIAINFO_STREAM_MENU, menu_index, "Chapters_Pos_End") or "0")
        chapters = [
            library.get_i(handle, MEDIAINFO_STREAM_MENU, menu_index, chapter_index)
            for chapter_index in range(begin, end)
        ]
        menus.append({"index": menu_index, "chapter_count": len(chapters), "chapters": chapters})
    return menus


def classify_emule_backend(file_path: Path) -> str:
    """Describe which backend the current eMule code will try first for the file."""
    extension = file_path.suffix.lower()
    if extension == ".avi":
        return "native_riff"
    if extension in {".rm", ".rmvb", ".ra"}:
        return "native_realmedia"
    if extension in {".asf", ".wm", ".wma", ".wmv", ".dvr-ms"}:
        return "native_windows_media"
    if extension in {".mp3", ".mp2", ".mp1", ".mpa"}:
        return "mediainfo_dll"
    return "mediainfo_dll"


def run_ffprobe(file_path: Path) -> dict[str, Any] | None:
    """Collect a compact ffprobe reference summary when available."""
    ffprobe = shutil_which("ffprobe")
    if ffprobe is None:
        return None
    process = subprocess.run(
        [
            ffprobe,
            "-v",
            "error",
            "-show_streams",
            "-show_format",
            "-print_format",
            "json",
            str(file_path),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if process.returncode != 0:
        return {"error": process.stderr.strip() or f"ffprobe exit code {process.returncode}"}
    payload = json.loads(process.stdout)
    format_info = payload.get("format", {})
    streams = payload.get("streams", [])
    return {
        "format_name": format_info.get("format_name", ""),
        "duration": format_info.get("duration", ""),
        "size": format_info.get("size", ""),
        "video_streams": [
            {
                "codec_name": stream.get("codec_name", ""),
                "codec_long_name": stream.get("codec_long_name", ""),
                "width": stream.get("width", ""),
                "height": stream.get("height", ""),
                "avg_frame_rate": stream.get("avg_frame_rate", ""),
                "display_aspect_ratio": stream.get("display_aspect_ratio", ""),
            }
            for stream in streams
            if stream.get("codec_type") == "video"
        ],
        "audio_streams": [
            {
                "codec_name": stream.get("codec_name", ""),
                "codec_long_name": stream.get("codec_long_name", ""),
                "channels": stream.get("channels", ""),
                "sample_rate": stream.get("sample_rate", ""),
                "bit_rate": stream.get("bit_rate", ""),
                "language": (stream.get("tags", {}) or {}).get("language", ""),
            }
            for stream in streams
            if stream.get("codec_type") == "audio"
        ],
        "subtitle_streams": [
            {
                "codec_name": stream.get("codec_name", ""),
                "language": (stream.get("tags", {}) or {}).get("language", ""),
            }
            for stream in streams
            if stream.get("codec_type") == "subtitle"
        ],
    }


def shutil_which(command: str) -> str | None:
    """Local `which` helper to avoid another dependency import."""
    for directory in os.environ.get("PATH", "").split(os.pathsep):
        candidate = Path(directory) / f"{command}.exe"
        if candidate.exists():
            return str(candidate)
        candidate = Path(directory) / command
        if candidate.exists():
            return str(candidate)
    return None


def probe_file(library: MediaInfoLibrary, file_path: Path, include_ffprobe: bool) -> dict[str, Any]:
    """Probe one file and return a JSON-serializable result."""
    handle = library.open(file_path)
    try:
        general = {field_name: library.get(handle, MEDIAINFO_STREAM_GENERAL, 0, field_name) for field_name in GENERAL_FIELDS}
        video_count = get_count(general, "VideoCount")
        audio_count = get_count(general, "AudioCount")
        text_count = get_count(general, "TextCount")
        menu_count = get_count(general, "MenuCount")
        result: dict[str, Any] = {
            "file_name": file_path.name,
            "full_path": str(file_path),
            "emule_backend": classify_emule_backend(file_path),
            "general": general,
            "video_streams": collect_stream_fields(library, handle, MEDIAINFO_STREAM_VIDEO, video_count, VIDEO_FIELDS),
            "audio_streams": collect_stream_fields(library, handle, MEDIAINFO_STREAM_AUDIO, audio_count, AUDIO_FIELDS),
            "text_streams": collect_stream_fields(library, handle, MEDIAINFO_STREAM_TEXT, text_count, TEXT_FIELDS),
            "menu_streams": collect_menu_fields(library, handle, menu_count),
        }
        if include_ffprobe:
            result["ffprobe"] = run_ffprobe(file_path)
        return result
    finally:
        library.close(handle)


def summarize_results(results: list[dict[str, Any]]) -> list[str]:
    """Build a concise human-readable summary for terminal output."""
    lines: list[str] = []
    for result in results:
        general = result["general"]
        video_streams = result["video_streams"]
        audio_streams = result["audio_streams"]
        general_format = general.get("Format", "") or "(blank)"
        duration = general.get("Duration", "") or "(blank)"
        line = (
            f"{result['file_name']}: backend={result['emule_backend']} "
            f"format={general_format} duration_ms={duration} "
            f"video={len(video_streams)} audio={len(audio_streams)}"
        )
        if video_streams:
            first_video = video_streams[0]
            line += (
                f" v0={first_video.get('Format', '') or '(blank)'}"
                f" {first_video.get('Width', '')}x{first_video.get('Height', '')}"
            )
        if audio_streams:
            first_audio = audio_streams[0]
            line += f" a0={first_audio.get('Format', '') or '(blank)'}"
        lines.append(line)
    return lines


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    files = expand_inputs(args.inputs)
    if not files:
        parser.error("No files found to probe")

    library = resolve_library(args.dll_path)
    results = [probe_file(library, file_path, args.include_ffprobe) for file_path in files]
    payload = {
        "mediainfo_dll_path": str(library.dll_path),
        "mediainfo_dll_version": str(library.version),
        "minimum_required_version": ".".join(str(part) for part in MINIMUM_VERSION),
        "results": results,
    }

    for line in summarize_results(results):
        print(line)

    if args.output:
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"JSON report written to {output_path}")
    else:
        print(json.dumps(payload, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
