<h1 align="center">Dynamic Wallpaper · Set video as Windows desktop wallpaper</h1>

<p align="center">
  <a href="README.md">简体中文</a>
  |
  <a href="README-en.md">English</a>
</p>

## Build

### CMake

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release
```

> You can also run `.\build_and_run.ps1` for a one-click build and launch

## Usage

1. Run `wallpaper.exe` — make sure `libmpv-2.dll` and `config.json` are in the same directory
2. Edit `background_path` in `config.json` to point to your video file (default: `background.mp4`)
3. Set `wallpaper_enabled` to `false` to disable the wallpaper temporarily; `auto_startup` controls autostart on login
4. The app shows a system tray icon after startup

### Configuration

```json
{
    "auto_startup": true,
    "dynamic_wallpaper": {
        "enabled": true,
        "background_path": "background.mp4"
    }
}
```

- `auto_startup`: Whether to autostart with Windows
- `enabled`: Whether to show the wallpaper
- `background_path`: Video file path

## Acknowledgements

- [mpv](https://mpv.io/) — Video playback engine, used under LGPLv2.1+
- [nlohmann/json](https://github.com/nlohmann/json) — JSON parsing library, MIT License

## License

MIT License
