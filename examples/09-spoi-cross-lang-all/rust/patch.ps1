$path = "d:\dev\project\aochenxiao\stream-punk\examples\09-spoi-cross-lang-all\rust\client.rs"
$txt = [IO.File]::ReadAllText($path)
$m = 'println!("=== 所有查询完成 ===");'
$ins = [IO.File]::ReadAllText("d:\dev\project\aochenxiao\stream-punk\examples\09-spoi-cross-lang-all\rust\insert.txt")
$txt = $txt.Replace($m, $ins + $m)
[IO.File]::WriteAllText($path, $txt)
Write-Output "OK"