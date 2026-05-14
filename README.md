<h1 align="center">Dynamic Wallpaper</h1>

<h3 align="center">将视频设置为动态壁纸</h3>

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

1. 运行程序时，只需保证 `wallpaper.exe`、`libmpv-2.dll` 和 `background.mp4` 位于同一目录
2. 首次运行会自动添加到开机自启动
3. 旧用户不需要清除旧的注册表开机自启动项，程序会自动更新

## 配置

视频文件默认读取同目录下的 `background.mp4`，可自行替换

## 许可证
MIT License