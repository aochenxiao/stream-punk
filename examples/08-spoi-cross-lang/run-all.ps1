# 示例 08：SPOI 跨语言数据互查 — 一键运行脚本 (Windows PowerShell)
# 启动 C++ 服务器，然后运行 Java 客户端

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = "$ScriptDir\build"

Write-Host "=== SPOI 跨语言数据互查 — 一键运行 ===" -ForegroundColor Cyan
Write-Host ""

# ===== 1. 检查 C++ 服务器是否存在 =====
$ServerExe = "$BuildDir\Release\spoi-cross-server.exe"
if (-not (Test-Path $ServerExe)) {
    Write-Host "[错误] C++ 服务器未找到: $ServerExe" -ForegroundColor Red
    Write-Host "请先运行 setup.ps1 编译项目" -ForegroundColor Yellow
    exit 1
}

# ===== 2. 检查 Java 客户端是否存在 =====
$JavaClassDir = "$BuildDir\java"
if (-not (Test-Path "$JavaClassDir\Main.class")) {
    Write-Host "[错误] Java 客户端未找到: $JavaClassDir\Main.class" -ForegroundColor Red
    Write-Host "请先运行 setup.ps1 编译项目" -ForegroundColor Yellow
    exit 1
}

# ===== 3. 清理可能残留的进程 =====
Write-Host "[1/3] 清理残留进程..." -ForegroundColor Yellow
$existing = netstat -ano | Select-String ":9999" | ForEach-Object {
    $parts = $_ -split '\s+' | Where-Object { $_ -match '^\d+$' } | Select-Object -Last 1
    $parts
}
foreach ($pid in $existing) {
    taskkill /PID $pid /F 2>$null
}
Start-Sleep -Seconds 1

# ===== 4. 启动 C++ 服务器 =====
Write-Host "[2/3] 启动 C++ 服务器..." -ForegroundColor Yellow
$ServerProcess = Start-Process -FilePath $ServerExe -NoNewWindow -PassThru
Start-Sleep -Seconds 2

if ($ServerProcess.HasExited) {
    Write-Host "[错误] C++ 服务器启动失败！" -ForegroundColor Red
    exit 1
}
Write-Host "  C++ 服务器已启动 (PID: $($ServerProcess.Id))" -ForegroundColor Green

# ===== 5. 运行 Java 客户端 =====
Write-Host "[3/3] 运行 Java 客户端..." -ForegroundColor Yellow
Write-Host ""
try {
    Push-Location "$ScriptDir\client"
    java -cp "$JavaClassDir" Main
    Pop-Location
} catch {
    Write-Host "[错误] Java 客户端运行失败: $_" -ForegroundColor Red
    Pop-Location
}

# ===== 6. 停止服务器 =====
Write-Host ""
Write-Host "正在停止 C++ 服务器..." -ForegroundColor Yellow
Stop-Process -Id $ServerProcess.Id -Force -ErrorAction SilentlyContinue
Write-Host "=== 运行完成 ===" -ForegroundColor Cyan