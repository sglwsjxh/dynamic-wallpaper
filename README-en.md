<h1 align="center">Dynamic Wallpaper</h1>

<h5 align="center">Set video as Windows desktop wallpaper</h5>

<p align="center">
  <a href="README.md">简体中文</a>
  |
  <a href="README-en.md">English</a>
</p>

## Build

### CMake (recommended)

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release
```

### Manual g++ build

```bash
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -Ithird_party/mpv/include main.cpp -o wallpaper.exe -Lthird_party/mpv/lib -lmpv -luser32 -lgdi32 -lshcore -ldwmapi -lole32 -mwindows -static-libgcc -static-libstdc++ '-Wl,-Bstatic' -lwinpthread '-Wl,-Bdynamic'
```

## Usage

1. In the repository, `libmpv-2.dll` is stored under `third_party/mpv/bin/`
2. When building with CMake, the DLL is copied automatically to the same output directory as `wallpaper.exe`
3. When running the program, make sure `wallpaper.exe`, `libmpv-2.dll`, and `background.mp4` are in the same directory
4. On first launch, the program will add itself to startup automatically

## Configuration

The video file is read from `background.mp4` in the same directory by default. You can replace it with your own video file.

## License

MIT License