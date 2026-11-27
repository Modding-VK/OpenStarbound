# H5 Windows contract probe - pure model suites (27 tests).
# Does not enable STAR_ENABLE_WORLDGEN_GL_COMPUTE or claim GPU compute.
param(
  [Parameter(Mandatory = $false)]
  [string]$ExePath = ".\dist\world_generation_contract_tests.exe",

  [Parameter(Mandatory = $false)]
  [string]$LogPath = ".\h5-contract-probe-run.txt"
)

$ErrorActionPreference = "Stop"

$filter = @(
  "WorldGenerationHardwareSelectionTest.*",
  "WorldGenerationDeliveryMetricsTest.*",
  "WorldGenerationExecutionProfileTest.*",
  "WorldGenerationScenarioMatrixTest.*",
  "WorldGenerationAdoptionGatesTest.*",
  "WorldGenerationEquivalenceVersionTest.*",
  "WorldGenerationPlatformMatrixTest.*",
  "WorldGenerationResourceGovernorTest.*"
) -join ":"

if (-not (Test-Path -LiteralPath $ExePath)) {
  Write-Error "Missing $ExePath - build target world_generation_contract_tests first."
}

Write-Host "H5 contract probe"
Write-Host "exe=$ExePath"
Write-Host "filter=$filter"

& $ExePath --gtest_filter=$filter 2>&1 | Tee-Object -FilePath $LogPath
$code = $LASTEXITCODE
Write-Host "exit=$code log=$LogPath"
exit $code
