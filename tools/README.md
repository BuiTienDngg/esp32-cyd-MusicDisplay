# Convert videos for the CYD

1. Install FFmpeg for Windows from [gyan.dev](https://www.gyan.dev/ffmpeg/builds/), extract it, and add its `bin` folder to `PATH`. Open a new terminal and check `ffmpeg -version` and `ffprobe -version`.
2. Put MP4, MOV, MKV, WEBM, or AVI files in `raw_videos/`. If that folder is empty, the script automatically uses the existing workspace `video/` folder.
3. Double-click `convert.bat`, or run `python tools/convert_videos.py --fps 15 --quality 7` from the project root.

Validated, silent 320x240 MJPEG AVI files are written to `sdcard/videos/`. Copy those AVI files into the microSD card's `/videos` directory. Existing outputs are skipped unless `--force` is supplied. Use `--fps 10` to reduce the frame rate; accepted values are 10-15, and MJPEG quality values are 5-10.

The converter center-crops to 4:3 after aspect-preserving scaling, so the image fills the panel without stretching or letterboxing. Source videos are never modified or deleted.

## Serve videos to the ESP32

Keep the ESP32 and this PC on the same LAN. Start the HTTP server from the project root:

```powershell
python tools/serve_videos.py --advertise-host 192.168.55.101
```

It serves `sdcard/videos/` and creates `/manifest.json` dynamically. The firmware is configured to fetch `http://192.168.55.101:8000/manifest.json`; if the PC's WiFi address changes, update `AMBIENT_MANIFEST_URL` in `include/config.h`, then rebuild and upload. Keep the server running while the ESP32 checks or downloads clips. Allow Python through Windows Firewall on private networks if prompted. New clips are scanned on every manifest request.

The ESP32 web page has **Check for new videos** and **Download new videos** controls. The firmware downloads each AVI to the SD card's `/videos` directory and adds completed clips to playback. For a different PC address or port, pass the matching `--advertise-host`/`--port` and set the same address in firmware.