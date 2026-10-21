param(
    [string]$Config = "Release",
    [string]$BuildDir = "build",
    [string]$OrtDir = "D:\rppg_deps\onnxruntime-win-x64-1.20.1",
    [string]$OpenCVDir = "D:\rppg_deps\opencv\build",
    [string]$VsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community",
    [string]$CMake = "C:\Users\Daniel\tools\cmake-3.30.5-windows-x86_64\bin\cmake.exe",
    [switch]$Clean,
    [switch]$Test
)
# Configures and builds the engine with MSVC + Ninja on Windows.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$vcvars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2]) }
}
$bdir = Join-Path $root $BuildDir
if ($Clean -and (Test-Path $bdir)) { Remove-Item -Recurse -Force $bdir }
& $CMake -S $root -B $bdir -G Ninja "-DCMAKE_BUILD_TYPE=$Config" "-DRPPG_ONNXRUNTIME_DIR=$OrtDir" "-DOpenCV_DIR=$OpenCVDir"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
& $CMake --build $bdir --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed" }
if ($Test) {
    Push-Location $bdir
    try { & ctest --output-on-failure } finally { Pop-Location }
}
