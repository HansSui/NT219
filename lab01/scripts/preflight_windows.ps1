[CmdletBinding()]
param(
    [double]$MaxCpuLoad = 15.0,
    [switch]$AllowBattery
)

$ErrorActionPreference = 'Stop'
$failed = $false

function Write-Warn([string]$Message) {
    Write-Host "WARNING: $Message" -ForegroundColor Yellow
}

$os = Get-CimInstance Win32_OperatingSystem
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$cs = Get-CimInstance Win32_ComputerSystem
$battery = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue
$plan = (powercfg /getactivescheme) -join ' '
try {
    $load = (Get-Counter '\Processor(_Total)\% Processor Time' -SampleInterval 1 -MaxSamples 1).CounterSamples.CookedValue
} catch {
    $load = (Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average
}
if ($null -eq $load) { $load = 0.0 }
$load = [double]$load

Write-Host '=== benchmark preflight ==='
Write-Host ("os: {0} {1}" -f $os.Caption, $os.Version)
Write-Host ("cpu: {0}" -f $cpu.Name.Trim())
Write-Host ("logical_cpus: {0}" -f $cs.NumberOfLogicalProcessors)
Write-Host ("power_scheme: {0}" -f $plan)
Write-Host ("cpu_load_pct: {0:N1}" -f $load)
Write-Host ("free_ram_gib: {0:N1}" -f ($os.FreePhysicalMemory / 1MB))

if ($battery) {
    $status = [int]$battery.BatteryStatus
    $acStatuses = @(2, 6, 7, 8, 9, 11)
    $onAc = $acStatuses -contains $status
    Write-Host ("battery_status: {0} (ac={1})" -f $status, $onAc)
    if (-not $onAc -and -not $AllowBattery) {
        Write-Warn 'laptop is not on AC power; plug in the charger before benchmarking'
        $failed = $true
    }
} else {
    Write-Host 'battery_status: none (desktop or no battery reported)'
}

if ($plan -notmatch 'High performance|8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c') {
    Write-Warn 'active power plan is not High performance (the runner will try to select it)'
}

if ($load -gt $MaxCpuLoad) {
    Write-Warn ("CPU load is {0:N1}% (limit {1:N1}%); close background work" -f $load, $MaxCpuLoad)
    $failed = $true
}

$stray = Get-Process aestool -ErrorAction SilentlyContinue
if ($stray) {
    Write-Warn ("{0} aestool process(es) are already running; stop them first" -f @($stray).Count)
    $failed = $true
}

Write-Host 'top CPU processes (PercentProcessorTime is per logical CPU; >100% means multiple threads):'
try {
    Get-CimInstance Win32_PerfFormattedData_PerfProc_Process |
        Where-Object { $_.Name -notin @('_Total', 'Idle') } |
        Sort-Object PercentProcessorTime -Descending |
        Select-Object -First 5 Name, PercentProcessorTime, IDProcess |
        Format-Table -AutoSize |
        Out-String |
        Write-Host
} catch {
    Write-Warn "could not read per-process CPU counters: $($_.Exception.Message)"
}

if (Test-Path 'build\windows-msvc\Release\aestool.exe') {
    Write-Host 'tool: build\windows-msvc\Release\aestool.exe (found)'
} else {
    Write-Warn 'build\windows-msvc\Release\aestool.exe is missing; build it first'
    $failed = $true
}

if ($failed) {
    Write-Host 'preflight: NOT CLEAN' -ForegroundColor Yellow
    exit 1
}
Write-Host 'preflight: OK' -ForegroundColor Green
exit 0
