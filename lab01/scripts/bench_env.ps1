[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$OutFile,
    [string]$Affinity = '0x4 (CPU 2)',
    [int]$Runs = 1000,
    [double]$Warmup = 1,
    [int]$Cooldown = 2,
    [int]$PilotWarmup = 15,
    [int]$Settle = 5,
    [int]$Threads = 1,
    [string]$Priority = 'high',
    [string]$Tool = 'build\windows-msvc\Release\aestool.exe'
)

$ErrorActionPreference = 'Stop'

$os = Get-CimInstance Win32_OperatingSystem
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$cs = Get-CimInstance Win32_ComputerSystem
$plan = (powercfg /getactivescheme) -join ' '
$battery = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue
$batteryStatus = if ($battery) { [int]$battery.BatteryStatus } else { 'none' }
$acPower = if ($battery) {
    if (@(2, 6, 7, 8, 9, 11) -contains [int]$battery.BatteryStatus) { 'true' } else { 'false' }
} else { 'n/a' }
try {
    $load = (Get-Counter '\Processor(_Total)\% Processor Time' -SampleInterval 1 -MaxSamples 1).CounterSamples.CookedValue
} catch {
    $load = (Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average
}
if ($null -eq $load) { $load = 0.0 }

$lines = @(
    "timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    "os: $($os.Caption) $($os.Version)",
    "cpu: $($cpu.Name.Trim())",
    "logical_cpus: $($cs.NumberOfLogicalProcessors)",
    "power_scheme: $plan",
    "battery_status: $batteryStatus",
    "ac_power: $acPower",
    "cpu_load_pct: $([math]::Round([double]$load, 1))",
    "affinity_mask: $Affinity",
    "priority: $Priority",
    "threads: $Threads",
    "runs: $Runs",
    "warmup_s: $Warmup",
    "pilot_warmup_s: $PilotWarmup",
    "settle_s: $Settle",
    "cooldown_s: $Cooldown",
    "tool: $Tool"
)

$dir = Split-Path -Parent $OutFile
if ($dir -and -not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir | Out-Null
}
$lines | Set-Content -Path $OutFile -Encoding UTF8
$lines | ForEach-Object { Write-Host $_ }
