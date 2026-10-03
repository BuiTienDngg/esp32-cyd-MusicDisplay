#!/usr/bin/env python3
"""Convert local videos to 320x240, 15 fps MJPEG AVI files for the CYD."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import unicodedata
from fractions import Fraction
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "raw_videos"
EXISTING_VIDEO_INPUT = ROOT / "video"
DEFAULT_OUTPUT = ROOT / "sdcard" / "videos"
VIDEO_EXTENSIONS = {".mp4", ".mov", ".mkv", ".webm", ".avi"}
OUTPUT_WIDTH = 320
OUTPUT_HEIGHT = 240


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Batch-convert videos to 320x240 MJPEG AVI for ESP32 CYD."
    )
    parser.add_argument("--input", type=Path, help="Input folder (default: raw_videos/)")
    parser.add_argument(
        "--output", type=Path, help="Output folder (default: sdcard/videos/)"
    )
    parser.add_argument(
        "--fps", type=int, choices=range(10, 16), default=15,
        metavar="{10,11,12,13,14,15}", help="Output frame rate, 10-15 (default: 15)"
    )
    parser.add_argument(
        "--quality", type=int, choices=range(5, 11), default=7,
        metavar="{5,6,7,8,9,10}", help="MJPEG q:v value, 5-10 (default: 7)"
    )
    parser.add_argument(
        "--force", action="store_true", help="Replace existing AVI files"
    )
    return parser.parse_args()


def find_videos(folder: Path) -> list[Path]:
    if not folder.is_dir():
        return []
    return sorted(
        (path for path in folder.rglob("*")
         if path.is_file() and path.suffix.lower() in VIDEO_EXTENSIONS),
        key=lambda path: str(path).casefold(),
    )


def select_input_folder(requested: Path | None) -> tuple[Path, list[Path]]:
    if requested is not None:
        folder = requested.expanduser().resolve()
        return folder, find_videos(folder)

    videos = find_videos(DEFAULT_INPUT)
    if videos:
        return DEFAULT_INPUT, videos

    existing_videos = find_videos(EXISTING_VIDEO_INPUT)
    if existing_videos:
        print(
            f"[INFO] {DEFAULT_INPUT.name}/ is empty; using existing "
            f"{EXISTING_VIDEO_INPUT.name}/ sources."
        )
        return EXISTING_VIDEO_INPUT, existing_videos
    return DEFAULT_INPUT, []


def find_tool(name: str) -> str | None:
    executable = name + ".exe" if sys.platform == "win32" else name
    return shutil.which(executable) or shutil.which(name)


def print_ffmpeg_install_help(missing: list[str]) -> None:
    print(f"ERROR: {', '.join(missing)} not found in PATH.", file=sys.stderr)
    print("Install FFmpeg on Windows:", file=sys.stderr)
    print("  1. Download a Windows build from https://www.gyan.dev/ffmpeg/builds/", file=sys.stderr)
    print("  2. Extract it and add its 'bin' folder to your PATH.", file=sys.stderr)
    print("  3. Open a new terminal and verify: ffmpeg -version", file=sys.stderr)
    print("     and: ffprobe -version", file=sys.stderr)


def run_probe(ffprobe: str, path: Path) -> dict[str, Any]:
    command = [
        ffprobe,
        "-v", "error",
        "-show_entries",
        "stream=codec_type,codec_name,width,height,avg_frame_rate,r_frame_rate,duration",
        "-show_entries", "format=duration",
        "-of", "json",
        str(path),
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        details = result.stderr.strip() or "ffprobe could not read the file"
        raise RuntimeError(details)
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError("ffprobe returned invalid JSON") from error


def frame_rate(stream: dict[str, Any]) -> float | None:
    for key in ("avg_frame_rate", "r_frame_rate"):
        value = stream.get(key)
        if value and value != "0/0":
            try:
                return float(Fraction(value))
            except (ValueError, ZeroDivisionError):
                continue
    return None


def duration_seconds(probe: dict[str, Any]) -> float | None:
    value = probe.get("format", {}).get("duration")
    if value is None:
        value = next(
            (stream.get("duration") for stream in probe.get("streams", [])
             if stream.get("codec_type") == "video" and stream.get("duration")),
            None,
        )
    try:
        return float(value) if value is not None else None
    except (TypeError, ValueError):
        return None


def safe_stem(stem: str) -> str:
    normalized = unicodedata.normalize("NFKD", stem)
    ascii_name = normalized.encode("ascii", "ignore").decode("ascii")
    sanitized = re.sub(r"[^A-Za-z0-9]+", "_", ascii_name).strip("_")
    return sanitized or "video"


def output_paths(sources: list[Path], output_dir: Path) -> list[Path]:
    used_names: set[str] = set()
    results = []
    for source in sources:
        base = safe_stem(source.stem)
        name = base
        suffix = 2
        while name.casefold() in used_names:
            name = f"{base}_{suffix}"
            suffix += 1
        used_names.add(name.casefold())
        results.append(output_dir / f"{name}.avi")
    return results


def validate_output(
    ffprobe: str, output: Path, requested_fps: int
) -> tuple[bool, float | None]:
    probe = run_probe(ffprobe, output)
    streams = probe.get("streams", [])
    video = next(
        (stream for stream in streams if stream.get("codec_type") == "video"), None
    )
    has_audio = any(stream.get("codec_type") == "audio" for stream in streams)
    if video is None:
        print("  Codec      : ERROR (no video stream)")
        print("  Resolution : ERROR")
        print("  FPS        : ERROR")
        print(f"  Audio      : {'ERROR (present)' if has_audio else 'NONE OK'}")
        return False, duration_seconds(probe)

    codec_ok = video.get("codec_name") == "mjpeg"
    resolution_ok = (
        video.get("width") == OUTPUT_WIDTH and video.get("height") == OUTPUT_HEIGHT
    )
    actual_fps = frame_rate(video)
    fps_ok = actual_fps is not None and abs(actual_fps - requested_fps) < 0.02
    audio_ok = not has_audio

    print(f"  Codec      : {'MJPEG OK' if codec_ok else 'ERROR (' + str(video.get('codec_name')) + ')'}")
    print(
        "  Resolution : "
        f"{'320x240 OK' if resolution_ok else 'ERROR (' + str(video.get('width')) + 'x' + str(video.get('height')) + ')'}"
    )
    actual_text = f"{actual_fps:g}" if actual_fps is not None else "unknown"
    print(f"  FPS        : {'OK' if fps_ok else 'ERROR'} ({actual_text}, target {requested_fps})")
    print(f"  Audio      : {'ERROR (present)' if has_audio else 'NONE OK'}")
    return codec_ok and resolution_ok and fps_ok and audio_ok, duration_seconds(probe)


def format_size(byte_count: int) -> str:
    return f"{byte_count / (1024 * 1024):.1f} MB"


def convert_one(
    ffmpeg: str,
    ffprobe: str,
    source: Path,
    destination: Path,
    fps: int,
    quality: int,
    force: bool,
) -> bool:
    if destination.exists() and not force:
        print(f"SKIPPED (already exists): {destination.name}")
        return True

    try:
        input_probe = run_probe(ffprobe, source)
        input_video = next(
            (stream for stream in input_probe.get("streams", [])
             if stream.get("codec_type") == "video"),
            {},
        )
        input_fps = frame_rate(input_video)
        input_fps_text = f"{input_fps:g}" if input_fps is not None else "unknown"
        input_width = input_video.get("width", "unknown")
        input_height = input_video.get("height", "unknown")
        print(f"Input : {input_width}x{input_height} @ {input_fps_text} FPS")
    except RuntimeError as error:
        print(f"ERROR: Cannot inspect input: {error}")
        return False

    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(f"{destination.stem}.partial.avi")
    temporary.unlink(missing_ok=True)
    video_filter = (
        f"fps={fps},"
        f"scale={OUTPUT_WIDTH}:{OUTPUT_HEIGHT}:"
        "force_original_aspect_ratio=increase:flags=lanczos,"
        f"crop={OUTPUT_WIDTH}:{OUTPUT_HEIGHT}"
    )
    command = [
        ffmpeg,
        "-hide_banner",
        "-loglevel", "warning",
        "-stats",
        "-y",
        "-i", str(source),
        "-map", "0:v:0",
        "-vf", video_filter,
        "-r", str(fps),
        "-c:v", "mjpeg",
        "-q:v", str(quality),
        "-pix_fmt", "yuvj420p",
        "-an",
        "-sn",
        "-dn",
        "-f", "avi",
        str(temporary),
    ]

    print(f"Output: {OUTPUT_WIDTH}x{OUTPUT_HEIGHT} @ {fps} FPS MJPEG")
    print("Converting...")
    try:
        result = subprocess.run(command, check=False)
    except OSError as error:
        print(f"ERROR: Could not start FFmpeg: {error}")
        temporary.unlink(missing_ok=True)
        return False
    if result.returncode != 0:
        print(f"ERROR: FFmpeg exited with code {result.returncode}")
        temporary.unlink(missing_ok=True)
        return False

    try:
        print("Validation:")
        valid, output_duration = validate_output(ffprobe, temporary, fps)
    except RuntimeError as error:
        print(f"ERROR: Could not validate output: {error}")
        temporary.unlink(missing_ok=True)
        return False
    if not valid:
        print("ERROR: Output validation failed; final AVI was not replaced.")
        temporary.unlink(missing_ok=True)
        return False

    try:
        temporary.replace(destination)
    except OSError as error:
        print(f"ERROR: Could not move validated output into place: {error}")
        temporary.unlink(missing_ok=True)
        return False

    duration = output_duration if output_duration is not None else duration_seconds(input_probe)
    duration_text = f"{duration:.1f} seconds" if duration is not None else "unknown"
    print(f"Input size : {format_size(source.stat().st_size)}")
    print(f"Output size: {format_size(destination.stat().st_size)}")
    print(f"Duration   : {duration_text}")
    print("DONE")
    return True


def main() -> int:
    args = parse_args()
    input_dir, sources = select_input_folder(args.input)
    if not input_dir.is_dir():
        print(f"ERROR: Input folder does not exist: {input_dir}", file=sys.stderr)
        return 2
    if not sources:
        print(f"No supported videos found in: {input_dir}")
        print("Supported formats: MP4, MOV, MKV, WEBM, AVI")
        return 0

    ffmpeg = find_tool("ffmpeg")
    ffprobe = find_tool("ffprobe")
    missing = [name for name, path in (("ffmpeg", ffmpeg), ("ffprobe", ffprobe)) if path is None]
    if missing:
        print_ffmpeg_install_help(missing)
        return 2

    output_dir = args.output.expanduser().resolve() if args.output else DEFAULT_OUTPUT
    destinations = output_paths(sources, output_dir)
    print(f"Found {len(sources)} videos in {input_dir}")
    print(f"Output folder: {output_dir}")

    failures = 0
    for index, (source, destination) in enumerate(zip(sources, destinations), start=1):
        print(f"\n[{index}/{len(sources)}] {source.name}")
        if not convert_one(
            ffmpeg, ffprobe, source, destination, args.fps, args.quality, args.force
        ):
            failures += 1

    print(f"\nFinished: {len(sources) - failures} successful/skipped, {failures} error(s).")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())