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

## 使用

1. 运行程序时，只需保证 `wallpaper.exe`、`libmpv-2.dll` 和 `background.mp4` 位于同一目录
2. 首次运行会自动添加到开机自启动
3. 旧用户不需要清除旧的注册表开机自启动项，程序会自动更新

## 配置

视频文件默认读取同目录下的 `background.mp4`，可自行替换

## 许可证

MIT License

## 致谢

- [mpv](https://mpv.io/) — 视频播放核心引擎，基于 LGPLv2.1+ 许可使用