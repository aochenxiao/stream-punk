# 示例 08：SPOI 跨语言数据互查 — 环境准备脚本 (Windows PowerShell)
# 编译 C++ 服务器和 Java 客户端

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path "$ScriptDir\..\.."

Write-Host "=== SPOI 跨语言数据互查 — 环境准备 ===" -ForegroundColor Cyan
Write-Host ""

# ===== 1. 编译 C++ 服务器 =====
Write-Host "[1/2] 编译 C++ 服务器..." -ForegroundColor Yellow

$MSBuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
$BuildDir = "$ScriptDir\build"

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Push-Location $BuildDir
try {
    & cmake .. -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake 配置失败"
    }
    & $MSBuild example-08-spoi-cross-lang.sln /p:Configuration=Release /p:Platform=x64 /t:spoi-cross-server
    if ($LASTEXITCODE -ne 0) {
        throw "C++ 编译失败"
    }
    Write-Host "  C++ 服务器编译成功！" -ForegroundColor Green
} finally {
    Pop-Location
}

# ===== 2. 编译 Java 客户端 =====
Write-Host "[2/2] 编译 Java 客户端..." -ForegroundColor Yellow

$JavaClientDir = "$ScriptDir\client"
$JavaOutDir = "$ScriptDir\build\java"

if (-not (Test-Path $JavaOutDir)) {
    New-Item -ItemType Directory -Path $JavaOutDir | Out-Null
}

Push-Location $JavaClientDir
try {
    & javac -encoding UTF-8 -d $JavaOutDir Main.java
    if ($LASTEXITCODE -ne 0) {
        throw "Java 编译失败"
    }
    Write-Host "  Java 客户端编译成功！" -ForegroundColor Green
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== 环境准备完成！===" -ForegroundColor Cyan
Write-Host ""
Write-Host "运行方式：" -ForegroundColor White
Write-Host "  1. 启动 C++ 服务器：  .\build\Release\spoi-cross-server.exe" -ForegroundColor Gray
Write-Host "  2. 启动 Java 客户端： java -cp build\java Main" -ForegroundColor Gray
Write-Host ""
Write-Host "或使用一键运行脚本：  .\run-all.ps1" -ForegroundColor Gray