# Lighting regression investigation — 2026-10-23

## Report and evidence

The uploaded Windows log identifies source revision
`9f755f9e501861cbdd319d9c6dd95d9eec556130`, NVIDIA RTX 3050, and Frackin'
Universe 6.5.8 with additional mods. The video shows black terrain and entities
while the sky, interface, and some emissive details remain visible. The log
does not report a shader compilation failure.

The log records creation of `lightMap` at 00:37:33.824, followed by recreation
at 00:37:34.170, 00:39:12.827, and 00:39:14.627. This matches the renderer
configuration reload path used by HDR and antialiasing settings. Saved settings
are applied in `ClientApplication::render`, so changing a setting during the
session is not required to encounter a reload.

## Defect found by source inspection

`OpenGlRenderer::loadConfig` cleared every effect's texture reference, then
created new shared textures without reconnecting those effects. `setTexture`
uploaded lighting pixels to the shared `lightMap`, but drawing bound effect
textures only when their references were non-null. The world effect therefore
lost access to the uploaded light map after a configuration reload.

The shared-texture redesign was introduced in upstream commit `687e4653`,
which is included in this branch's base `61c50860`. The earlier working log
identifies `e604e078`, a different source revision despite the same displayed
application version. Our optimization commits did not edit the renderer before
this correction. The user subsequently confirmed that the corrected Windows
client no longer exhibits lighting failures (see validation below).

## Correction

- Flush queued drawing before replacing renderer resources.
- Reconnect shared textures to all effects after recreating them.
- Keep framebuffer inputs ahead of shared textures with matching names.
- Restore the active effect against the new framebuffers and texture sizes,
  including when the caller selects the same effect after reloading.
- Remove stale shared textures when their definitions disappear from config.

The correction is confined to renderer resource reloads. It does not change
light propagation calculations, world generation, Lua execution, or save data.

## Validation status

The modified `StarRenderer_opengl.cpp` translation unit compiled successfully
using the configured Linux release build. No automated runtime regression tests
were run in the cloud environment.

On 2026-10-23, the user supplied a Windows log identifying corrected revision
`684e6ae0ee211d7f470f17813347c32cf9a39614` and explicitly reported:
"Sem falhas de iluminação, corrigido" (no lighting failures, corrected).
The session ran from 01:20:21 to 01:23:54, recreated `lightMap` during startup
and at 01:21:00, visited planetary worlds and the ship, and stopped gracefully.
There are no logged shader/OpenGL errors. The visual confirmation comes from
the user; the log alone does not establish visual correctness or exhaustive
coverage of graphics settings.

This revision is the accepted reference for subsequent exploration performance
comparisons; see `performance-baseline.md`. The log also reports a missing ship
object, `osbprintedmap8x4`, which remains a separate unresolved asset issue.
