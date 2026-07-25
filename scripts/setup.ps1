param([switch]$Clean)

$RootDir = Split-Path -Parent $PSScriptRoot

if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    $buildDir = Join-Path $RootDir "build"
    if (Test-Path $buildDir) {
        Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
        if (Test-Path $buildDir) {
            Write-Host "  [WARN] Cannot fully remove build directory (files in use)." -ForegroundColor Yellow
            Write-Host "  Attempting to clean CMake cache..." -ForegroundColor Yellow
            Remove-Item -Force (Join-Path $buildDir "CMakeCache.txt") -ErrorAction SilentlyContinue
            Remove-Item -Recurse -Force (Join-Path $buildDir "CMakeFiles") -ErrorAction SilentlyContinue
        }
        else {
            Write-Host "  Build directory cleaned." -ForegroundColor Green
        }
    }
    Write-Host ""
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  StreamPunk Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "[1/5] Checking CMake..." -ForegroundColor Yellow
$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if ($null -eq $cmakeCmd) { Write-Host "  [ERROR] CMake not found" -ForegroundColor Red; exit 1 }
Write-Host "  CMake found" -ForegroundColor Green
Write-Host "[2/5] Checking C++ compiler..." -ForegroundColor Yellow
$msbuild = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
if ($null -eq $msbuild) {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -products * -property installationPath
        $msbuildPath = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path $msbuildPath) { $msbuild = $msbuildPath }
    }
}
$useVS = $false
if ($msbuild) { Write-Host "  MSBuild found" -ForegroundColor Green; $useVS = $true }
if (-not $useVS) { Write-Host "  [WARN] MSBuild not found" -ForegroundColor Yellow }

# [3/5] 安装依赖
Write-Host "[3/5] Installing dependencies..." -ForegroundColor Yellow
$legacy3rd = Join-Path $RootDir "3/x64-windows/include/doctest/doctest.h"
$cmakeExtraArgs = @()
if (Test-Path $legacy3rd) {
    Write-Host "  Using legacy 3/ directory" -ForegroundColor Green
} else {
    # 3/ 目录不存在，使用 vcpkg
    $vcpkgRoot = $env:VCPKG_ROOT
    if (-not $vcpkgRoot) { $vcpkgRoot = Join-Path $env:USERPROFILE "vcpkg" }
    $vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
    $vcpkgToolchain = Join-Path $vcpkgRoot "scripts/buildsystems/vcpkg.cmake"
    if (-not (Test-Path $vcpkgExe)) {
        Write-Host "  [ERROR] vcpkg not found." -ForegroundColor Red
        Write-Host "  Please install vcpkg:" -ForegroundColor Yellow
        Write-Host "    git clone https://github.com/Microsoft/vcpkg.git" -ForegroundColor Yellow
        Write-Host "    .\vcpkg\bootstrap-vcpkg.bat" -ForegroundColor Yellow
        Write-Host "  Or set VCPKG_ROOT environment variable." -ForegroundColor Yellow
        exit 1
    }
    Write-Host "  vcpkg found: $vcpkgExe" -ForegroundColor Green
    Write-Host "  Running vcpkg install (this may take a while on first run)..." -ForegroundColor Yellow
    Push-Location $RootDir
    $vcpkgResult = & $vcpkgExe install --triplet x64-windows 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host $vcpkgResult
        Write-Host "  [ERROR] vcpkg install failed" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    Pop-Location
    Write-Host "  Dependencies installed" -ForegroundColor Green
    $cmakeExtraArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
}

# [4/5] 构建 sp-gen
Write-Host "[4/5] Building sp-gen..." -ForegroundColor Yellow
$buildDir = Join-Path $RootDir "build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Push-Location $buildDir
if ($useVS) {
    cmake .. -G "Visual Studio 17 2022" -A x64 --fresh @cmakeExtraArgs
    if ($LASTEXITCODE -ne 0) { Pop-Location; exit 1 }
    cmake --build . --target sp-gen --config Release
    if ($LASTEXITCODE -ne 0) { Pop-Location; exit 1 }
}
else {
    cmake .. -DCMAKE_BUILD_TYPE=Release --fresh @cmakeExtraArgs
    if ($LASTEXITCODE -ne 0) { Pop-Location; exit 1 }
    cmake --build . --target sp-gen
    if ($LASTEXITCODE -ne 0) { Pop-Location; exit 1 }
}
Pop-Location
Write-Host "  sp-gen built" -ForegroundColor Green

# [5/5] 构建 examples
Write-Host "[5/5] Building examples..." -ForegroundColor Yellow
Push-Location $buildDir
$targets = @("example-01-basic-cpp","example-02-cpp-to-ts","example-03-dynamic-schema","example-04-json","example-05-orm","example-06-shadow-delta")
foreach ($t in $targets) {
    if ($useVS) { cmake --build . --target $t --config Release }
    else { cmake --build . --target $t }
}
Pop-Location
Write-Host "  All examples built" -ForegroundColor Green
Write-Host ""
Write-Host "  Setup complete! Run .\scripts\run-all.ps1 to test" -ForegroundColor Cyan