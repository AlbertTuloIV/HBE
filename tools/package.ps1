#requires -Version 5.1
<#
.SYNOPSIS
    Clean, build, stage, and zip a distributable HBE.Sandbox release.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\package.ps1
    powershell -ExecutionPolicy Bypass -File tools\package.ps1 -Version 0.24.0
#>
[CmdletBinding()]
param(
    [string]$Version,
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [string]$Platform = 'x64',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# --- Paths --------------------------------------------------------------
$RepoRoot = Split-Path -Parent $PSScriptRoot          # tools\.. = repo root
$Slnx     = Join-Path $RepoRoot 'HonestlyBadEngine.slnx'
$OutDir   = Join-Path $RepoRoot "$Platform\$Configuration"
$SrcAssets= Join-Path $RepoRoot 'HBE.Sandbox\assets'
$DistRoot = Join-Path $RepoRoot 'dist'

$RequiredDlls = @(
    (Join-Path $RepoRoot 'external\SDL3\lib\x64\SDL3.dll'),
    (Join-Path $RepoRoot 'external\SDL3_mixer\lib\x64\SDL3_mixer.dll')
)

# --- Locate MSBuild -----------------------------------------------------
function Find-MSBuild {
    $known = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
    if (Test-Path $known) { return $known }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $p = & $vswhere -latest -requires Microsoft.Component.MSBuild `
                        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
        if ($p) { return $p }
    }
    throw "MSBuild.exe not found. Install VS with the C++ workload or edit Find-MSBuild."
}

# --- Resolve version ----------------------------------------------------
function Resolve-Version {
    if ($Version) { return $Version }
    $vfile = Join-Path $RepoRoot 'VERSION'
    if (Test-Path $vfile) {
        $v = (Get-Content $vfile -Raw).Trim()
        if ($v) { return $v }
    }
    $date = Get-Date -Format 'yyyy.MM.dd'
    $sha  = ''
    try { $sha = (& git -C $RepoRoot rev-parse --short HEAD 2>$null).Trim() } catch { }
    if ($sha) { return "0.0.0-$date-$sha" } else { return "0.0.0-$date" }
}

# --- Build --------------------------------------------------------------
function Invoke-Build {
    $msbuild = Find-MSBuild
    Write-Host "Cleaning $OutDir ..." -ForegroundColor Cyan
    if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }

    Write-Host "Building $Configuration|$Platform ..." -ForegroundColor Cyan
    & $msbuild $Slnx /t:Build /p:Configuration=$Configuration /p:Platform=$Platform `
               /nologo /v:minimal /m
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }
}

# --- Stage --------------------------------------------------------------
$ExcludeFiles = @('*.pdb','*.ilk','*.exp','*.lib','*.obj','*.iobj','*.ipdb',
                  'bindings.json','bindings.cfg','graphics.cfg','*.log')

function Copy-AssetsFiltered {
    param([string]$Src, [string]$Dst)
    Get-ChildItem -Path $Src -Recurse -File | ForEach-Object {
        $skip = $false
        foreach ($pat in $ExcludeFiles) { if ($_.Name -like $pat) { $skip = $true; break } }
        if ($skip) { return }
        $rel = $_.FullName.Substring($Src.Length).TrimStart('\')
        $target = Join-Path $Dst $rel
        New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
        Copy-Item $_.FullName $target -Force
    }
}

function Invoke-Stage {
    param([string]$Ver)

    $exe = Join-Path $OutDir 'HBE.Sandbox.exe'
    if (-not (Test-Path $exe)) { throw "Missing exe: $exe (build first, or drop -SkipBuild)." }
    foreach ($dll in $RequiredDlls) {
        if (-not (Test-Path $dll)) { throw "Missing runtime DLL: $dll" }
    }
    if (-not (Test-Path $SrcAssets)) { throw "Missing source assets: $SrcAssets" }

    $stageName = "HBE.Sandbox-$Ver"
    $stageDir  = Join-Path $DistRoot $stageName
    if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

    Write-Host "Staging -> $stageDir" -ForegroundColor Cyan
    Copy-Item $exe $stageDir -Force
    foreach ($dll in $RequiredDlls) { Copy-Item $dll $stageDir -Force }

    # Assets from the SOURCE tree (pristine, prefab-referencing scene).
    Copy-AssetsFiltered -Src $SrcAssets -Dst (Join-Path $stageDir 'assets')

    # Short run instructions (see doc 06 for the canonical text).
    $readme = @"
HBE.Sandbox $Ver
================
Double-click HBE.Sandbox.exe to run.

Keep SDL3.dll, SDL3_mixer.dll, and the assets\ folder next to the exe.
Saved settings live in %APPDATA%\HBE\HonestlyBadEngine\.

Controls: A/D or arrow keys move, Space jump, E attack, R restart,
F11 fullscreen, Esc pause, ` (backtick) dev console.
"@
    Set-Content -Path (Join-Path $stageDir 'README.txt') -Value $readme -Encoding UTF8

    return $stageDir
}

# --- Zip ----------------------------------------------------------------
function Invoke-Zip {
    param([string]$StageDir, [string]$Ver)
    $zip = Join-Path $DistRoot "HBE.Sandbox-$Ver.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Write-Host "Zipping -> $zip" -ForegroundColor Cyan
    Compress-Archive -Path (Join-Path $StageDir '*') -DestinationPath $zip -Force
    return $zip
}

# --- Main ---------------------------------------------------------------
try {
    $ver = Resolve-Version
    Write-Host "=== Packaging HBE.Sandbox $ver ($Configuration|$Platform) ===" -ForegroundColor Green
    New-Item -ItemType Directory -Force -Path $DistRoot | Out-Null

    if (-not $SkipBuild) { Invoke-Build }

    $stageDir = Invoke-Stage -Ver $ver
    $zip      = Invoke-Zip -StageDir $stageDir -Ver $ver

    $sizeMB = [math]::Round((Get-Item $zip).Length / 1MB, 2)
    Write-Host "`nSUCCESS: $zip ($sizeMB MB)" -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "`nPACKAGE FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
