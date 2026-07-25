# Example 09: SPOI Cross-Language Data Query - Run All Script (Windows PowerShell)
# Starts each language server in turn, runs all 8 language clients against it
# Test matrix: 8 servers x 8 clients = 64 language combinations

$ErrorActionPreference = "Continue"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = "$ScriptDir\build"
$ServerHost = "127.0.0.1"
$Port = 9999

# 设置控制台编码为 UTF-8，确保中文内容匹配正常
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  SPOI Cross-Language Data Query - 8x8 Full Test" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# ===== Clear residual processes =====
function Clear-Port {
    Write-Host "[Pre] Clearing port $Port residual processes..." -ForegroundColor Yellow
    $existing = netstat -ano | Select-String ":$Port" | ForEach-Object {
        ($_ -split '\s+' | Where-Object { $_ -match '^\d+$' } | Select-Object -Last 1)
    }
    foreach ($p in $existing) {
        taskkill /PID $p /F 2>$null
    }
    Start-Sleep -Seconds 1
}

# ===== Server definitions =====
$servers = @(
    @{
        Name = "C++"
        PreCheck = { Test-Path "$BuildDir\Release\spoi-cross-server-09.exe" }
        Start  = { Start-Process -FilePath "$BuildDir\Release\spoi-cross-server-09.exe" -NoNewWindow -PassThru }
    },
    @{
        Name = "Rust"
        PreCheck = { Test-Path "$ScriptDir\rust\target\release\spoi-cross-server-rust.exe" }
        Start  = { Start-Process -FilePath "$ScriptDir\rust\target\release\spoi-cross-server-rust.exe" -NoNewWindow -PassThru }
    },
    @{
        Name = "Go"
        PreCheck = { Test-Path "$BuildDir\go\spoi-cross-server-go.exe" }
        Start  = { Start-Process -FilePath "$BuildDir\go\spoi-cross-server-go.exe" -NoNewWindow -PassThru }
    },
    @{
        Name = "Java"
        PreCheck = { Test-Path "$BuildDir\java\Server.class" }
        Start  = { Start-Process -FilePath "java" -ArgumentList "-cp", "$BuildDir\java", "Server" -NoNewWindow -PassThru }
    },
    @{
        Name = "Kotlin"
        PreCheck = { Test-Path "$BuildDir\kotlin-server\ServerKt.class" }
        Start  = { Start-Process -FilePath "kotlin" -ArgumentList "-cp", "$BuildDir\kotlin-server", "ServerKt" -NoNewWindow -PassThru }
    },
    @{
        Name = "Python"
        PreCheck = { Test-Path "$ScriptDir\python\server.py" }
        Start  = { Start-Process -FilePath "python" -ArgumentList "$ScriptDir\python\server.py" -NoNewWindow -PassThru }
    },
    @{
        Name = "JavaScript"
        PreCheck = { Test-Path "$ScriptDir\js\server.js" }
        Start  = { Start-Process -FilePath "node" -ArgumentList "$ScriptDir\js\server.js" -NoNewWindow -PassThru }
    },
    @{
        Name = "TypeScript"
        PreCheck = { Test-Path "$BuildDir\ts\server.js" }
        Start  = { Start-Process -FilePath "node" -ArgumentList "$BuildDir\ts\server.js" -NoNewWindow -PassThru }
    }
)

# ===== Client definitions =====
$clients = @(
    @{
        Name = "C++"
        Exe = "$BuildDir\Release\spoi-cross-client-cpp.exe"
        Args = @()
        PreCheck = { Test-Path "$BuildDir\Release\spoi-cross-client-cpp.exe" }
    },
    @{
        Name = "Rust"
        Exe = "$ScriptDir\rust\target\release\spoi-cross-client-rust.exe"
        Args = @()
        PreCheck = { Test-Path "$ScriptDir\rust\target\release\spoi-cross-client-rust.exe" }
    },
    @{
        Name = "Go"
        Exe = "$BuildDir\go\spoi-cross-client-go.exe"
        Args = @()
        PreCheck = { Test-Path "$BuildDir\go\spoi-cross-client-go.exe" }
    },
    @{
        Name = "Java"
        Exe = "java"
        Args = @("-Dfile.encoding=UTF-8", "-cp", """$BuildDir\java""", "Main")
        PreCheck = { Test-Path "$BuildDir\java\Main.class" }
    },
    @{
        Name = "Kotlin"
        Exe = "C:\env\kotlinc\bin\kotlin.bat"
        Args = @("-cp", """$BuildDir\kotlin""", "MainKt")
        PreCheck = { Test-Path "$BuildDir\kotlin\MainKt.class" }
    },
    @{
        Name = "Python"
        Exe = "python"
        Args = @("-X", "utf8", """$ScriptDir\python\main.py""")
        PreCheck = { Test-Path "$ScriptDir\python\main.py" }
    },
    @{
        Name = "JavaScript"
        Exe = "node"
        Args = @("""$ScriptDir\js\client.js""")
        PreCheck = { Test-Path "$ScriptDir\js\client.js" }
    },
    @{
        Name = "TypeScript"
        Exe = "node"
        Args = @("""$BuildDir\ts\client.js""")
        PreCheck = { Test-Path "$BuildDir\ts\client.js" }
    }
)

# ===== Global stats =====
$globalTotal = 0
$globalPassed = 0
$globalFailed = 0
$globalSkipped = 0
$globalResults = @()

# ===== Main loop: iterate over each server =====
foreach ($server in $servers) {
    $serverName = $server.Name
    
    Write-Host ""
    Write-Host "############################################################" -ForegroundColor DarkGray
    Write-Host "  [$serverName Server] Starting tests" -ForegroundColor Magenta
    Write-Host "############################################################" -ForegroundColor DarkGray
    
    # Clear port
    Clear-Port
    
    # Check if server is compiled
    $preCheckOk = & $server.PreCheck
    if (-not $preCheckOk) {
        Write-Host "  [SKIP] $serverName server not compiled" -ForegroundColor Yellow
        foreach ($client in $clients) {
            $globalResults += [PSCustomObject]@{ Server = $serverName; Client = $client.Name; Status = "SKIP_SRV" }
            $globalSkipped++
        }
        continue
    }
    
    # Start server
    Write-Host "[START] Starting $serverName server..." -ForegroundColor Yellow
    $serverProcess = & $server.Start
    Start-Sleep -Seconds 2
    
    if ($serverProcess.HasExited) {
        Write-Host "  [ERROR] $serverName server failed to start!" -ForegroundColor Red
        foreach ($client in $clients) {
            $globalResults += [PSCustomObject]@{ Server = $serverName; Client = $client.Name; Status = "ERR_SRV" }
            $globalFailed++
        }
        continue
    }
    Write-Host "  $serverName server started (PID: $($serverProcess.Id))" -ForegroundColor Green
    
    # ===== Run all clients =====
    $serverPassed = 0
    $serverFailed = 0
    $serverSkipped = 0
    
    foreach ($client in $clients) {
        $clientName = $client.Name
        $globalTotal++
        
        Write-Host ""
        Write-Host "  ----------------------------------------" -ForegroundColor DarkGray
        Write-Host "  [$serverName -> $clientName] Client test" -ForegroundColor Cyan
        Write-Host "  ----------------------------------------" -ForegroundColor DarkGray
        
        # Check if client is compiled
        $clientPreCheckOk = & $client.PreCheck
        if (-not $clientPreCheckOk) {
            Write-Host "  [SKIP] $clientName client not compiled" -ForegroundColor Yellow
            $globalResults += [PSCustomObject]@{ Server = $serverName; Client = $clientName; Status = "SKIP_CLI" }
            $serverSkipped++
            $globalSkipped++
            continue
        }
        
        try {
            # 使用 Start-Process + 文件重定向捕获输出，彻底避免编码问题
            $tempOut = [System.IO.Path]::GetTempFileName()
            $tempErr = [System.IO.Path]::GetTempFileName()
            
            $argList = $client.Args
            if ($argList.Count -eq 0) {
                $proc = Start-Process -FilePath $client.Exe -NoNewWindow -Wait -RedirectStandardOutput $tempOut -RedirectStandardError $tempErr -PassThru
            } else {
                $proc = Start-Process -FilePath $client.Exe -ArgumentList $argList -NoNewWindow -Wait -RedirectStandardOutput $tempOut -RedirectStandardError $tempErr -PassThru
            }
            $exitCode = $proc.ExitCode
            
            # 以 UTF-8 编码读取输出文件
            $output = [System.IO.File]::ReadAllText($tempOut, [System.Text.Encoding]::UTF8)
            $errOutput = [System.IO.File]::ReadAllText($tempErr, [System.Text.Encoding]::UTF8)
            Remove-Item $tempOut, $tempErr -Force -ErrorAction SilentlyContinue
            
            if ($errOutput) { $output += "`n$errOutput" }
            
            # 内容校验：使用 Unicode 转义序列避免脚本文件编码问题
            $hasComplete = $output -match "\u6240\u6709\u67E5\u8BE2\u5B8C\u6210"  # 所有查询完成
            $hasUnknownType = $output -match "\u672A\u77E5\u7ED3\u679C\u7C7B\u578B"  # 未知结果类型
            $hasError = $output -match "连接失败|Connection refused"
            
            # DEBUG: 输出捕获信息
            Write-Host "    [DEBUG] output_len=$($output.Length) hasComplete=$hasComplete exitCode=$exitCode" -ForegroundColor DarkGray
            if (-not $hasComplete -and $output.Length -gt 0) {
                $tail = $output.Substring([Math]::Max(0, $output.Length - 150))
                Write-Host "    [DEBUG] tail: $tail" -ForegroundColor DarkGray
            }
            
            if ($exitCode -eq 0 -and $hasComplete -and -not $hasUnknownType -and -not $hasError) {
                Write-Host "  [$serverName -> $clientName] PASS" -ForegroundColor Green
                $globalResults += [PSCustomObject]@{ Server = $serverName; Client = $clientName; Status = "PASS" }
                $serverPassed++
                $globalPassed++
            } else {
                $reason = @()
                if ($exitCode -ne 0) { $reason += "exit=$exitCode" }
                if (-not $hasComplete) { $reason += "no_complete" }
                if ($hasUnknownType) { $reason += "unknown_type" }
                if ($hasError) { $reason += "conn_error" }
                $failReason = $reason -join ", "
                Write-Host "  [$serverName -> $clientName] FAIL ($failReason)" -ForegroundColor Red
                if ($output) {
                    $lines = $output -split "`n"
                    $showLines = [Math]::Min(5, $lines.Count)
                    for ($i = 0; $i -lt $showLines; $i++) {
                        Write-Host "    $($lines[$i])" -ForegroundColor DarkGray
                    }
                }
                $globalResults += [PSCustomObject]@{ Server = $serverName; Client = $clientName; Status = "FAIL_$failReason" }
                $serverFailed++
                $globalFailed++
            }
        } catch {
            Write-Host "  [$serverName -> $clientName] ERROR: $_" -ForegroundColor Red
            $globalResults += [PSCustomObject]@{ Server = $serverName; Client = $clientName; Status = "ERROR" }
            $serverFailed++
            $globalFailed++
        }
        
        Start-Sleep -Milliseconds 500
    }
    
    # Round summary
    Write-Host ""
    $roundColor = if ($serverFailed -eq 0) { "Green" } else { "Yellow" }
    Write-Host "  [$serverName Server] Round: PASS=$serverPassed FAIL=$serverFailed SKIP=$serverSkipped" -ForegroundColor $roundColor
    
    # Stop server
    Write-Host "  Stopping $serverName server..." -ForegroundColor Yellow
    Stop-Process -Id $serverProcess.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}

# ===== Final summary =====
Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Final Results (8x8 Matrix)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# Print table header
Write-Host ("{0,-12}" -f "Server\Client") -NoNewline
foreach ($client in $clients) {
    Write-Host ("{0,8}" -f $client.Name) -NoNewline
}
Write-Host ""
Write-Host ("-" * (12 + 8 * 8))

# Print each row
foreach ($server in $servers) {
    $serverName = $server.Name
    Write-Host ("{0,-12}" -f $serverName) -NoNewline
    foreach ($client in $clients) {
        $clientName = $client.Name
        $result = $globalResults | Where-Object { $_.Server -eq $serverName -and $_.Client -eq $clientName } | Select-Object -First 1
        if ($result) {
            $symbol = $result.Status
            if ($symbol.Length -gt 8) { $symbol = $symbol.Substring(0, 8) }
            $color = switch ($result.Status) {
                "PASS" { "Green" }
                "FAIL" { "Red" }
                "ERROR" { "Red" }
                default { "DarkGray" }
            }
            Write-Host ("{0,8}" -f $symbol) -NoNewline -ForegroundColor $color
        } else {
            Write-Host ("{0,8}" -f "?") -NoNewline -ForegroundColor DarkGray
        }
    }
    Write-Host ""
}

Write-Host ""
$finalColor = if ($globalFailed -eq 0) { "Green" } else { "Yellow" }
Write-Host "PASS: $globalPassed / FAIL: $globalFailed / SKIP: $globalSkipped / Total: $globalTotal" -ForegroundColor $finalColor
Write-Host ""
Write-Host "=== All tests completed ===" -ForegroundColor Cyan