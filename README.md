<h1 align="center">Dynamic Wallpaper</h1>

<p align="center">
  <a href="README.md">简体中文</a>
  |
  <a href="README-en.md">English</a>
</p>

将视频设为 Windows 桌面壁纸

## 编译

```bash
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -I. main.cpp -o wallpaper.exe -L. -lmpv -luser32 -lgdi32 -lshcore -ldwmapi -lole32 -mwindows
```

## 使用

1. 把 `wallpaper.exe` 和 `libmpv-2.dll` 放到同一目录
2. 同目录下放置 `background.mp4` 作为视频文件
3. 运行程序
4. 首次运行会自动添加到开机自启动

## 配置

视频文件默认读取同目录下的 `background.mp4`，可自行替换