# CYD Ambient Display

ESP32-2432S028R firmware for local MJPEG AVI playback, SD-card caching, WiFi updates, and a browser control page. The ESP32 does not download or decode MP4/H.264; prepare 320x240 MJPEG AVI clips on a PC/server.

## Firmware setup

1. Copy `include/secrets.example.h` to `include/secrets.h` and set `WIFI_SSID` and `WIFI_PASSWORD`. `include/secrets.h` is ignored by Git.
2. Set `AMBIENT_MANIFEST_URL` in `include/config.h` to the published manifest URL. HTTP and HTTPS URLs are accepted.
3. Build with PlatformIO using `platformio run` and upload to `esp32dev`.
4. Format the microSD card as FAT32 and put `.avi` files in `/videos`. Settings and the cached manifest are created under `/config` and `/cache`.

The TFT uses GPIO 12/13/14/15/2 and backlight GPIO 21. The CYD SD slot is configured on its separate HSPI bus: SCK 18, MISO 19, MOSI 23, CS 5. Confirm the SD pinout if using a board revision or carrier with different wiring.

## Prepare and publish clips

Convert a source video on the server/PC, not on the ESP32:

```sh
ffmpeg -i input.mp4 -an -vf "fps=15,scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2" -c:v mjpeg -q:v 8 -pix_fmt yuvj420p server/videos/space_001.avi
```

Generate `server/manifest.json` from AVI files in `server/videos/`:

```sh
python tools/generate_manifest.py --base-url https://your-host.example/videos
```

Publish that JSON at the configured manifest URL and the AVI files at their URLs. For a quick LAN-only test, run `python -m http.server 8000 --directory server`, set the manifest URL to `http://<server-ip>:8000/manifest.json`, and generate the manifest with `--base-url http://<server-ip>:8000/videos`.

The ESP32 validates manifest dimensions/FPS, downloads in 2 KB chunks into `/videos/<id>.tmp`, checks the declared size and available SD space, and renames a complete file to `.avi`. HTTPS transport currently uses encrypted TLS without certificate validation; use a trusted network/server or configure certificate validation before exposing it to untrusted networks.

## Controls

After connecting to WiFi, open the IP printed to Serial at `http://<esp32-ip>/`. The REST API is `/api/status`, `/api/play?id=...`, `/api/next`, `/api/previous`, `/api/pause`, `/api/mode?value=random`, `/api/update`, and `DELETE /api/video?id=...`. Modes are `continuous`, `random`, `loop_one`, `loop_all`, and `pause`.

Auto-update checks immediately after WiFi connects and then every six hours. Playback and download work without WiFi once clips are stored on the SD card.