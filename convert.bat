@echo off
cd /d "%~dp0"
python tools\convert_videos.py --fps 15 --quality 7
pause