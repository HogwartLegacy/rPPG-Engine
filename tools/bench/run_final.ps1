param(
    [string]$Bench = "C:\Users\Daniel\source\rPPG\build\apps\rppg_bench.exe",
    [string]$Out = "C:\Users\Daniel\source\rPPG\docs\results\raw",
    [int]$Jobs = 8
)
# Final benchmark: per-method baselines, engine presets (incl. real-time protocol) and ablations.
New-Item -ItemType Directory -Force $Out | Out-Null
$sweep = Join-Path $PSScriptRoot "sweep.ps1"
$all = "--methods GREEN,ICA,PCA,CHROM,POS,LGI,PBV,OMIT,2SR,PRISM --multi-roi 1 --neural facephys,me --classical-prior 1"
foreach ($ds in @("ubfc", "mcd")) {
    $cache = "D:\rppg_cache\$ds"
    $csv = Join-Path $Out "$ds.csv"
    Remove-Item $csv -ErrorAction SilentlyContinue
    & $sweep -Bench $Bench -Cache $cache -Dataset $ds -Out $csv -Jobs $Jobs -Configs @("methods|$all")
    & $sweep -Bench $Bench -Cache $cache -Dataset $ds -Out $csv -Jobs $Jobs -Stream -Configs @(
        "engine|",
        "engine-lite|--neural facephys",
        "classical-only|--neural none")
    & $sweep -Bench $Bench -Cache $cache -Dataset $ds -Out $csv -Jobs $Jobs -Configs @(
        "ablate-no-track|--track 0",
        "ablate-motion-comp-on|--motion-comp 1",
        "ablate-no-pose-prior|--pose-prior 0",
        "ablate-equal-prior|--classical-prior 1",
        "ablate-win8|--window 8",
        "ablate-win15|--window 15",
        "ablate-win30|--window 30")
}
