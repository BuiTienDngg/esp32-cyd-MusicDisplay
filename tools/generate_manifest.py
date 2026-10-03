#!/usr/bin/env python3
"""Generate the JSON manifest for converted 320x240 MJPEG AVI clips."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


VIDEO_ID = re.compile(r"^[A-Za-z0-9_-]{1,48}$")


def main() -> None:
    repository = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--videos-dir", type=Path, default=repository / "server" / "videos")
    parser.add_argument("--output", type=Path, default=repository / "server" / "manifest.json")
    parser.add_argument("--base-url", default="https://server.example.com/videos")
    arguments = parser.parse_args()

    videos = []
    for path in sorted(arguments.videos_dir.glob("*.avi")):
        video_id = path.stem
        if not VIDEO_ID.fullmatch(video_id):
            raise SystemExit(f"Invalid video ID for {path.name}: use letters, digits, '_' or '-'.")
        videos.append(
            {
                "id": video_id,
                "name": video_id.replace("_", " "),
                "url": f"{arguments.base_url.rstrip('/')}/{path.name}",
                "size": path.stat().st_size,
                "fps": 15,
                "width": 320,
                "height": 240,
            }
        )

    previous_version = 0
    if arguments.output.exists():
        try:
            previous_version = int(json.loads(arguments.output.read_text(encoding="utf-8")).get("version", 0))
        except (TypeError, ValueError, json.JSONDecodeError):
            previous_version = 0

    manifest = {"version": previous_version + 1, "videos": videos}
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(videos)} video(s) to {arguments.output}")


if __name__ == "__main__":
    main()