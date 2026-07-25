# StreamWorms — 生成 JS 端代码
# 从 C++ 类型定义生成 JavaScript 等价类型和 SPOI Builder

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$GeneratedDir = "$ScriptDir\client\src\generated"

Write-Host "=== StreamWorms — 生成 JS 代码 ===" -ForegroundColor Cyan

# 确保输出目录存在
New-Item -ItemType Directory -Force -Path $GeneratedDir | Out-Null

# TODO: 集成 sp-gen 工具生成 JS 代码
# 目前手动复制运行时文件

# 复制 StreamPunk JS 运行时
$RuntimeDir = "$ScriptDir\..\..\runtimes\js"
if (Test-Path $RuntimeDir) {
    Copy-Item "$RuntimeDir\stream-punk.js" "$GeneratedDir\" -Force
    Copy-Item "$RuntimeDir\spoi_builder.js" "$GeneratedDir\" -Force
    Copy-Item "$RuntimeDir\spoi_executor.js" "$GeneratedDir\" -Force
    Write-Host "  已复制 JS 运行时文件" -ForegroundColor Green
} else {
    Write-Host "  警告: 未找到 JS 运行时目录: $RuntimeDir" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== 完成 ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "下一步: cd client && npm install && npm run dev" -ForegroundColor Green