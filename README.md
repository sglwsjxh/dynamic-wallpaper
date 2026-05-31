<h1 align="center">Dynamic Wallpaper · 将视频设置为动态壁纸</h1>

<p align="center">
  <a href="README.md">简体中文</a>
  |
  <a href="README-en.md">English</a>
</p>

## 编译

### CMake

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release
```

> 也可以直接运行 `.\build_and_run.ps1` 一键构建并启动

## 使用

1. 运行 `wallpaper.exe`，确保 `libmpv-2.dll`、`config.json` 和 `public/` 在同一目录
2. 修改 `config.json` 中的 `background_path` 指定视频路径（默认 `background.mp4`）
3. 设置 `wallpaper_enabled` 为 `false` 可临时关闭壁纸，`auto_startup` 控制开机自启
4. 程序启动后会在系统托盘显示图标

### 配置说明

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
            "weekday"
        ]
    }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `auto_startup` | bool | 是否开机自启 |
| `dynamic_wallpaper.enabled` | bool | 是否启动壁纸 |
| `dynamic_wallpaper.background_path` | string | 视频文件路径 |
| `desktop_overlay.enabled` | bool | 是否启用桌面文字叠加（时钟、日期等） |
| `desktop_overlay.widgets_dir` | string | widget JSON 目录（默认 `public/widgets`，一般不需要改） |
| `desktop_overlay.order` | string[] | widget 绘制顺序，从顶层到底层排列，按 id 匹配，数组中第一个在顶层 |

### Overlay 叠加层（桌面文字）

在桌面上显示自定义文字叠加层，默认显示时钟、日期和星期背景

每个 widget 是一个 `.json` 文件，放在 `public/widgets/` 目录下，一个文件对应一个图层

**默认 widget：**

| 文件 | 显示内容 | 默认样式 |
|------|----------|----------|
| `weekday.json` | 星期背景（如 Monday） | Segoe Script 140pt，半透明装饰文字 |
| `time.json` | 时钟（如 14:30） | Segoe UI 95pt 粗体，居中 |
| `date.json` | 日期（如 1 June） | Segoe UI 36pt 粗体，居中 |

**可定制的字段：**

```json
{
    "id": "time",
    "type": "time",         // time / date / weekday / static
    "enabled": true,
    "position": {
        "x_percent": 50.0,  // 矩形框左边缘 X（屏幕百分比）
        "y_percent": 38.0,  // 矩形框上边缘 Y（屏幕百分比）
        "width_percent": 40.0,   // 矩形框宽（屏幕百分比）
        "height_percent": 14.0,  // 矩形框高（屏幕百分比）
        "anchor": "center"       // top_left / top_center / center / bottom_center
    },
    "style": {
        "font_family": "Segoe UI",
        "font_size": 95,
        "font_style": "regular",  // regular / bold / italic / bold_italic
        "color": "#F0FFFFFF",     // #AARRGGBB 或 #RRGGBB
        "align": "center",        // near / center / far（水平对齐）
        "line_align": "center",   // near / center / far（垂直对齐）
        "shadow": {
            "enabled": true,
            "offset_x": 2,
            "offset_y": 2,
            "color": "#66000000"
        }
    }
}
```

- 添加、删除或修改 widget 文件后，重启程序即可生效
- 绘制顺序由 `config.json` 的 `order` 数组控制，第一个元素在最顶层，最后一个在最底层
- `type` 为 `static` 时，用 `static_text` 字段显示固定文字
- 支持的颜色格式：`#RRGGBB`、`#AARRGGBB`、`rgb(r,g,b)`、`rgba(r,g,b,a)`、基本颜色名

## 致谢

- [mpv](https://mpv.io/) — 视频播放核心引擎，基于 LGPLv2.1+ 许可使用
- [nlohmann/json](https://github.com/nlohmann/json) — JSON 解析库，MIT 许可使用

## 许可证

MIT License
