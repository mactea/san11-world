# Isolated runtime investigation. Hidden test processes only; some modes are
# deliberately unsafe reproductions and may exit with an access violation.
[CmdletBinding()]
param([ValidateSet('sample-only','bind','drain','preload','flush','direct','direct-surface','sample')][string[]]$Modes=@('sample-only'))
$ErrorActionPreference='Stop'
$worldmodDir=Split-Path -Parent $PSScriptRoot
$exe=(Resolve-Path -LiteralPath (Join-Path $worldmodDir 'build/render_interop.exe')).Path
$run=Join-Path $worldmodDir ('build/default-probes/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null=New-Item -ItemType Directory -Path $run
Copy-Item -LiteralPath $exe -Destination (Join-Path $run 'render_interop.exe')
$keyPath='HKCU:\Software\Microsoft\DirectX\UserGpuPreferences'
$key=Get-Item -LiteralPath $keyPath
$existed=$key.GetValueNames() -contains $exe
$old=$key.GetValue($exe)
$kind=if($existed){$key.GetValueKind($exe)}else{$null}
$oldMode=[Environment]::GetEnvironmentVariable('WORLDMOD_DEFAULT_PROBE','Process')
$report=[ordered]@{game_launched=$false;executable_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;checks=@();gpu_preference_restored=$false;environment_restored=$false}
try {
    foreach($gpu in 1,2){
        Set-ItemProperty -LiteralPath $keyPath -Name $exe -Value "GpuPreference=$gpu;" -Type String
        foreach($backend in '9on12','native'){foreach($mode in $Modes){
            $env:WORLDMOD_DEFAULT_PROBE=$mode
            $name="gpu$gpu-$backend-$mode"
            $out=Join-Path $run "$name.stdout.log"
            $err=Join-Path $run "$name.stderr.log"
            $p=Start-Process -FilePath $exe -ArgumentList "--$backend" -WorkingDirectory $worldmodDir -WindowStyle Hidden -RedirectStandardOutput $out -RedirectStandardError $err -PassThru
            try {
                $timedOut=-not $p.WaitForExit(30000)
                if($timedOut){$p.Kill();$null=$p.WaitForExit(5000)}
                $code=$p.ExitCode
            }finally{$p.Dispose()}
            $log=Get-Content -LiteralPath $out -Raw
            $vendor=if($gpu -eq 1){'8086'}else{'10de'}
            $identity=$log -match "Test backend: requested=9on12 actual=9on12 vendor=$vendor adapter="
            $count=([regex]::Matches($log,'Default texture contract iteration=')).Count
            $complete=$code -eq 0 -and $identity -and $count -eq 72 -and $log.Contains("PASS isolated DEFAULT probe mode=$mode cycles=12")
            $report.checks += [ordered]@{name=$name;exit_code=$code;timed_out=$timedOut;adapter_verified=$identity;exact_checks=$count;completed=$complete}
            Write-Host "$name exit=$code identity=$identity exact_checks=$count completed=$complete"
        }}
    }
}finally{
    if($existed){Set-ItemProperty -LiteralPath $keyPath -Name $exe -Value $old -Type $kind}
    else{Remove-ItemProperty -LiteralPath $keyPath -Name $exe -ErrorAction SilentlyContinue}
    if($null -eq $oldMode){Remove-Item Env:WORLDMOD_DEFAULT_PROBE -ErrorAction SilentlyContinue}
    else{[Environment]::SetEnvironmentVariable('WORLDMOD_DEFAULT_PROBE',$oldMode,'Process')}
    $key=Get-Item -LiteralPath $keyPath
    $report.gpu_preference_restored=(($key.GetValueNames() -contains $exe) -eq $existed) -and ($key.GetValue($exe) -eq $old) -and ((-not $existed) -or ($key.GetValueKind($exe) -eq $kind))
    $report.environment_restored=([Environment]::GetEnvironmentVariable('WORLDMOD_DEFAULT_PROBE','Process') -eq $oldMode)
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'summary.json') -Encoding utf8
    Write-Host "Report: $run"
}
if(-not $report.gpu_preference_restored -or -not $report.environment_restored){throw 'Probe settings were not restored'}
if(@($report.checks | Where-Object { $_.name -like '*-sample-only' -and -not $_.completed }).Count){throw 'Sample-only qualification failed; do not promote it to production'}
