cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release
./build/wallpaper.exe