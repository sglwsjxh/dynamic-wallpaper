$ErrorActionPreference = "Stop"

cmake -S . -B build -G "MinGW Makefiles"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& "./build/wallpaper.exe"