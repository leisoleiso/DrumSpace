# DrumSpace installer
# Installs the VST3 plugin and the bundled inference server (with model).
# Run as administrator: right-click -> Run with PowerShell, or from an
# elevated terminal.

$ErrorActionPreference = 'Stop'

# --- Paths ---------------------------------------------------------------
# This script lives in the same folder as the built artefacts:
#   DrumSpace.vst3        (the plugin)
#   drumspace_server.exe  (the inference server)
#   checkpoints/          (the drum model)
$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$vst3Src     = Join-Path $scriptDir 'DrumSpace.vst3'
$serverSrc   = Join-Path $scriptDir 'drumspace_server.exe'
$ckptSrc     = Join-Path $scriptDir 'checkpoints'
$vst3Dst     = 'C:\Program Files\Common Files\VST3\DrumSpace.vst3'
$serverDst   = "$env:ProgramData\DrumSpace\server"
$ckptDst     = "$env:ProgramData\DrumSpace\server\checkpoints"

Write-Host '=== DrumSpace installer ===' -ForegroundColor Cyan

# 1) VST3 plugin
Write-Host "Installing VST3 -> $vst3Dst" -ForegroundColor Green
if (Test-Path $vst3Dst) { Remove-Item $vst3Dst -Recurse -Force }
Copy-Item $vst3Src $vst3Dst -Recurse -Force

# 2) Inference server
Write-Host "Installing server -> $serverDst" -ForegroundColor Green
New-Item -ItemType Directory -Force -Path $serverDst | Out-Null
Copy-Item $serverSrc (Join-Path $serverDst 'drumspace_server.exe') -Force

# 3) Model checkpoint
Write-Host "Installing model -> $ckptDst" -ForegroundColor Green
New-Item -ItemType Directory -Force -Path $ckptDst | Out-Null
Copy-Item "$ckptSrc\*" $ckptDst -Recurse -Force

Write-Host ''
Write-Host 'Done. DrumSpace is installed.' -ForegroundColor Green
Write-Host 'Load it in your DAW (FL Studio: Options -> Manage plugins -> Find more plugins).'
Write-Host 'The inference server starts automatically when the plugin loads.'
Write-Host 'Press Enter to exit.'
Read-Host
