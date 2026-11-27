# H4 — Checklist probe OpenGL (Windows / RTX)

Data: 27/11/2026. Branch `cursor/h4-gpu-shadow-9f77`.  
Validador local: Desktop-Vinicius RTX (`bc-23cf4f5c`).  
Agente H4: harness + probe opt-in (sem merge INT; build normal sem claim GL).

## Pré-requisitos

- Windows com GPU NVIDIA (ou outro driver GL 4.3+ / `ARB_compute_shader`)
- Checkout de `cursor/h4-gpu-shadow-9f77` (commit com harness)
- Toolchain do projeto (vcpkg + CMake + MSVC) já usada no worker
- `STAR_BUILD_GUI=ON` (obrigatório para SDL3/GLEW/OpenGL)

## Configure (diagnostic — não é build normal)

Na pasta `source/` (ou o preset local equivalente):

```bat
cmake -S . -B ../build-h4-gl ^
  -DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON ^
  -DSTAR_BUILD_GUI=ON ^
  -DBUILD_TESTING=ON ^
  -DSTAR_ENABLE_TRACY=OFF
```

Confirme no log: `Worldgen OpenGL compute probe: ON (diagnostic; not a normal-build claim)`.

## Build

```bat
cmake --build ../build-h4-gl --config RelWithDebInfo --target world_generation_gl_compute_probe world_generation_contract_tests
```

Binários esperados em `dist/` (ou o runtime output do preset):

- `world_generation_gl_compute_probe.exe`
- `world_generation_contract_tests.exe`

## Rodar o harness

```bat
cd dist
.\world_generation_gl_compute_probe.exe
```

### Saída esperada (PASS)

- `build_enabled=1`
- `gpuOpenGlCompute=1`
- `context` ≥ `4.3`
- linhas `gl: version=... renderer=...` com o dispositivo RTX
- `shadow DenseUint32Xor: verdict=0` (Equal)
- `shadow DenseUint32Add: verdict=0`
- `gpu submit/poll: status=1 backend=1`
- `PASS: probe + DenseUint32* CPU↔GPU shadow equal`
- exit code `0`

### Falhas úteis

| exit / mensagem | Ação |
| --- | --- |
| `build_enabled=0` | Rebuild com `-DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON` |
| `Probe did not claim OpenGL compute` + `failure:` | Atualizar driver; confirmar GL 4.3+; capturar `failure` |
| `shadow ... ShadowUnavailable` / divergences≠0 | Anexar log completo; não publicar saída GPU |
| crash no create context | Reportar vendor/driver; não forçar claim |

## Testes de contrato (build com flag)

```bat
.\world_generation_contract_tests.exe --gtest_filter=WorldGenerationComputeTest.*
```

Esperado **15/15** também com `STAR_ENABLE_WORLDGEN_GL_COMPUTE=ON` após o ajuste
que força `gpuOpenGlCompute=false` nos casos “sem capability”, e exercita submit/
sombra GPU quando o probe real claima compute.

Build **normal** (flag OFF) deve continuar com `gpuOpenGlCompute=false` e 15/15
sem claim GL.

## O que reportar ao Project / H4

1. Commit testado (`git rev-parse --short HEAD`)
2. Trecho do harness (probe + shadow + PASS/FAIL)
3. `gl version / vendor / renderer / glsl`
4. Se pediu patch: descreva o `failure` literal (sem tokens/paths pessoais)

## Não fazer

- Não mergear em `cursor/integration-sector-runtime-7342` a partir deste checklist
- Não ligar a flag em builds/release normais
- Não declarar 50% de H4 só porque o harness passou uma vez
- Não disparar GitHub Actions sem pedido explícito
