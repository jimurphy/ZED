#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory)][string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Run on Windows with PowerShell 7 and Inno Setup 6.7.x.' }
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$bundle = (Resolve-Path -LiteralPath $BundlePath).Path
if (-not (Test-Path -LiteralPath $bundle -PathType Container) -or
    $bundle -notmatch '[\\/]ZED_artefacts[\\/]Release[\\/]VST3[\\/]ZED\.vst3$') {
    throw 'Expected the established ZED_artefacts/Release/VST3/ZED.vst3 directory.'
}
# Do not follow reparse points when validating or packaging a bundle.
$items = @(Get-Item -LiteralPath $bundle) + @(Get-ChildItem -LiteralPath $bundle -Recurse -Force)
if (@($items | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) {
    throw 'Reparse points are not supported in the source bundle.'
}
$binary = Join-Path $bundle 'Contents/x86_64-win/ZED.vst3'
$executables = @($items | Where-Object { -not $_.PSIsContainer -and $_.Extension -eq '.vst3' })
if ($executables.Count -ne 1 -or $executables[0].FullName -ne $binary) {
    throw 'Expected exactly one VST3 executable at Contents/x86_64-win/ZED.vst3.'
}
$bytes = [IO.File]::ReadAllBytes($binary)
if ($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x5A4D) { throw 'Not a PE executable.' }
$pe = [BitConverter]::ToInt32($bytes, 0x3C)
if ($pe -lt 0 -or $pe -gt $bytes.Length - 26 -or
    [BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550 -or
    [BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664 -or
    [BitConverter]::ToUInt16($bytes, $pe + 24) -ne 0x20B -or
    ([BitConverter]::ToUInt16($bytes, $pe + 22) -band 0x2000) -eq 0) {
    throw 'Expected an AMD64 PE32+ DLL, not x86 or ARM64.'
}
$version = [Diagnostics.FileVersionInfo]::GetVersionInfo($binary)
if ($version.IsDebug -or $version.FileMajorPart -ne 1 -or $version.FileMinorPart -ne 0 -or
    $version.FileBuildPart -ne 0 -or $version.FilePrivatePart -ne 0) {
    throw 'Expected non-debug version 1.0.0.0. Supply the Release build.'
}
$manifest = Join-Path $bundle 'Contents/Resources/moduleinfo.json'
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { throw 'Missing VST3 moduleinfo.json.' }
$module = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
if ($module.Name -ne 'ZED' -or $module.Version -ne '1.0.0') { throw 'Unexpected VST3 metadata.' }

# Outputs inside this repository must be under its ignored ZED/build tree.
# Refuse reparse points in the output ancestry to avoid redirecting writes.
$output = [IO.Path]::GetFullPath($OutputDirectory)
if ($output -eq $bundle -or $output.StartsWith($bundle.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Installer output must not be inside the source bundle.'
}
$ancestor = $output
while ($ancestor) {
    if (Test-Path -LiteralPath $ancestor) {
        if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Output ancestry must not contain reparse points.'
        }
    }
    $ancestor = Split-Path -Parent $ancestor
}
$repoPrefix = $repo.TrimEnd('\') + '\'
$buildPrefix = (Join-Path $repo 'ZED/build').TrimEnd('\') + '\'
if ($output -eq $repo -or ($output.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase) -and
    -not $output.StartsWith($buildPrefix, [StringComparison]::OrdinalIgnoreCase))) {
    throw 'Use a dedicated directory beneath ZED/build or outside the repository.'
}
if (Test-Path -LiteralPath $output) {
    if (-not (Test-Path -LiteralPath $output -PathType Container) -or
        @(Get-ChildItem -LiteralPath $output -Force).Count -ne 0) {
        throw 'Installer output directory must be absent or empty; nothing is overwritten.'
    }
}

$candidates = @()
$onPath = @(Get-Command ISCC.exe -All -CommandType Application -ErrorAction SilentlyContinue)
foreach ($command in $onPath) { $candidates += $command.Source }
foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
    if ($base) {
        $candidate = Join-Path $base 'Inno Setup 6/ISCC.exe'
        $candidates += $candidate
    }
}
# Resolve before deduplication: PATH and known folders can spell the same file
# differently. A Chocolatey shim is a launcher, not the compiler to version-check.
$installations = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$chocolateyBin = if ($env:ChocolateyInstall) {
    [IO.Path]::GetFullPath((Join-Path $env:ChocolateyInstall 'bin')).TrimEnd('\')
} else { $null }
foreach ($raw in $candidates) {
    $canonical = '<unresolved>'
    $fileVersion = '<unavailable>'
    $productVersion = '<unavailable>'
    $passes = $false
    $reason = 'Candidate does not exist as a file'
    try {
        if (Test-Path -LiteralPath $raw -PathType Leaf) {
            $resolved = Resolve-Path -LiteralPath $raw -ErrorAction Stop
            if ($resolved.Provider.Name -ne 'FileSystem') { throw 'Not a filesystem path.' }
            $item = Get-Item -LiteralPath $resolved.ProviderPath -ErrorAction Stop
            $canonical = [IO.Path]::GetFullPath($item.FullName).Replace('/', '\')
            $v = [Diagnostics.FileVersionInfo]::GetVersionInfo($canonical)
            $fileVersion = $v.FileVersion
            $productVersion = $v.ProductVersion
            $metadata = "$($v.ProductName) $($v.FileDescription) $($v.CompanyName)"
            $isShim = $metadata -match '(?i)chocolatey|shimgen' -or
                ($chocolateyBin -and (Split-Path -Parent $canonical) -eq $chocolateyBin)
            $compilerLibrary = Join-Path (Split-Path -Parent $canonical) 'ISCmplr.dll'
            if ($isShim) {
                $reason = 'Chocolatey shim excluded; real compiler is discovered through installation folders'
            } elseif (-not (Test-Path -LiteralPath $compilerLibrary -PathType Leaf)) {
                $reason = 'Not a full Inno Setup compiler installation: ISCmplr.dll missing'
            } else {
                $passes = $true
                if ($installations.Add($canonical)) { $reason = 'Canonical compiler installation; engine version requires probe' }
                else { $reason = 'Duplicate route to the same canonical compiler installation' }
            }
        }
    } catch {
        $reason = "Candidate inspection failed: $($_.Exception.Message)"
    }
    Write-Host "Raw candidate: $raw"
    Write-Host "  Canonical path: $canonical"
    Write-Host "  File version: $fileVersion"
    Write-Host "  Product version: $productVersion"
    Write-Host "  Installation candidate: $passes; $reason"
}
Write-Host "Canonical compiler-installation count: $($installations.Count)"
if ($installations.Count -ne 1) {
    throw 'Expected exactly one real ISCC.exe installation with ISCmplr.dll. No compiler will be downloaded.'
}
$compiler = @($installations)[0]
Write-Host "Inno Setup compiler: $compiler"
Write-Host "Version: $([Diagnostics.FileVersionInfo]::GetVersionInfo($compiler).FileVersion)"
Write-Host "Product version: $([Diagnostics.FileVersionInfo]::GetVersionInfo($compiler).ProductVersion)"
Write-Host "Validated Release-layout AMD64 VST3: $bundle"
$definition = Join-Path $repo 'ZED/installer/windows/ZED.iss'
$compilerArguments = @("/DBundleSource=$bundle", "/DInstallerOutput=$output", $definition)
# ISCC 6.7.x can have 0.0.0.0 version resources. Its exact engine version is
# printed only after loading ISCmplr.dll for compilation. /O- disables output.
# Capture both streams independently, without PowerShell native-error preferences
# hiding the compiler's diagnostics or replacing its actual exit code.
Write-Host 'Probing compiler engine with /O- (no installer output)'
$startInfo = [Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $compiler
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.ArgumentList.Add('/O-')
foreach ($argument in $compilerArguments) { $startInfo.ArgumentList.Add($argument) }
$probe = [Diagnostics.Process]::new()
$probe.StartInfo = $startInfo
try {
    if (-not $probe.Start()) { throw 'Could not start the no-output compiler probe.' }
    $stdoutTask = $probe.StandardOutput.ReadToEndAsync()
    $stderrTask = $probe.StandardError.ReadToEndAsync()
    $probe.WaitForExit()
    $probeStdout = $stdoutTask.GetAwaiter().GetResult()
    $probeStderr = $stderrTask.GetAwaiter().GetResult()
    $probeExitCode = $probe.ExitCode
} finally {
    $probe.Dispose()
}
Write-Host "Probe stdout:`n$probeStdout"
Write-Host "Probe stderr:`n$probeStderr"
Write-Host "No-output probe exit code: $probeExitCode"
if ($probeExitCode -ne 0) { throw "No-output compilation probe failed with exit code $probeExitCode." }
$engineMatches = [regex]::Matches("$probeStdout`n$probeStderr",
    '(?m)^\s*Compiler engine version:\s*Inno Setup\s+(\d+\.\d+\.\d+(?:\.\d+)?)(?![\d.])[^\r\n]*\r?$')
if ($engineMatches.Count -ne 1) { throw 'Expected exactly one precise compiler-engine version line in probe output.' }
[version]$engineVersion = $null
if (-not [version]::TryParse($engineMatches[0].Groups[1].Value, [ref]$engineVersion)) {
    throw 'Could not parse the compiler-engine numeric version.'
}
Write-Host "Detected compiler-engine version: $engineVersion"
if ($engineVersion.Major -ne 6 -or $engineVersion.Minor -ne 7) {
    throw "Inno Setup 6.7.x is required; detected engine $engineVersion."
}
if (Test-Path -LiteralPath $output) {
    if (-not (Test-Path -LiteralPath $output -PathType Container) -or
        @(Get-ChildItem -LiteralPath $output -Force).Count -ne 0) {
        throw 'Output directory is no longer absent or empty after the no-output probe.'
    }
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
& $compiler @compilerArguments
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE." }
$name = 'ZED-1.0.0-rc.1-Windows-x64-Setup.exe'
$installer = Join-Path $output $name
$produced = @(Get-ChildItem -LiteralPath $output -File)
if ($produced.Count -ne 1 -or $produced[0].Name -cne $name -or $produced[0].Length -eq 0) {
    throw 'Compiler did not produce exactly the expected non-empty installer.'
}
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name" | Set-Content -LiteralPath "$installer.sha256" -Encoding ascii
Write-Host "Unsigned installer: $installer ($((Get-Item -LiteralPath $installer).Length) bytes)"
Write-Host "SHA-256: $hash; checksum: $installer.sha256"
