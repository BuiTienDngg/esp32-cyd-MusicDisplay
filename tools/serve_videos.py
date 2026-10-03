#!/usr/bin/env python3
"""Serve converted CYD AVI files and a live JSON manifest over the LAN."""

from __future__ import annotations

import argparse
import json
import re
import socket
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import quote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VIDEO_DIR = ROOT / "sdcard" / "videos"
VIDEO_ID = re.compile(r"^[A-Za-z0-9_-]{1,48}$")


def discover_address() -> str:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("192.168.55.106", 80))
        return probe.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        probe.close()


class AmbientRequestHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, video_dir: Path, advertise_host: str,
                 port: int, fps: int, **kwargs):
        self.video_dir = video_dir
        self.advertise_host = advertise_host
        self.server_port = port
        self.fps = fps
        super().__init__(*args, directory=str(video_dir.parent), **kwargs)

    def do_GET(self) -> None:
        if urlsplit(self.path).path == "/manifest.json":
            self.send_manifest()
            return
        super().do_GET()

    def send_manifest(self) -> None:
        videos = []
        for path in sorted(self.video_dir.glob("*.avi"), key=lambda item: item.name.casefold()):
            video_id = path.stem
            if not VIDEO_ID.fullmatch(video_id):
                continue
            videos.append(
                {
                    "id": video_id,
                    "name": video_id.replace("_", " "),
                    "url": (
                        f"http://{self.advertise_host}:{self.server_port}/videos/"
                        f"{quote(path.name)}"
                    ),
                    "size": path.stat().st_size,
                    "fps": self.fps,
                    "width": 320,
                    "height": 240,
                }
            )

        body = json.dumps({"version": 1, "videos": videos}, indent=2).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0", help="Bind address (default: all interfaces)")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--videos", type=Path, default=DEFAULT_VIDEO_DIR)
    parser.add_argument("--advertise-host", default=None, help="PC LAN IP included in video URLs")
    parser.add_argument("--fps", type=int, choices=range(10, 16), default=15)
    args = parser.parse_args()

    video_dir = args.videos.expanduser().resolve()
    if not video_dir.is_dir():
        parser.error(f"video directory does not exist: {video_dir}")

    advertise_host = args.advertise_host or discover_address()
    handler = partial(
        AmbientRequestHandler,
        video_dir=video_dir,
        advertise_host=advertise_host,
        port=args.port,
        fps=args.fps,
    )
    try:
        server = ThreadingHTTPServer((args.host, args.port), handler)
    except OSError as error:
        parser.error(f"cannot start HTTP server: {error}")

    print(f"Serving AVI files from: {video_dir}")
    print(f"Manifest: http://{advertise_host}:{args.port}/manifest.json")
    print(f"ESP32 firmware manifest URL: http://{advertise_host}:{args.port}/manifest.json")
    print("Press Ctrl+C to stop the server.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping video server.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()