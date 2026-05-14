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

## Usage

1. When running the program, make sure `wallpaper.exe`, `libmpv-2.dll`, and `background.mp4` are in the same directory
2. On first launch, the program will add itself to startup automatically
3. Existing users do not need to clear the old registry startup entry, the program will update it automatically

## Configuration

The video file is read from `background.mp4` in the same directory by default. You can replace it with your own video file.

## License

MIT License