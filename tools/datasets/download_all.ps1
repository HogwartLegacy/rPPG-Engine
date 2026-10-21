param([string]$Root = "D:\rppg_data", [string]$Aria = "D:\rppg_deps\aria2-1.37.0-win-64bit-build1\aria2c.exe")
# Resumable download of the benchmark datasets (lists made by make_download_lists.py).
$common = @("--continue=true", "--max-tries=0", "--retry-wait=5", "--timeout=60", "--connect-timeout=30",
            "--file-allocation=none", "--auto-file-renaming=false", "--allow-overwrite=false",
            "--summary-interval=120", "--console-log-level=warn")
& $Aria @common -j 8 -x 1 -s 1 -i "$Root\aria_small.txt" --log="$Root\aria_small.log" --log-level=notice
& $Aria @common -j 6 -x 2 -s 2 -i "$Root\aria_mcd_videos.txt" --log="$Root\aria_mcd.log" --log-level=notice
& $Aria @common -j 2 -x 8 -s 8 -k 4M -i "$Root\aria_ubfc_videos.txt" --log="$Root\aria_ubfc.log" --log-level=notice
"ALL DOWNLOADS FINISHED"
