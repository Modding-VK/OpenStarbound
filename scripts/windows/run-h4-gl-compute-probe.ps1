# H4 diagnostic: build+run OpenGL worldgen compute probe (Windows/RTX).
# Requires STAR_ENABLE_WORLDGEN_GL_COMPUTE. See
# doc/sector-runtime-h4-gpu-probe-windows-rtx-checklist-2026-11-27.md

param(
  [string]$BuildDir = "$(Join-Path $PSScriptRoot '..\..\build-h4-gl')",
  [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$SourceDir = Resolve-Path (Join-Path $PSScriptRoot "..\..\source")
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

Write-Host "Configuring $BuildDir (WORLDGEN_GL_COMPUTE=ON)"
cmake -S "$SourceDir" -B "$BuildDir" `
  -DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON `
  -DSTAR_BUILD_GUI=ON `
  -DBUILD_TESTING=ON `
  -DSTAR_ENABLE_TRACY=OFF

Write-Host "Building world_generation_gl_compute_probe ($Config)"
cmake --build "$BuildDir" --config $Config --target world_generation_gl_compute_probe

$Dist = Join-Path $RepoRoot "dist"
$Probe = Join-Path $Dist "world_generation_gl_compute_probe.exe"
if (-not (Test-Path $Probe)) {
  throw "Probe binary not found at $Probe"
}

Write-Host "Running $Probe"
& $Probe
exit $LASTEXITCODE
