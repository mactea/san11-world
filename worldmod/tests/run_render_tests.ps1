# Offline suite: builds and runs hidden test programs; never starts or deploys the game.
[CmdletBinding()]
param(
    [ValidateSet(1,2)][int[]]$GpuPreferences = @(1,2),
    [ValidateSet('system','9on12','native')][string[]]$Backends = @('system','9on12','native')
)

$ErrorActionPreference = 'Stop'
$worldmodDir = Split-Path -Parent $PSScriptRoot
$runDir = Join-Path $worldmodDir ('build\offline-tests\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $runDir
$report = [ordered]@{ started = (Get-Date).ToString('o'); passed = $false; checks = @(); gpu_preferences = $GpuPreferences; backends = $Backends; gpu_preference_restored = $null }
$restoreNeeded = $false
$failure = $null

function Invoke-Check([string]$Name, [string]$File, [string]$Arguments) {
    $stdout = Join-Path $runDir "$Name.stdout.log"
    $stderr = Join-Path $runDir "$Name.stderr.log"
    $process = $null
    $code = $null
    try {
        $process = Start-Process -FilePath $File -ArgumentList $Arguments -WorkingDirectory $worldmodDir `
            -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
        # Native runs all image fixtures and three Reset cycles on real GPUs.
        $timeoutSeconds = if ($Name -like 'gpu*-native') { 120 } else { 60 }
        $timer = [Diagnostics.Stopwatch]::StartNew()
        while (-not $process.WaitForExit(10000)) {
            if ($timer.Elapsed.TotalSeconds -ge $timeoutSeconds) {
                $process.Kill()
                $null = $process.WaitForExit(5000)
                throw "$Name exceeded $timeoutSeconds seconds; the test process was stopped."
            }
        }
        $code = $process.ExitCode
    } finally {
        $report.checks += [ordered]@{ name = $Name; exit_code = $code; stdout = $stdout; stderr = $stderr }
        if ($process) { $process.Dispose() }
    }
    if ($code -ne 0) { throw "$Name failed (exit $code). See $stdout and $stderr" }
    Write-Host "PASS $Name"
    return $stdout
}

try {
    if(Test-Path Env:WORLDMOD_DEFAULT_PROBE) { throw 'Unset WORLDMOD_DEFAULT_PROBE before the full regression suite.' }
    $python = (Get-Command python -CommandType Application | Select-Object -First 1).Source
    $null = Invoke-Check 'dll-build' $env:ComSpec ('/d /s /c ""' + (Join-Path $worldmodDir 'build.bat') + '""')
    $null = Invoke-Check 'shader-compile' $python ('"' + (Join-Path $PSScriptRoot 'validate_shaders.py') + '"')
    $null = Invoke-Check 'benchmark-validation' $python ('"' + (Join-Path $PSScriptRoot 'test_render_benchmark.py') + '"')
    $null = Invoke-Check 'terrain-asset-tests' $python ('"' + (Join-Path $PSScriptRoot 'test_terrain_assets.py') + '"')
    $null = Invoke-Check 'ground-loader-tests' $python ('"' + (Join-Path $PSScriptRoot 'test_ground_loader.py') + '"')
    $null = Invoke-Check 'terrain-sample-build' $python ('"' + (Join-Path $PSScriptRoot 'prepare_terrain_sample.py') + '"')
    $null = Invoke-Check 'terrain-replacement-build' $python ('"' + (Join-Path $PSScriptRoot 'pack_terrain_replacement.py') + '"')
    $null = Invoke-Check 'ground-identity-build' $python ('"' + (Join-Path $PSScriptRoot 'prepare_ground_identities.py') + '"')
    $null = Invoke-Check 'test-build' $env:ComSpec ('/d /s /c ""' + (Join-Path $PSScriptRoot 'build_interop.bat') + '" --build-only"')
    $testExe = (Resolve-Path -LiteralPath (Join-Path $worldmodDir 'build\render_interop.exe')).Path
    $gpuKeyPath = 'HKCU:\Software\Microsoft\DirectX\UserGpuPreferences'
    $gpuKey = Get-Item -LiteralPath $gpuKeyPath
    $hadPreference = $gpuKey.GetValueNames() -contains $testExe
    $oldPreference = $gpuKey.GetValue($testExe)
    $oldKind = if ($hadPreference) { $gpuKey.GetValueKind($testExe) } else { $null }
    @{ path = $testExe; existed = $hadPreference; value = $oldPreference; kind = "$oldKind" } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runDir 'gpu-preference-backup.json') -Encoding UTF8
    $restoreNeeded = $true
    foreach ($preference in $GpuPreferences) {
        Set-ItemProperty -LiteralPath $gpuKeyPath -Name $testExe -Value "GpuPreference=$preference;" -Type String
        foreach ($backend in $Backends) {
            $stdout = Invoke-Check "gpu$preference-$backend" $testExe "--$backend"
            $result = Get-Content -LiteralPath $stdout -Raw
            if ($result -notmatch 'Test backend: requested=(\w+) actual=(\w+) vendor=([0-9a-f]+) adapter=([^\r\n]+)') {
                throw "Missing backend/adapter evidence in $stdout"
            }
            $report.checks[-1].requested = $Matches[1]
            $report.checks[-1].actual = $Matches[2]
            $report.checks[-1].vendor = $Matches[3]
            $report.checks[-1].adapter = $Matches[4]
            $expectedVendor = if ($preference -eq 1) { '8086' } else { '10de' }
            if ($Matches[3] -ne $expectedVendor) { throw "GPU preference $preference selected unexpected vendor $($Matches[3]); do not count it as coverage of $expectedVendor." }
            $expected = if ($backend -eq 'native') { '9on12' } else { $backend }
            if ($Matches[1] -ne $expected) { throw "Unexpected requested backend in $stdout" }
            Write-Host "  actual=$($Matches[2]) adapter=$($Matches[4])"
            if ($result -notmatch 'Test native: requested=(\d) shared_queue=(\d)') {
                throw "Missing native queue evidence in $stdout"
            }
            $report.checks[-1].native_requested = [bool][int]$Matches[1]
            $report.checks[-1].native_shared_queue = [bool][int]$Matches[2]
            if ($report.checks[-1].native_requested -ne ($backend -eq 'native') -or
                $report.checks[-1].native_shared_queue -ne ($backend -eq 'native')) {
                throw "Unexpected native queue configuration in $stdout"
            }
        }
    }
    $report.passed = $true
} catch {
    $failure = $_
    $report.error = $_.Exception.Message
} finally {
    if ($restoreNeeded) {
        try {
            if ($hadPreference) {
                Set-ItemProperty -LiteralPath $gpuKeyPath -Name $testExe -Value $oldPreference -Type $oldKind
            } else {
                Remove-ItemProperty -LiteralPath $gpuKeyPath -Name $testExe
            }
            $key = Get-Item -LiteralPath $gpuKeyPath
            $report.gpu_preference_restored = (($key.GetValueNames() -contains $testExe) -eq $hadPreference) -and
                ($key.GetValue($testExe) -eq $oldPreference) -and
                ((-not $hadPreference) -or ($key.GetValueKind($testExe) -eq $oldKind))
            if (-not $report.gpu_preference_restored) { throw 'GPU preference restoration check failed.' }
        } catch {
            $report.gpu_preference_restored = $false
            $report.passed = $false
            $report.restore_error = $_.Exception.Message
            $failure = $_
        }
    }
    $report.finished = (Get-Date).ToString('o')
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDir 'summary.json') -Encoding UTF8
    Write-Host "Report: $runDir"
}
if ($failure) { throw $failure }
