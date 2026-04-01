<h1 align="center">Dynamic Wallpaper</h1>

<p align="center">
  <a href="README.md">简体中文</a>
  |
  <a href="README-en.md">English</a>
</p>

Set video as Windows desktop wallpaper

## Build

```bash
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -I. main.cpp -o wallpaper.exe -L. -lmpv -luser32 -lgdi32 -lshcore -ldwmapi -lole32 -mwindows
```

## Usage

1. Place `wallpaper.exe` and `libmpv-2.dll` in the same directory
2. Put `background.mp4` in the same directory
3. Run the program
4. First run will add itself to startup automatically<h1 align="center">Dynamic Wallpaper</h1>