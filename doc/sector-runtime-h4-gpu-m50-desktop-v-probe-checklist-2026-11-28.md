# H4 m50 — Checklist probe Desktop V Pessoal (janela até 2026-11-29)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Autorização user:** testes GPU Desktop até **2026-11-29** (inclusive).  
**Executor local:** Desktop V Pessoal (Windows / RTX).  
**Cloud H4:** preparação cloud-safe pronta; **sem claim 50%** até PASS local + evidência.

## Objetivo do probe

Confirmar vertical milestone-50 com GL 4.3+ real:

1. Probe opt-in claima `gpuOpenGlCompute`
2. Shadow CPU↔GPU **TerrainFieldMax2 / Min2 / ClampU16** e **BiomeIdBucket**
3. Timing wall (cpu_us vs gpu_submit_poll_us) — informativo, não é claim de 50%
4. Deferred GPU ticket → `pump()` não-bloqueante → Ready com valores corretos

**Fora de escopo:** novos DenseUint32 / Half* / rejectedDuplicates horizontais.

## Pré-requisitos

- Checkout tip atual de `cursor/h4-gpu-shadow-9f77` (após tip com shaders Terrain/Biome)
- Toolchain já usada no worker (`STAR_BUILD_GUI=ON`)
- Variável / flag: `-DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON`

## Configure + build

```bat
cmake -S source -B ../build-h4-gl ^
  -DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON ^
  -DSTAR_BUILD_GUI=ON ^
  -DBUILD_TESTING=ON ^
  -DSTAR_ENABLE_TRACY=OFF

cmake --build ../build-h4-gl --config RelWithDebInfo --target world_generation_gl_compute_probe world_generation_contract_tests
```

Ou: `scripts\windows\run-h4-gl-compute-probe.ps1`

## Rodar

```bat
cd dist
.\world_generation_gl_compute_probe.exe
```

### PASS mínimo m50 (além do harness DenseUint32 legado)

- `build_enabled=1` · `gpuOpenGlCompute=1` · context ≥ `4.3`
- linhas `gl: version=... renderer=...` com o dispositivo local
- `--- m50 terrain/biome ---`
- `shadow TerrainFieldMax2: verdict=0`
- `shadow TerrainFieldMin2: verdict=0`
- `shadow TerrainFieldClampU16: verdict=0`
- `shadow BiomeIdBucket: verdict=0`
- `timing TerrainFieldMax2: ... equal=1` (e `ratio_cpu_over_gpu` / `gpu_faster`)
- `timing BiomeIdBucket: ... equal=1`
- `--- m50 gain pipeline ---` + `gain_pipeline: ... equal=1` (alvo: `gpu_faster=1`)
- `deferred TerrainFieldMax2: ... final_status=1` (Ready) com valores corretos
- `driver_evidence vendor=... caps_fp=... shadow_fp=... terrain_verdict=0 biome_verdict=0`
- `PASS: ... + driver_evidence + gain_pipeline`
- exit code `0`

Opcional (ganho pipeline — preferir sobre timing single-kernel):  
`set STAR_WORLDGEN_GL_GAIN_REPEATS=65536`  
`set STAR_WORLDGEN_GL_GAIN_ITERS=8`  
`set STAR_WORLDGEN_GL_GAIN_PASSES=32`

## Contrato (flag ON)

```bat
.\world_generation_contract_tests.exe --gtest_filter=WorldGenerationComputeTest.DeferredGpuTicketsCompleteViaPumpFencesRehearsal:WorldGenerationComputeTest.GpuSubmitRejectedWithoutCapabilityOrRehearsal:WorldGenerationComputeTest.GlOwnerThreadAndFenceHarnessNonBlocking:WorldGenerationComputeTest.TerrainBiomeFieldSketchExactCpuAndShadow:WorldGenerationComputeTest.TerrainGainPipelineCpuReferenceMatchesStackedKernels:WorldGenerationComputeTest.RealGlDeferredTerrainFieldCompletesViaPump:WorldGenerationComputeTest.OwnerThreadRejectsForeignMutations
```

Esperado: **0 FAIL** (rehearsal força `gpu=false`; RealGl* SKIP só se probe não claimar).

## Reportar ao Project / H4

1. `git rev-parse --short HEAD`
2. Trecho: `m50 terrain/biome` + **`gain_pipeline`** (`gpu_faster`) + deferred + `driver_evidence`
3. `gl version / vendor / renderer / glsl`
4. Se FAIL: `failure:` literal (sem tokens/paths pessoais)

Gravar log local (ex. `h4-m50-gl-probe-run.txt`); no Git só manifesto/hash se pedido.

## Multi-driver (bloqueio 50%)

Ganho NVIDIA (`gpu_faster=1`, tip `509aaed0`) **não** autoriza 50%.  
Coleta do 2º vendor: [multidriver path](sector-runtime-h4-gpu-m50-multidriver-path-2026-11-28.md).

## Não fazer

- Não claimar H4 50% com um único vendor / um PASS
- Não mergear INT a partir deste checklist
- Não ligar a flag em builds/release normais
- Não disparar GitHub Actions sem pedido
- Não retomar DenseUint32 horizontal
