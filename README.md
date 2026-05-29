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

1. 运行 `wallpaper.exe`，确保 `libmpv-2.dll` 和 `config.json` 在同一目录
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
    }
}
```

- `auto_startup`：是否开机自启
- `enabled`：是否启动壁纸
- `background_path`：视频文件路径

## 致谢

- [mpv](https://mpv.io/) — 视频播放核心引擎，基于 LGPLv2.1+ 许可使用
- [nlohmann/json](https://github.com/nlohmann/json) — JSON 解析库，MIT 许可使用

## 许可证

MIT License
