# Generate TypeScript code from C++ metadata
Write-Host "=== Generating TypeScript code for collab-editor ===" -ForegroundColor Cyan

# Build meta_extractor
Push-Location $PSScriptRoot
try {
    cmake -B build -S .
    cmake --build build --config Release
} finally {
    Pop-Location
}

# Run meta_extractor
$metaExe = "$PSScriptRoot\build\Release\meta_extractor.exe"
if (Test-Path $metaExe) {
    Push-Location $PSScriptRoot
    try {
        & $metaExe
        Write-Host "Meta extraction done" -ForegroundColor Green
    } finally {
        Pop-Location
    }
}

# Run sp-gen
$metaFile = "$PSScriptRoot\temp\stream-punk-meta.bin"
if (Test-Path $metaFile) {
    $outFile = "$PSScriptRoot\client\src\stream-punk-data.ts"
    if (Test-Path $outFile) { Remove-Item $outFile }
    sp-gen -t ts-meta -p $outFile -m "$metaFile"
    Write-Host "TypeScript code generated!" -ForegroundColor Green
} else {
    Write-Host "Meta file not found!" -ForegroundColor Red
}