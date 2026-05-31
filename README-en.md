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

1. Run `wallpaper.exe` — make sure `libmpv-2.dll`, `config.json` and `public/` are in the same directory
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
    },
    "desktop_overlay": {
        "enabled": true
    }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `auto_startup` | bool | Autostart with Windows |
| `dynamic_wallpaper.enabled` | bool | Enable video wallpaper |
| `dynamic_wallpaper.background_path` | string | Path to video file |
| `desktop_overlay.enabled` | bool | Enable desktop text overlay (clock, date, etc.) |
| `desktop_overlay.widgets_dir` | string | Widget JSON directory (default `public/widgets`) |

### Overlay (Desktop Text)

The app can render custom text overlays on the desktop. By default it shows a clock, date, and weekday background.

Each widget is defined as a `.json` file in `public/widgets/`, one layer per file.

**Default widgets:**

| File | Content | Default Style |
|------|---------|---------------|
| `weekday.json` | Weekday background (e.g. Monday) | Segoe Script 140pt, semi-transparent decorative text |
| `time.json` | Clock (e.g. 14:30) | Segoe UI 95pt bold, centered |
| `date.json` | Date (e.g. 1 June) | Segoe UI 36pt bold, centered |

**Customizable fields:**

```json
{
    "id": "time",
    "type": "time",         // time / date / weekday / static
    "enabled": true,
    "position": {
        "x_percent": 50.0,  // Rect left edge X (screen %)
        "y_percent": 38.0,  // Rect top edge Y (screen %)
        "width_percent": 40.0,   // Rect width (screen %)
        "height_percent": 14.0,  // Rect height (screen %)
        "anchor": "center"       // top_left / top_center / center / bottom_center
    },
    "style": {
        "font_family": "Segoe UI",
        "font_size": 95,
        "font_style": "regular",  // regular / bold / italic / bold_italic
        "color": "#F0FFFFFF",     // #AARRGGBB or #RRGGBB
        "align": "center",        // near / center / far (horizontal)
        "line_align": "center",   // near / center / far (vertical)
        "shadow": {
            "enabled": true,
            "offset_x": 2,
            "offset_y": 2,
            "color": "#66000000"
        }
    }
}
```

- Add, remove or modify widget files freely — loaded on startup
- Render order: Weekday (background) → Time → Date → StaticText
- For `type: "static"`, use the `static_text` field for fixed text
- Supported color formats: `#RRGGBB`, `#AARRGGBB`, `rgb(r,g,b)`, `rgba(r,g,b,a)`, named colors

## Acknowledgements

- [mpv](https://mpv.io/) — Video playback engine, used under LGPLv2.1+
- [nlohmann/json](https://github.com/nlohmann/json) — JSON parsing library, MIT License

## License

MIT License
