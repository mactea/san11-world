# Read-only system sampling. Never starts a game, changes power settings or stops processes.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Out,
    [ValidateRange(2,120)][int]$Seconds = 10,
    [ValidateRange(1,10)][int]$IntervalSeconds = 2
)
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $Out) { throw 'Output already exists; choose a new file.' }
if (-not ('RenderPowerStatus' -as [type])) {
    Add-Type @'
using System.Runtime.InteropServices;
public static class RenderPowerStatus {
    [StructLayout(LayoutKind.Sequential)] public struct Status {
        public byte ACLineStatus, BatteryFlag, BatteryLifePercent, SystemStatusFlag;
        public uint BatteryLifeTime, BatteryFullLifeTime;
    }
    [DllImport("kernel32.dll")] public static extern bool GetSystemPowerStatus(out Status status);
}
'@
}
function Read-Power {
    $status = New-Object RenderPowerStatus+Status
    $ok = [RenderPowerStatus]::GetSystemPowerStatus([ref]$status)
    if (-not $ok) { return @{ available = $false } }
    return @{ available = $true; ac_line = $status.ACLineStatus; battery_percent = $status.BatteryLifePercent; battery_saver = $status.SystemStatusFlag }
}
function Read-Processes {
    $result = @{}
    foreach ($process in Get-Process) {
        try {
            $result[$process.Id] = @{ name = $process.ProcessName; cpu = $process.TotalProcessorTime.TotalSeconds; start = $process.StartTime.ToUniversalTime().Ticks }
        } catch { } # Protected/exited processes cannot supply a comparable sample.
        finally { $process.Dispose() }
    }
    return $result
}
$smi = Get-Command nvidia-smi -ErrorAction SilentlyContinue
$report = [ordered]@{
    started = (Get-Date).ToString('o'); logical_processors = [Environment]::ProcessorCount
    note = 'Read-only; CPU core_percent=100 means one logical CPU, machine_percent divides by CPU count. Newly started/exited/inaccessible processes are excluded from deltas. GPU samples describe all applications, not this renderer. Idle clocks do not prove throttling.'
    active_scheme = @(& powercfg /getactivescheme)
    processor_policy = @(& powercfg /query SCHEME_CURRENT SUB_PROCESSOR)
    samples = @()
}
if ($smi) { $report.gpu_details_before = @(& $smi.Source -q -d PERFORMANCE,POWER,CLOCK,TEMPERATURE 2>&1 | ForEach-Object { "$_" }) }
$previous = Read-Processes
$timer = [Diagnostics.Stopwatch]::StartNew()
$previousTime = $timer.Elapsed.TotalSeconds
while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
    Start-Sleep -Seconds $IntervalSeconds
    $now = Read-Processes
    $at = $timer.Elapsed.TotalSeconds
    $elapsed = $at - $previousTime
    $rows = foreach ($id in $now.Keys) {
        if ($previous.ContainsKey($id) -and $now[$id].start -eq $previous[$id].start) {
            $delta = $now[$id].cpu - $previous[$id].cpu
            if ($delta -ge 0) {
                [pscustomobject]@{ pid = $id; name = $now[$id].name; core_percent = [math]::Round(100*$delta/$elapsed,2); machine_percent = [math]::Round(100*$delta/$elapsed/[Environment]::ProcessorCount,2) }
            }
        }
    }
    $sample = [ordered]@{ utc = (Get-Date).ToUniversalTime().ToString('o'); interval_seconds = $elapsed; power = Read-Power; top_cpu = @($rows | Sort-Object core_percent -Descending | Select-Object -First 12) }
    if ($smi) {
        $sample.gpu_csv = @(& $smi.Source --query-gpu=name,pstate,clocks.gr,clocks.mem,utilization.gpu,temperature.gpu,power.draw,power.limit --format=csv 2>&1 | ForEach-Object { "$_" })
        $sample.gpu_exit_code = $LASTEXITCODE
    }
    $report.samples += $sample
    $previous = $now; $previousTime = $at
}
if ($smi) { $report.gpu_details_after = @(& $smi.Source -q -d PERFORMANCE,POWER,CLOCK,TEMPERATURE 2>&1 | ForEach-Object { "$_" }) }
$report.finished = (Get-Date).ToString('o')
$parent = Split-Path -Parent ([IO.Path]::GetFullPath($Out))
$null = New-Item -ItemType Directory -Force -Path $parent
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Out -Encoding utf8
Write-Host "Saved read-only system sample: $Out"
