param(
    [string]$Bench = "C:\Users\Daniel\source\rPPG\build\apps\rppg_bench.exe",
    [string]$Models = "C:\Users\Daniel\source\rPPG\models",
    [string]$Out = "C:\Users\Daniel\source\rPPG\docs\results\raw\robustness.csv",
    [int]$Jobs = 10
)
# Robustness study: real UBFC-rPPG videos (10 subjects, first 60 s) under controlled perturbations.
$subs = "subject20,subject22,subject23,subject24,subject25,subject26,subject27,subject30,subject31,subject32"
$conds = [ordered]@{
    "clean"        = @()
    "h264_crf18"   = @("--video-dir", "D:\rppg_data\aug\crf18")
    "h264_crf23"   = @("--video-dir", "D:\rppg_data\aug\crf23")
    "h264_crf28"   = @("--video-dir", "D:\rppg_data\aug\crf28")
    "h264_crf35"   = @("--video-dir", "D:\rppg_data\aug\crf35")
    "lowlight_0.25" = @("--augment", "dark:0.25")
    "melanin_1.0"  = @("--augment", "melanin:1.0")
    "noise_8"      = @("--augment", "noise:8")
    "flicker_3pct" = @("--augment", "flicker:0.03")
    "shake_15px"   = @("--augment", "shake:15")
    "shake_30px"   = @("--augment", "shake:30")
    "lowres_160"   = @("--augment", "lowres:160")
}
Remove-Item $Out -ErrorAction SilentlyContinue
$sweep = Join-Path $PSScriptRoot "sweep.ps1"
foreach ($c in $conds.Keys) {
    $cache = "D:\rppg_cache\aug\$c"
    $a = @("capture", "--dataset", "ubfc", "--root", "D:\rppg_data\ubfc_rppg", "--cache", $cache, "--models", $Models,
           "--jobs", "$Jobs", "--filter", $subs, "--max-seconds", "60") + $conds[$c]
    & $Bench @a | Select-Object -Last 1
    & $sweep -Bench $Bench -Cache $cache -Dataset $c -Out $Out -Jobs 8 -Stream -Configs @(
        "engine|", "engine-lite|--neural facephys", "classical-only|--neural none")
    & $sweep -Bench $Bench -Cache $cache -Dataset $c -Out $Out -Jobs 8 -Configs @(
        "methods|--methods CHROM,POS,LGI,OMIT,PRISM --multi-roi 1 --neural facephys,me --classical-prior 1")
}
