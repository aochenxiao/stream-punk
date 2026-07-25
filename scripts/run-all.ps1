# StreamPunk Run All Examples (Windows PowerShell)
# Usage: .\scripts\run-all.ps1

$RootDir = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RootDir "build"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  StreamPunk Examples" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$examples = @(
    @{Name="01-basic-cpp";       Exe="example-01-basic-cpp.exe";       Desc="C++ Serialization"},
    @{Name="02-cpp-to-ts";       Exe="example-02-cpp-to-ts.exe";       Desc="C++ <-> TypeScript"},
    @{Name="03-dynamic-schema";  Exe="example-03-dynamic-schema.exe";  Desc="Dynamic Schema"},
    @{Name="04-json";            Exe="example-04-json.exe";            Desc="JSON Serialization"},
    @{Name="05-orm";             Exe="example-05-orm.exe";             Desc="ORM SQL Generation"},
    @{Name="06-shadow-delta";    Exe="example-06-shadow-delta.exe";    Desc="Shadow Delta Update"}
)

$passed = 0
$failed = 0
$total = $examples.Count
$i = 0

foreach ($ex in $examples) {
    $i++
    $exePath = Join-Path (Join-Path (Join-Path $BuildDir "examples") $ex.Name) "Release"
    $exePath = Join-Path $exePath $ex.Exe
    Write-Host ("[{0}/{1}] {2} ... " -f $i, $total, $ex.Desc) -NoNewline

    # Also try Debug directory
    if (-not (Test-Path $exePath)) {
        $exePath = Join-Path (Join-Path (Join-Path $BuildDir "examples") $ex.Name) "Debug"
        $exePath = Join-Path $exePath $ex.Exe
    }
    if (-not (Test-Path $exePath)) {
        $exePath = Join-Path (Join-Path $BuildDir "examples") $ex.Name
        $exePath = Join-Path $exePath $ex.Exe
    }

    if (-not (Test-Path $exePath)) {
        Write-Host "SKIP (not built)" -ForegroundColor Yellow
        $failed++
        continue
    }

    $startTime = Get-Date
    & $exePath *> $null
    $exitCode = $LASTEXITCODE
    $elapsed = (Get-Date) - $startTime

    if ($exitCode -eq 0) {
        Write-Host "PASS (${elapsed}s)" -ForegroundColor Green
        $passed++
    }
    else {
        Write-Host "FAIL (exit code: $exitCode)" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Result: $passed passed, $failed failed, $total total" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan