$ErrorActionPreference = "Stop"

$processName = "wallpaper"

Get-Process -Name $processName -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "Stopping existing wallpaper process: PID=$($_.Id)"
    Stop-Process -Id $_.Id -Force
}

Start-Sleep -Milliseconds 300

cmake -S . -B build -G "MinGW Makefiles"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& "./build/wallpaper.exe"