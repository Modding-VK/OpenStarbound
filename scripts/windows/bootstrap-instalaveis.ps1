# Bootstrap local DevOnly "Instalaveis" layout for Windows gameplay smoke.
# ASCII-only for Windows PowerShell 5.1.
# Does not hardcode secrets or personal tokens. Paths are parameters / discovery.
#
# Creates/updates:
#   Instalaveis\sbinit.config
#   Instalaveis\steam_appid.txt
#   mods\workshop-*.pak hardlinks (same-volume Steam workshop)
#   storage-instalaveis\ and logs-instalaveis\
#
# Does NOT copy player saves (opt-in via -CopySavesFrom). Does NOT merge INT.

param(
  [Parameter(Mandatory = $false)]
  [string]$RepoRoot = "",

  [Parameter(Mandatory = $false)]
  [string]$SteamAssets = "",

  [Parameter(Mandatory = $false)]
  [string]$OpenStarboundInstall = "",

  [Parameter(Mandatory = $false)]
  [string]$WorkshopContent = "",

  [Parameter(Mandatory = $false)]
  [string]$SteamAppId = "211820",

  [Parameter(Mandatory = $false)]
  [string]$InstalaveisName = "Instalaveis",

  [Parameter(Mandatory = $false)]
  [string]$StorageName = "storage-instalaveis",

  [Parameter(Mandatory = $false)]
  [string]$LogsName = "logs-instalaveis",

  [Parameter(Mandatory = $false)]
  [string]$CopySavesFrom = "",

  [Parameter(Mandatory = $false)]
  [switch]$SkipWorkshopLinks,

  [Parameter(Mandatory = $false)]
  [switch]$WhatIf
)

$ErrorActionPreference = "Stop"

function Resolve-ExistingDir {
  param([string[]]$Candidates)
  foreach ($c in $Candidates) {
    if ($c -and (Test-Path -LiteralPath $c)) {
      return (Resolve-Path -LiteralPath $c).Path
    }
  }
  return $null
}

function To-SbinitPath {
  param([string]$Path)
  if (-not $Path) { return $null }
  $full = [IO.Path]::GetFullPath($Path)
  if (-not $full.EndsWith("\") -and -not $full.EndsWith("/")) {
    $full = $full + "\"
  }
  return ($full -replace "\\", "/")
}

if (-not $RepoRoot) {
  $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
} else {
  $RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
}

if (-not $SteamAssets) {
  $SteamAssets = Resolve-ExistingDir @(
    "M:\SteamLibrary\steamapps\common\Starbound\assets",
    "C:\Program Files (x86)\Steam\steamapps\common\Starbound\assets",
    "D:\SteamLibrary\steamapps\common\Starbound\assets",
    "E:\SteamLibrary\steamapps\common\Starbound\assets"
  )
}

if (-not $OpenStarboundInstall) {
  $localPrograms = Join-Path $env:LOCALAPPDATA "Programs\OpenStarbound"
  $OpenStarboundInstall = Resolve-ExistingDir @(
    $localPrograms,
    "C:\Program Files\OpenStarbound",
    "C:\Program Files (x86)\OpenStarbound"
  )
}

if (-not $WorkshopContent) {
  $WorkshopContent = Resolve-ExistingDir @(
    "M:\SteamLibrary\steamapps\workshop\content\211820",
    "C:\Program Files (x86)\Steam\steamapps\workshop\content\211820",
    "D:\SteamLibrary\steamapps\workshop\content\211820"
  )
}

$instalaveis = Join-Path $RepoRoot $InstalaveisName
$storage = Join-Path $RepoRoot $StorageName
$logs = Join-Path $RepoRoot $LogsName
$mods = Join-Path $RepoRoot "mods"
$osbAssets = $null
$osbMods = $null
if ($OpenStarboundInstall) {
  $osbAssets = Join-Path $OpenStarboundInstall "assets"
  $osbMods = Join-Path $OpenStarboundInstall "mods"
}

Write-Host "bootstrap-instalaveis"
Write-Host ("RepoRoot={0}" -f $RepoRoot)
Write-Host ("Instalaveis={0}" -f $instalaveis)
Write-Host ("SteamAssets={0}" -f $(if ($SteamAssets) { $SteamAssets } else { "(missing)" }))
Write-Host ("OpenStarboundInstall={0}" -f $(if ($OpenStarboundInstall) { $OpenStarboundInstall } else { "(missing)" }))
Write-Host ("WorkshopContent={0}" -f $(if ($WorkshopContent) { $WorkshopContent } else { "(missing)" }))
Write-Host ("SteamAppId={0}" -f $SteamAppId)

if (-not $SteamAssets) {
  Write-Error "Steam assets not found. Pass -SteamAssets <path-to-Starbound\\assets>."
}
if (-not (Test-Path -LiteralPath (Join-Path $SteamAssets "packed.pak"))) {
  Write-Error ("packed.pak missing under SteamAssets: {0}" -f $SteamAssets)
}

if ($WhatIf) {
  Write-Host "WhatIf: no files written."
  exit 0
}

foreach ($dir in @($instalaveis, $storage, $logs, $mods)) {
  if (-not (Test-Path -LiteralPath $dir)) {
    New-Item -ItemType Directory -Path $dir | Out-Null
    Write-Host ("created {0}" -f $dir)
  }
}

# steam_appid.txt enables Steamworks/UGC when Steam client is running.
$appIdPath = Join-Path $instalaveis "steam_appid.txt"
Set-Content -LiteralPath $appIdPath -Value $SteamAppId -Encoding ASCII -NoNewline
Write-Host ("wrote {0}" -f $appIdPath)

# Workshop hardlinks into repo mods\ (same volume only; skip cross-volume).
$linked = 0
$failed = 0
if (-not $SkipWorkshopLinks -and $WorkshopContent) {
  Get-ChildItem -LiteralPath $WorkshopContent -Directory -ErrorAction SilentlyContinue | ForEach-Object {
    $pak = Join-Path $_.FullName "contents.pak"
    if (-not (Test-Path -LiteralPath $pak)) {
      $script:failed++
      return
    }
    $dst = Join-Path $mods ("workshop-{0}.pak" -f $_.Name)
    if (Test-Path -LiteralPath $dst) {
      Remove-Item -LiteralPath $dst -Force
    }
    cmd /c ("mklink /H `"{0}`" `"{1}`"" -f $dst, $pak) | Out-Null
    if (($LASTEXITCODE -eq 0) -and (Test-Path -LiteralPath $dst)) {
      $script:linked++
    } else {
      $script:failed++
      Write-Host ("hardlink failed for {0} (need same volume; symlink needs elevation)" -f $_.Name)
    }
  }
  Write-Host ("workshop hardlinks linked={0} failed={1}" -f $linked, $failed)
} elseif ($SkipWorkshopLinks) {
  Write-Host "SkipWorkshopLinks set; not linking workshop paks."
} else {
  Write-Host "Workshop content path missing; UGC still works if Steam + steam_appid are available."
}

# Optional note file (ASCII).
$readmeMods = Join-Path $mods "README-instalaveis-bootstrap.txt"
$readmeBody = @(
  "Generated by scripts/windows/bootstrap-instalaveis.ps1",
  "FrackinUniverse / install mods: pointed via sbinit absolute OpenStarbound mods path.",
  "Workshop: workshop-*.pak hardlinks here and/or Steam UGC via steam_appid.txt.",
  "Do not commit this directory contents (gitignored)."
) -join "`r`n"
Set-Content -LiteralPath $readmeMods -Value $readmeBody -Encoding ASCII

# Build sbinit assetDirectories.
$assetDirs = New-Object System.Collections.Generic.List[string]
$assetDirs.Add((To-SbinitPath $SteamAssets))
if ($osbAssets -and (Test-Path -LiteralPath $osbAssets)) {
  $assetDirs.Add((To-SbinitPath $osbAssets))
}
if ($osbMods -and (Test-Path -LiteralPath $osbMods)) {
  $assetDirs.Add((To-SbinitPath $osbMods))
}
$assetDirs.Add("../assets/")
$assetDirs.Add("../mods/")

$sbStorage = To-SbinitPath $storage
$sbLogs = To-SbinitPath $logs

$assetJson = ($assetDirs | ForEach-Object { '    "' + $_ + '"' }) -join ",`r`n"
$sbinit = @(
  "{",
  '  "assetDirectories" : [',
  $assetJson,
  "  ],",
  "",
  ('  "storageDirectory" : "' + $sbStorage + '",'),
  ('  "logDirectory" : "' + $sbLogs + '",'),
  '  "includeUGC" : true',
  "}",
  ""
) -join "`r`n"

$sbinitPath = Join-Path $instalaveis "sbinit.config"
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($sbinitPath, $sbinit, $utf8NoBom)
Write-Host ("wrote {0}" -f $sbinitPath)

# Optional save copy (player/universe only). Preserves existing starbound.config.
if ($CopySavesFrom) {
  if (-not (Test-Path -LiteralPath $CopySavesFrom)) {
    Write-Error ("CopySavesFrom not found: {0}" -f $CopySavesFrom)
  }
  $cfgPreserve = Join-Path $storage "starbound.config"
  $cfgTmp = Join-Path $env:TEMP ("starbound-instalaveis-config-{0}.json" -f [guid]::NewGuid().ToString("N"))
  $hadCfg = Test-Path -LiteralPath $cfgPreserve
  if ($hadCfg) {
    Copy-Item -LiteralPath $cfgPreserve -Destination $cfgTmp -Force
  }
  foreach ($sub in @("player", "universe", "universeclient", "lua")) {
    $srcSub = Join-Path $CopySavesFrom $sub
    if (Test-Path -LiteralPath $srcSub) {
      $dstSub = Join-Path $storage $sub
      New-Item -ItemType Directory -Force -Path $dstSub | Out-Null
      robocopy $srcSub $dstSub /E /R:1 /W:1 /XF universe.lock /NFL /NDL /NJH /NJS /NP | Out-Null
      Write-Host ("copied {0}" -f $sub)
    }
  }
  if ($hadCfg) {
    Copy-Item -LiteralPath $cfgTmp -Destination $cfgPreserve -Force
    Remove-Item -LiteralPath $cfgTmp -Force -ErrorAction SilentlyContinue
    Write-Host "preserved existing storage starbound.config"
  }
}

Write-Host "bootstrap done"
Write-Host ("Launch: Start-Process -FilePath .\{0}\starbound.exe -WorkingDirectory .\{0}" -f $InstalaveisName)
Write-Host "Require starbound.exe (and DLLs) already present under Instalaveis from All-DevOnly/build."
exit 0
