#requires -Version 7.0
# Destructive installation test: exclusively for an empty, disposable hosted runner.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory)][string]$InstallerPath,
    [Parameter(Mandatory)][string]$LogDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'This installation test is restricted to GitHub-hosted Windows runners.'
}
$admin = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $admin.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Runner must be elevated.' }
$source = (Resolve-Path -LiteralPath $BundlePath).Path
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$target = Join-Path $env:CommonProgramW6432 'VST3/ZED.vst3'
$appDirectory = Join-Path $env:ProgramW6432 'South Coast Synthesis/ZED'
if ((Test-Path -LiteralPath $target) -or (Test-Path -LiteralPath $appDirectory)) {
    throw 'Refusing to touch an existing ZED bundle or installation directory.'
}
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
$logs = (Resolve-Path -LiteralPath $LogDirectory).Path
function Get-Inventory([string]$Root) {
    @(Get-ChildItem -LiteralPath $Root -Recurse -File -Force | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($Root, $_.FullName)
        "$relative`t$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
    } | Sort-Object)
}
try {
    $installLog = Join-Path $logs 'install.log'
    $process = Start-Process -FilePath $installer -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/RESTARTEXITCODE=3010', "/LOG=`"$installLog`"") -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installation failed or requested restart: $($process.ExitCode)." }
    $binary = Join-Path $target 'Contents/x86_64-win/ZED.vst3'
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw 'Installed VST3 executable missing.' }
    $expected = Get-Inventory $source
    $actual = Get-Inventory $target
    if ($expected.Count -eq 0 -or @(Compare-Object $expected $actual).Count -ne 0) {
        throw 'Installed bundle file paths/hashes differ from the built source.'
    }
    Write-Host 'PASS: installed complete bundle matches every source file, including the executable.'
}
finally {
    # Both destinations were absent before this test. Never delete them manually.
    if (Test-Path -LiteralPath $appDirectory) {
        $uninstallers = @(Get-ChildItem -LiteralPath $appDirectory -Filter 'unins*.exe' -File)
        if ($uninstallers.Count -ne 1) { throw 'Expected exactly one generated ZED uninstaller.' }
        $uninstallLog = Join-Path $logs 'uninstall.log'
        $process = Start-Process -FilePath $uninstallers[0].FullName -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$uninstallLog`"") -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Uninstallation failed: $($process.ExitCode)." }
    }
}
if (Test-Path -LiteralPath $target) { throw 'ZED.vst3 remains after uninstallation.' }
if (Test-Path -LiteralPath $appDirectory) { throw 'ZED installation directory remains after uninstallation.' }
Write-Host 'PASS: silent uninstall removed ZED; no DAW was installed or launched.'
