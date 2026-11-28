# H4 GPU milestone-50 — multi-driver path (bloqueio restante do 50%)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77` (prep tip `85ab73ba`; Desktop INT evidence `509aaed0`)  
**Milestone:** **25%** — **não claimar 50%** até o 2º vendor + review Project.

## Evidência já consumida (1 vendor)

| Item | Valor |
| --- | --- |
| Host | Desktop V Pessoal — NVIDIA GeForce RTX 3050 |
| Driver | 621.40 / GL 4.6 |
| Tip | `509aaed0` (INT merge de `85ab73ba`) |
| `gain_pipeline` | `gpu_faster=1`, ratio ≈ **17.4** (passes=32) / **23.3** (passes=64) |
| `driver_evidence` | vendor NVIDIA; terrain/biome verdict Equal |
| Store | `internal/h4-desktop-gl-probe-gain-pipeline-2026-11-28.md` |

Single-kernel continua `gpu_faster=0` (controle negativo). Equality OK.

## O que falta para inputs do gate 50% (GPU-03/04)

1. **≥2º vendor / família de driver** com o mesmo harness diagnostic  
   - Exemplos aceitáveis: Intel (UHD/Arc), AMD (Radeon), ou segundo NVIDIA com **família/driver major diferente** documentada  
   - Mesma máquina com só outro driver patch NVIDIA **não** basta como 2º vendor
2. **Project review** dos logs/SHA256 (ganho harness ≠ adoção de worldgen GPU)
3. **WorldThread** wiring (GPU-02 restante) — fora deste tip; não confundir com multi-driver
4. Sem DenseUint32 horizontal

## Como coletar o 2º vendor

### Build (igual ao checklist Desktop)

```bat
cmake -S source -B ../build-h4-gl ^
  -DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON -DSTAR_BUILD_GUI=ON ^
  -DBUILD_TESTING=ON -DSTAR_ENABLE_TRACY=OFF
cmake --build ../build-h4-gl --config RelWithDebInfo --target world_generation_gl_compute_probe
```

### Rodar (mínimo multi-driver)

```bat
set STAR_WORLDGEN_GL_GAIN_REPEATS=65536
set STAR_WORLDGEN_GL_GAIN_ITERS=8
set STAR_WORLDGEN_GL_GAIN_PASSES=32
world_generation_gl_compute_probe.exe > h4-m50-gl-probe-vendor2-run.txt
```

### Linhas obrigatórias no log

- `gl: version=... vendor=... renderer=...`
- shadows TerrainField*/BiomeIdBucket `verdict=0`
- `gain_pipeline: ... equal=1` (preferir `gpu_faster=1`; se `0`, documentar corpus/passes e não forçar claim)
- `driver_evidence vendor='...' renderer='...' caps_fp=... shadow_fp=... terrain_verdict=0 biome_verdict=0`
- exit `0`

### Comparação com NVIDIA (509aaed0)

| Campo | Expectativa |
| --- | --- |
| terrain/biome verdict | Equal (0) em ambos |
| `shadow_fp` | Idealmente igual para o mesmo tip/corpus; se divergir, anexar divergences e **não** publicar GPU |
| `caps_fp` | Pode diferir (limites do device) — registrar, não falhar só por isso |
| `gpu_faster` | Preferível 1; 0 no 2º vendor bloqueia claim de ganho multi-driver |

Manifesto privacy-safe: nome do log, bytes, SHA256, UTC, `git rev-parse --short HEAD`, vendor/renderer — sem paths pessoais.

## Não fazer

- Não claimar H4 **50%** com um único vendor
- Não mergear INT / não Actions sem pedido
- Não retomar DenseUint32 horizontal
- Não tratar ratio 17–23 no harness diagnostic como throughput de setores/s de produção

## Próximo acionável

H4 entrou em **standby** neste path (28/11/2026): WorldStorage sector adopt
não é cloud-safe
([standby](sector-runtime-h4-gpu-m50-worldstorage-adopt-standby-2026-11-28.md)).

1. Agendar worker no 2º vendor (Desktop alternativo / lab) com tip INT ≥ `8a38889f` / H4 standby tip  
2. H4 consome `driver_evidence` + `gain_pipeline` do 2º log  
3. Só então Project avalia inputs do marco 50%  
4. WorldStorage adopt só com host Desktop+assets e tip auditado separado
