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
$onPath = Get-Command ISCC.exe -CommandType Application -ErrorAction SilentlyContinue
if ($onPath) { $candidates += $onPath.Source }
foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
    if ($base) {
        $candidate = Join-Path $base 'Inno Setup 6/ISCC.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { $candidates += $candidate }
    }
}
$compatible = @($candidates | Select-Object -Unique | Where-Object {
    $v = [Diagnostics.FileVersionInfo]::GetVersionInfo($_)
    $v.FileMajorPart -eq 6 -and $v.FileMinorPart -eq 7
})
if ($compatible.Count -ne 1) {
    throw 'Expected exactly one Inno Setup 6.7.x ISCC.exe on PATH or in Program Files. No compiler will be downloaded.'
}
$compiler = $compatible[0]
Write-Host "Inno Setup compiler: $compiler"
Write-Host "Version: $([Diagnostics.FileVersionInfo]::GetVersionInfo($compiler).FileVersion)"
Write-Host "Validated Release-layout AMD64 VST3: $bundle"
New-Item -ItemType Directory -Path $output -Force | Out-Null
$definition = Join-Path $repo 'ZED/installer/windows/ZED.iss'
& $compiler "/DBundleSource=$bundle" "/DInstallerOutput=$output" $definition
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
