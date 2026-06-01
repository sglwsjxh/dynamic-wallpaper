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
        "enabled": true,
        "widgets_dir": "public/widgets",
        "order": [
            "time",
            "date",
            "weekday",
            "audio_spectrum"
        ]
    },
    "audio": {
        "enabled": true,
        "widget_id": "audio_spectrum"
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
| `desktop_overlay.order` | string[] | Widget draw order, top to bottom, matched by id, first element is topmost |
| `audio.enabled` | bool | Enable audio spectrum capture and analysis |
| `audio.widget_id` | string | Audio spectrum widget id (must match an entry in overlay order) |

### Overlay (Desktop Text)

The app can render custom overlays on the desktop. By default it shows a clock, date, weekday background, and audio spectrum.

Each widget is defined as a `.json` file in `public/widgets/`, one layer per file.
The repository tracks `.example.json` template files instead. Build scripts automatically copy them to `.json`, so your local configuration won't be overwritten by git.

**Default widgets:**

| File | Content | Default Style |
|------|---------|---------------|
| `weekday.json` | Weekday background (e.g. Monday) | Segoe Script 140pt, semi-transparent decorative text |
| `time.json` | Clock (e.g. 14:30) | Segoe UI 95pt bold, centered |
| `date.json` | Date (e.g. 1 June) | Segoe UI 36pt bold, centered |
| `audio_spectrum.json` | Audio spectrum visualization | WASAPI loopback capture + FFT analysis, bar rendering |

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

- Add, remove or modify widget files, then restart to apply
- Draw order is controlled by the `order` array in `config.json` — first element is topmost, last element is bottommost
- For `type: "static"`, use the `static_text` field for fixed text
- For `type: "audio_spectrum"`, renders audio spectrum bars with these extra style parameters:

| Field | Type | Description |
|-------|------|-------------|
| `bands` | int | Number of spectrum bars (default 36) |
| `gap` | float | Gap between bars (px) |
| `min_height` | float | Minimum bar height (px) |
| `max_height` | float | Maximum bar height (px) |
| `sensitivity` | float | Sensitivity (higher = more responsive) |
| `smoothing` | float | Smoothing factor (0~1, higher = smoother but slower) |
| `color` | string | Bar color (#AARRGGBB format) |

- Supported color formats: `#RRGGBB`, `#AARRGGBB`, `rgb(r,g,b)`, `rgba(r,g,b,a)`, named colors

## Acknowledgements

- [mpv](https://mpv.io/) — Video playback engine, used under LGPLv2.1+
- [nlohmann/json](https://github.com/nlohmann/json) — JSON parsing library, MIT License
- [KISS FFT](https://github.com/mborgerding/kissfft) — Fast Fourier Transform library, BSD-3-Clause License

## License

MIT License
