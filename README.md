<h1 align="center">Dynamic Wallpaper</h1>

<h5 align="center">将视频设置为动态壁纸</h5>

<p align="center">
  <a href="README.md">简体中文</a>
  |
  <a href="README-en.md">English</a>
</p>

## 编译

### CMake（推荐）

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release
```

### 手动编译

```bash
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -Ithird_party/mpv/include main.cpp -o wallpaper.exe -Lthird_party/mpv/lib -lmpv -luser32 -lgdi32 -lshcore -ldwmapi -lole32 -mwindows -static-libgcc -static-libstdc++ '-Wl,-Bstatic' -lwinpthread '-Wl,-Bdynamic'
```

## 使用

1. 源码仓库中的 `libmpv-2.dll` 位于 `third_party/mpv/bin/`
2. 使用 CMake 构建后，`libmpv-2.dll` 会自动复制到 `wallpaper.exe` 所在输出目录
3. 运行程序时，只需保证 `wallpaper.exe`、`libmpv-2.dll` 和 `background.mp4` 位于同一目录
4. 首次运行会自动添加到开机自启动

## 配置

视频文件默认读取同目录下的 `background.mp4`，可自行替换

## 许可证
MIT License