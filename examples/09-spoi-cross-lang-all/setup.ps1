# 示例 09：SPOI 全语言跨语言数据互查 — 一键编译脚本 (Windows PowerShell)
# 编译所有语言的服务器和客户端

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = "$ScriptDir\build"

Write-Host "=== SPOI 全语言跨语言数据互查 — 编译 ===" -ForegroundColor Cyan
Write-Host ""

# 创建构建目录
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

# ===== 1. C++ 项目（服务器 + 客户端）=====
Write-Host "[1/8] 编译 C++ 项目（服务器 + 客户端）..." -ForegroundColor Yellow
Push-Location $ScriptDir
& cmake -B build -G "Visual Studio 17 2022" -A x64 2>&1 | Out-Null
$MSBuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $MSBuild "$BuildDir\example-09-spoi-cross-lang-all.sln" /p:Configuration=Release /p:Platform=x64 /t:Build /v:minimal
Pop-Location
Write-Host "  C++ 编译完成" -ForegroundColor Green

# ===== 2. Java 服务器 + 客户端 =====
Write-Host "[2/8] 编译 Java 服务器 + 客户端..." -ForegroundColor Yellow
$JavaOutDir = "$BuildDir\java"
New-Item -ItemType Directory -Force -Path $JavaOutDir | Out-Null
Push-Location "$ScriptDir\java"
javac -encoding UTF-8 -d $JavaOutDir Main.java Server.java
Pop-Location
Write-Host "  Java 编译完成" -ForegroundColor Green

# ===== 3. Kotlin 服务器 + 客户端（分开编译，避免常量冲突）=====
Write-Host "[3/8] 编译 Kotlin 客户端..." -ForegroundColor Yellow
$KotlinOutDir = "$BuildDir\kotlin"
New-Item -ItemType Directory -Force -Path $KotlinOutDir | Out-Null
Push-Location "$ScriptDir\kotlin"
kotlinc -d $KotlinOutDir Main.kt
Pop-Location
Write-Host "  Kotlin 客户端编译完成" -ForegroundColor Green

Write-Host "  [3b] 编译 Kotlin 服务器..." -ForegroundColor Yellow
$KotlinServerOutDir = "$BuildDir\kotlin-server"
New-Item -ItemType Directory -Force -Path $KotlinServerOutDir | Out-Null
Push-Location "$ScriptDir\kotlin"
kotlinc -d $KotlinServerOutDir Server.kt
Pop-Location
Write-Host "  Kotlin 服务器编译完成" -ForegroundColor Green

# ===== 4. Go 服务器 + 客户端 =====
Write-Host "[4/8] 编译 Go 服务器 + 客户端..." -ForegroundColor Yellow
$GoOutDir = "$BuildDir\go"
New-Item -ItemType Directory -Force -Path $GoOutDir | Out-Null
Push-Location "$ScriptDir\go"
go build -o "$GoOutDir\spoi-cross-client-go.exe" client.go
go build -o "$GoOutDir\spoi-cross-server-go.exe" server.go
Pop-Location
Write-Host "  Go 编译完成" -ForegroundColor Green

# ===== 5. Rust 服务器 + 客户端 =====
Write-Host "[5/8] 编译 Rust 服务器 + 客户端..." -ForegroundColor Yellow
Push-Location "$ScriptDir\rust"
$prevErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"
cargo build --release 2>&1 | Out-Null
$ErrorActionPreference = $prevErrorAction
Pop-Location
Write-Host "  Rust 编译完成" -ForegroundColor Green

# ===== 6. TypeScript 服务器 + 客户端 =====
Write-Host "[6/8] 编译 TypeScript 服务器 + 客户端..." -ForegroundColor Yellow
Push-Location "$ScriptDir\ts"
tsc
Pop-Location
Write-Host "  TypeScript 编译完成" -ForegroundColor Green

# ===== 7. Python 无需编译 =====
Write-Host "[7/8] Python 无需编译" -ForegroundColor Yellow

# ===== 8. JavaScript 无需编译 =====
Write-Host "[8/8] JavaScript 无需编译" -ForegroundColor Yellow

Write-Host ""
Write-Host "=== 编译完成 ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "运行方式: .\run-all.ps1" -ForegroundColor Green