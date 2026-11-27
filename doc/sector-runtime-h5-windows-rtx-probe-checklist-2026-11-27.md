# H5 — Checklist probe Windows / RTX (contratos de escala)

Data: 27/11/2026. Branch `cursor/h5-hardware-scale-9f77` @ **`cfd1aa5e`**.  
Validador local: Desktop-Vinicius (`bc-23cf4f5c`).  
Pré-condição: H4 probe RTX PASS @ `c7f6e605` (GPU shadow real). Sem merge INT.

## Objetivo deste probe

Validar no host RTX os **27 testes de contrato H5** (GPU-05 + VAL-03/04/05 +
MOD-02/03/04 + CPU-05). São modelos puros: **não** ligam OpenGL nem reclamam
compute. A capacidade GPU real já comprovada em H4 alimenta o próximo passo de
escala (seletor + governador com `gpuComputeAvailable=true`); este checklist só
fecha a evidência Windows dos contratos H5 no tip atual.

## Pré-requisitos

- Windows / Desktop-Vinicius (mesmo host do probe H4)
- Checkout exato: `cursor/h5-hardware-scale-9f77` @ `cfd1aa5e`
- Toolchain já usada no worker (VsDevCmd + CMake + Ninja/MSVC + vcpkg)
- `BUILD_TESTING=ON`; Tracy OFF no build de probe

## Configure (build de teste — não é release)

```bat
cmake -S source -B build-h5-probe ^
  -DBUILD_TESTING=ON ^
  -DSTAR_BUILD_GUI=ON ^
  -DSTAR_ENABLE_TRACY=OFF
```

`STAR_ENABLE_WORLDGEN_GL_COMPUTE` **não** é necessário para os contratos H5.
Se o tree local ainda tiver a flag (após rebase/cherry H4), deixe OFF neste
probe para não misturar claims GL com o alvo H5.

## Build

```bat
cmake --build build-h5-probe --config RelWithDebInfo --target world_generation_contract_tests
```

Binário esperado: `dist\world_generation_contract_tests.exe` (ou o runtime
output do preset).

## Rodar (harness de contratos)

Filtro único com as oito suites H5:

```bat
cd dist
.\world_generation_contract_tests.exe --gtest_filter=WorldGenerationHardwareSelectionTest.*:WorldGenerationDeliveryMetricsTest.*:WorldGenerationExecutionProfileTest.*:WorldGenerationScenarioMatrixTest.*:WorldGenerationAdoptionGatesTest.*:WorldGenerationEquivalenceVersionTest.*:WorldGenerationPlatformMatrixTest.*:WorldGenerationResourceGovernorTest.*
```

Ou o script:

```powershell
pwsh -File scripts\windows\run-h5-contract-probe.ps1 -ExePath .\dist\world_generation_contract_tests.exe
```

### Saída esperada (PASS)

- **27/27** testes PASSED
- exit code `0`
- Sem Actions; sem claim de backend GPU neste log

### Contagens por suite

| Suite | Esperado |
| --- | ---: |
| WorldGenerationHardwareSelectionTest | 7 |
| WorldGenerationDeliveryMetricsTest | 3 |
| WorldGenerationExecutionProfileTest | 3 |
| WorldGenerationScenarioMatrixTest | 2 |
| WorldGenerationAdoptionGatesTest | 4 |
| WorldGenerationEquivalenceVersionTest | 3 |
| WorldGenerationPlatformMatrixTest | 2 |
| WorldGenerationResourceGovernorTest | 3 |
| **Total** | **27** |

## O que reportar ao Project / H5

1. `git rev-parse --short HEAD` (deve ser `cfd1aa5e`)
2. Contagem gtest + exit code
3. Trecho final PASSED/FAILED
4. Host GPU (nome via nvidia-smi) — contexto de máquina, não prova GL H5
5. Arquivar log sob `media/h5-contract-probe/` no store do Project (ou path local + hashes)

## Não fazer

- Não mergear em `cursor/integration-sector-runtime-7342`
- Não declarar H5 50% só com este probe
- Não disparar GitHub Actions sem pedido explícito
- Não exigir `world_generation_gl_compute_probe` neste tip H5 (fica no branch H4)
- Não editar código H4 a partir deste checklist

## Depois do PASS

H5 pode sair de `waits_on: [h5-windows-rtx-probe]` e avançar o recorte de
escala que consome a API GPU sombra H4 (capabilities reais → seletor /
governador), ainda sem merge INT até autorização do Project.
