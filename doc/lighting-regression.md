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
this correction. The code defect explains the observed rendering symptom;
the user's Windows scenario has not yet been rerun with the correction.

## Correction

- Flush queued drawing before replacing renderer resources.
- Reconnect shared textures to all effects after recreating them.
- Keep framebuffer inputs ahead of shared textures with matching names.
- Restore the active effect against the new framebuffers and texture sizes,
  including when the caller selects the same effect after reloading.
- Remove stale shared textures when their definitions disappear from config.

The correction is confined to renderer resource reloads. It does not change
light propagation calculations, world generation, Lua execution, or save data.

## Validation status and local follow-up

The modified `StarRenderer_opengl.cpp` translation unit compiled successfully
using the configured Linux release build. No runtime regression tests were run;
this is not a Windows graphics or gameplay validation.

Build a new Windows client from `perf/threading-audit`; the previously downloaded
`9f755f9e` binary does not contain this fix. With the same mods and graphics
settings, revisit the recorded location and check daylight, enclosed darkness,
held lights, placed lights, and emissive objects. Toggle HDR and antialiasing
individually, then restart with those settings saved to cover startup reloads.
Keep the new log and a short recording for comparison before drawing conclusions
about exploration performance.
