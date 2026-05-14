<h1 align="center">Dynamic Wallpaper</h1>

<h3 align="center">Set video as Windows desktop wallpaper</h3>

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

1. When running the program, make sure `wallpaper.exe`, `libmpv-2.dll`, and `background.mp4` are in the same directory
2. On first launch, the program will add itself to startup automatically
3. Existing users do not need to clear the old registry startup entry, the program will update it automatically

## Configuration

The video file is read from `background.mp4` in the same directory by default. You can replace it with your own video file.

## License

MIT License