# Instalaveis bootstrap (Windows DevOnly)

Local layout helper for gameplay smoke after an All-DevOnly / client drop.
Does **not** replace CI, does **not** merge INT, and does **not** embed secrets.

## Script

`scripts/windows/bootstrap-instalaveis.ps1` (PowerShell 5.1, ASCII)

Default discovery (overridable):

| Parameter | Purpose |
| --- | --- |
| `-SteamAssets` | Starbound `assets` with `packed.pak` |
| `-OpenStarboundInstall` | Install root (`assets` + `mods`, e.g. FrackinUniverse) |
| `-WorkshopContent` | Steam workshop `content/211820` |
| `-SteamAppId` | Default `211820` (public Starbound app id) |
| `-CopySavesFrom` | Optional storage to copy `player`/`universe` |
| `-SkipWorkshopLinks` | Skip `mods/workshop-*.pak` hardlinks |

## What it writes

```
<repo>/
  Instalaveis/
    sbinit.config
    steam_appid.txt
    starbound.exe          # you place from All-DevOnly / build
  mods/
    workshop-*.pak         # hardlinks (same volume as workshop)
  storage-instalaveis/
  logs-instalaveis/
```

`sbinit.config` points at Steam assets, OpenStarbound install assets/mods,
`../assets/`, `../mods/`, absolute `storage-instalaveis`, `includeUGC: true`.

## Example

```powershell
# From repo root, after copying All-DevOnly binaries into Instalaveis\
powershell -NoProfile -File scripts\windows\bootstrap-instalaveis.ps1

# Optional: import characters from an existing OpenStarbound storage
powershell -NoProfile -File scripts\windows\bootstrap-instalaveis.ps1 `
  -CopySavesFrom "$env:LOCALAPPDATA\Programs\OpenStarbound\storage"
```

Then:

```powershell
Start-Process -FilePath .\Instalaveis\starbound.exe -WorkingDirectory .\Instalaveis
```

## Notes

- Workshop **file symlinks** need elevation; the script uses **hardlinks** (same
  volume) plus Steam UGC when `steam_appid.txt` + Steam client work.
- Cross-volume OpenStarbound `mods` stay as an **absolute** `assetDirectories`
  entry (no copy of multi-hundred-MB packs).
- `/mods/*` is gitignored; keep `Instalaveis/`, `storage-instalaveis/`, and
  `logs-instalaveis/` local (untracked).
- Characters need the same mods as the save (e.g. FrackinUniverse); missing
  mods produce `Failed to validate player` and an empty list.
