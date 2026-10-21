param(
    [string]$Cache = "D:\rppg_cache\mcd",
    [string]$Out = "D:\rppg_cache\sweep.csv",
    [string]$Bench = "C:\Users\Daniel\source\rPPG\build_eval\apps\rppg_bench.exe",
    [string]$Dataset = "mcd",
    [int]$Jobs = 4,
    [string[]]$Configs = @(),   # entries "name|--opt value --opt value"
    [switch]$Stream
)
# Evaluates analysis configurations on cached sessions (no video decoding).
foreach ($entry in $Configs) {
    $name, $opts = $entry.Split("|", 2)
    $a = @("eval", "--cache", $Cache, "--dataset", $Dataset, "--name", $name, "--out", $Out, "--jobs", "$Jobs")
    if ($opts) { $a += $opts.Trim() -split "\s+" }
    if (-not $Stream) { $a += "--no-stream" }
    & $Bench @a
}
