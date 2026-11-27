# Fluxo de integração multi-frente

Data: 27/11/2026 UTC. Coordenador: Cloud Agent neste ambiente.

## Branch de integração

`cursor/integration-sector-runtime-7342` — criada a partir de `Experimental`
(`e55c008f`). **Não** é `main`. Recebe os merges das frentes após revisão.

## Frentes → integração

Abrir/atualizar PRs com **base** = `cursor/integration-sector-runtime-7342`
e **head** = a branch da frente. Links de compare (abrir PR no GitHub):

| Frente | Head | Compare |
| --- | --- | --- |
| H0 | `cursor/h0-foundations-9f77` | [compare](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h0-foundations-9f77?expand=1) |
| H1 | `cursor/h1-graph-cpu-9f77` | [compare](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h1-graph-cpu-9f77?expand=1) |
| H2 | `cursor/h2-products-prefetch-9f77` | [compare](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h2-products-prefetch-9f77?expand=1) |
| H3 | `cursor/h3-kernels-publication-9f77` | [compare](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h3-kernels-publication-9f77?expand=1) |
| H4 | `cursor/h4-gpu-shadow-9f77` | [compare](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h4-gpu-shadow-9f77?expand=1) |
| H5 | `cursor/h5-hardware-scale-9f77` | [compare](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h5-hardware-scale-9f77?expand=1) |
| R1 | `cursor/r1-regional-owners-9f77` | [compare](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/r1-regional-owners-9f77?expand=1) |
| R2 | `cursor/r2-regional-ops-9f77` | [compare](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/r2-regional-ops-9f77?expand=1) |

Este ambiente **não conseguiu** criar os PRs via API (`createPullRequest`
403 / ManagePullRequest preso ao repo temporário). Push de branches funciona;
abrir o PR uma vez pelos links acima desbloqueia o fluxo.

## Ordem sugerida de merge na integração

1. H0  
2. H1  
3. H2  
4. H4 (protótipo; sem publicação)  
5. R1  
6. R2 (após R1)  
7. H5 por último (depende de maturidade anterior)  
8. H3 quando houver commits  

Todos atualizam `doc/sector-runtime/progress.md` / checklist / roadmap →
**conflito documental quase certo**; reconciliar o painel numa passagem só na
integração.

## Política de revisão (coordenador)

A cada ciclo:

1. `git fetch` das `cursor/*-9f77` e da integração  
2. Revisar diff `integration...head` (compatibilidade, publicação, RNG/cópias)  
3. Registrar veredito abaixo  
4. Não mergear em `main`; não disparar Actions sem pedido  

## Revisão inicial (27/11/2026)

### H0 — aprovar com ressalvas

+ Continua harness sombra/VAL-02; sem publicação.  
+ Handoffs concorrentes alinhados ao programa.  
− Touches massivos em `world_generation_real_block_info_test.cpp` + `progress.md`.  
**Veredito:** OK para integrar **primeiro**, após rebase na integração.

### H1 — aprovar com ressalvas (após H0)

+ Código novo isolado (`ExecutiveShadow`/`Priority`); testes próprios.  
+ Fora do caminho vivo.  
− Atualiza progresso/roadmap; gates H0 ainda abertos no programa.  
**Veredito:** merge na integração **depois de H0**.

### H2 — aguardar H1 na integração

+ Header/cache isolado + teste.  
− Prematuro sem identidade H1 estabilizada na mesma linha.  
**Veredito:** hold até H1 mergeado na integração.

### H4 — hold documentado

+ Backend CPU de referência; sem publicação GPU.  
− Concorrência com H1/H2 em docs; publicação GPU bloqueada pelo roadmap.  
**Veredito:** pode ficar aberto; merge só como protótipo após H0/H1.

### H5 — hold

+ Fundações isoláveis e testes.  
− Cedo demais no roadmap; conflito documental alto.  
**Veredito:** não integrar ainda.

### R1 — hold relativo a H0/H1

+ Harness regional sombra.  
− Não autoritativo; docs conflitantes.  
**Veredito:** após H0 (e preferencialmente H1) na integração.

### R2 — hold até R1

+ REG-03..05 em sombra.  
− Depende de R1.  
**Veredito:** depois de R1.

### H3

Sem commits à frente da base.

## Revisão 27/11/2026 01:30 UTC

Base integração: `5388ed72`. PRs GitHub com base na integração: **nenhum**.

| Frente | Ahead | Novos desde revisão inicial | Veredito |
| --- | ---: | --- | --- |
| H0 | 6 | +2: setores adjacentes `{0,1}` e `{1,1}` lifecycle/assets | Mantém: **integrar primeiro** (rebase) |
| H1 | 5 | +3: cancel/mailbox/scheduler, WT-02 `runPass`, PreparedSector + governor sombra | Mantém: **após H0** |
| H2 | 2 | +1: milestones H2 + identidade PreparedSector | Mantém: **hold até H1** na integração |
| H3 | 0 | — | Sem trabalho |
| H4 | 2 | (sem novos) | Hold / protótipo após H0/H1 |
| H5 | 3 | (sem novos) | Hold |
| R1 | 3 | (sem novos) | Após H0(/H1) |
| R2 | 3 | (sem novos) | Após R1 |

Conflito documental (`progress.md`/checklist/roadmap) permanece o principal risco
de merge. Nenhuma frente editada por este coordenador; sem Actions; sem merge
em `main`.

## Revisão 27/11/2026 01:33 UTC (agente de integração)

Bases: Experimental `e55c008f` · integração `ef4f2d45` (+2 docs de fluxo vs Exp).
PRs com base na integração: **ainda nenhum** (403).

### Ahead vs Experimental / integração

| Frente | vs Exp | vs Int | Risco |
| --- | ---: | ---: | --- |
| H0 | 6 | 6 | Docs + harness RealBlockInfo (só H0 no `.cpp`) |
| H1 | 5 | 5 | Docs + CMake; **colide código** com H2/H5 |
| H2 | 2 | 2 | Docs + CMake; **PreparedSector** vs H1 |
| H3 | 0 | 0 | Vazia |
| H4 | 2 | 2 | Docs + CMake; compute isolado |
| H5 | 3 | 3 | Docs + CMake; **ResourceGovernor** vs H1 |
| R1 | 3 | 3 | Docs + CMake; region.* isolado vs R2 código |
| R2 | 3 | 3 | Docs + CMake; após R1 |

### Sobreposições (path tocado por ≥2 frentes)

**7 frentes:** `README.md`, `doc/regional-runtime-execution-checklist.md`,
`doc/sector-runtime/progress.md`  
**6:** `roadmap.md`, `source/game/CMakeLists.txt`, `source/test/CMakeLists.txt`  
**Código crítico:**
- `StarWorldGenerationPreparedSector.hpp` (+ teste) — **H1 ∩ H2**
- `StarWorldGenerationResourceGovernor.hpp` (+ teste) — **H1 ∩ H5**

Demais overlaps: `cpu-and-stages.md` (H1/H2/H5), `gpu-compute.md` (H4/H5),
`regional-threads.md` (R1/R2), `worldthread.md` (H1/H2).

### Vereditos / ordem

1. **H0** — merge primeiro (rebase na integração; reconciliar só docs).  
2. **H1** — depois; ao integrar, **escolher/unificar** PreparedSector e ResourceGovernor.  
3. **H2** — só após H1; resolver fork de PreparedSector.  
4. **H4** — protótipo OK após H0/H1 (CMake/docs).  
5. **R1 → R2** — após H0(/H1); código regional sem colisão entre si além de docs/CMake.  
6. **H5** — por último; unificar ResourceGovernor com H1.  
7. **H3** — aguardar commits.

Compares (base = integração):
- [H0](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h0-foundations-9f77?expand=1)
- [H1](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h1-graph-cpu-9f77?expand=1)
- [H2](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h2-products-prefetch-9f77?expand=1)
- [H4](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h4-gpu-shadow-9f77?expand=1)
- [H5](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h5-hardware-scale-9f77?expand=1)
- [R1](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/r1-regional-owners-9f77?expand=1)
- [R2](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/r2-regional-ops-9f77?expand=1)

## Merges já na integração (até tip `a0823338`)

- H0 base `d6a6419a` → `d87516d0`; H0-delta `7b1bbbbc` → `4e2e0bdd`; H0 VAL-02 completo `54f69cd2` → `a0823338`
- H1 `7ed91dec` → `b43fc767` (PreparedSector/ResourceGovernor canônicos = H1)
- H2 `b9572592` → `55c38c27` (PreparedSector = H1; H2-only cache/prefetch/batch)
- Experimental `e55c008f`; INT ahead 35, behind 0. PRs base=integração: **nenhum**

## Revisão sync 27/11/2026 ~02:00 UTC

Bases: Exp `e55c008f` · INT `a0823338`. Frentes **não** editadas. Sem Actions / sem `main`.
Política: Project define caminhos; Integrador mergeia tips `ready-for-integration`
(H0-delta/H3 quando marcar; ordem H4→R1→R2).

| Frente | Tip origin | vs Exp | vs INT | Conflitos (merge-tree) | phase (store) | Nota |
| --- | --- | ---: | ---: | ---: | --- | --- |
| H0 | `8206aa7d` | 20 | **5** | 4 — checklist, progress, CMake×2 | standby | MOD-01 EffectBoundary (+5 vs `54f69cd2`); **não** ready-for-integration |
| H1 | `7ed91dec` | 7 | 0 | 0 (ancestral) | standby | Já integrado |
| H2 | `b9572592` | 2 | 0 | 0 (ancestral) | standby | Já integrado |
| H3 | `4d84944e` | 8 | **8** | 11 — docs H3 + CMake×2 | active | CPU-04..WT-04 sombra; aguardar ready |
| H4 | `80825c7c` | 2 | 2 | 9 — README/docs/CMake | standby | Próximo na ordem quando ready |
| H5 | `cfd1aa5e` | 3 | 3 | 16 — docs/CMake + **ResourceGovernor** add/add | blocked(h4) | Hold; unificar governor = H1 |
| R1 | origin `94acf8eb` | 3 | 3 | 8 — README/docs/CMake | standby | Store head `bfd2c23a` **local não pushed** (token inválido) |
| R2 | `dd9e3b58` | 3 | 3 | 9 — README/docs/CMake | ready-for-integration | Fila após H4→R1 |

### Vereditos deste ciclo

- **Sem merge agora:** nenhum tip novo em `ready-for-integration` na ordem aplicável.
- H0 delta MOD-01 (`8206aa7d`): candidata quando marcar ready; conflitos só docs/CMake.
- H3: trabalho novo relevante (+8); merge quando `ready-for-integration`.
- H4→R1→R2: H4/R1 standby; R2 ready mas fora de ordem. R1 tip efetiva na origem = `94acf8eb` até push de `bfd2c23a`.
- H5: hold (ResourceGovernor vs H1).

Compares: [H0](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h0-foundations-9f77?expand=1) · [H3](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h3-kernels-publication-9f77?expand=1) · [H4](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/h4-gpu-shadow-9f77?expand=1) · [R1](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/r1-regional-owners-9f77?expand=1) · [R2](https://github.com/Modding-VK/OpenStarbound/compare/cursor/integration-sector-runtime-7342...cursor/r2-regional-ops-9f77?expand=1)

## Revisão sync 27/11/2026 ~02:30 UTC

Bases: Exp `e55c008f` · INT `486be7d4`. Frentes **não** editadas. Sem Actions / sem `main`.
PRs com base na integração: **nenhum**.

| Frente | Tip **remoto** | vs Exp | vs INT | Conflitos | phase (store) | Delta vs sync 02:00 |
| --- | --- | ---: | ---: | ---: | --- | --- |
| H0 | `8206aa7d` | 20 | 5 | 4 docs/CMake | standby | remoto igual; store local `c124f899` **não pushed** (401) |
| H1 | `7ed91dec` | 7 | 0 | 0 | standby | sem mudança (já na INT) |
| H2 | `b9572592` | 2 | 0 | 0 | standby | sem mudança (já na INT) |
| H3 | `db3753a8` | 9 | **9** | 11 docs/CMake | local-ready | +1 remoto (IO-02); store local `b3b7deee` ahead 3 **não pushed** (401) |
| H4 | `80825c7c` | 2 | 2 | 9 | standby | sem mudança |
| H5 | `cfd1aa5e` | 3 | 3 | 16 (+ResourceGovernor) | blocked | sem mudança |
| R1 | `94acf8eb` | 3 | 3 | 8 | standby | remoto igual; store local `bfd2c23a` **não pushed** (401) |
| R2 | `dd9e3b58` | 3 | 3 | 9 | ready-for-integration | espera ordem H4→R1 |

### Commits locais não pushed (Project / 401)

| Frente | SHA local | Relação ao remoto |
| --- | --- | --- |
| H0 | `c124f899` | além de `8206aa7d` (não visível neste clone) |
| H3 | `b3b7deee` | ahead 3 de `db3753a8` (IO-03..05 sombra no store) |
| R1 | `bfd2c23a` | ahead 1 de `94acf8eb` |

Integrador **não** pode mergear tips locais até o push; `local-ready` ≠ `ready-for-integration`.

### Vereditos

- Sem merge neste ciclo.
- H3 remoto avançou (IO-02 em `db3753a8`); tip completa H3 25% ainda presa no push.
- Fila inalterada: H0-delta/`ready` quando tip remota + phase; H3 quando tip remota + ready; depois H4→R1→R2.

## Revisão sync 27/11/2026 ~03:00 UTC

Bases: Exp `e55c008f` · INT **`34c11ed7`** (H0+H1+H2+H3 mergeados). Frentes não editadas.
PRs com base na integração: **nenhum**. Sem Actions / sem `main`.

| Frente | Tip remoto | vs INT | Conflitos | phase (store) |
| --- | --- | ---: | ---: | --- |
| H0 | `c124f899` | 0 | 0 | ready-for-integration (já ancestral) |
| H1 | `7ed91dec` | 0 | 0 | active (já ancestral) |
| H2 | `b9572592` | 0 | 0 | standby (já ancestral) |
| H3 | `b3b7deee` | 0 | 0 | ready-for-integration (já ancestral) |
| H4 | `80825c7c` | **+2** | 10 — README/docs/CMake | standby |
| H5 | `cfd1aa5e` | **+3** | 16 — docs/CMake + ResourceGovernor add/add | blocked(h4) |
| R1 | `bfd2c23a` | **+4** | 9 — README/docs/CMake | standby (tip pushed) |
| R2 | `dd9e3b58` | **+3** | 9 — README/docs/CMake | ready-for-integration (espera H4→R1) |

### Vereditos

- Sem merge neste ciclo (nada novo em ordem com `ready-for-integration` aplicável).
- Próximo quando ready: **H4** → R1 → R2; H5 por último (ResourceGovernor = H1).
- H3 commits: já integrados em `34c11ed7` (CPU-04..IO-05 sombra).

## Revisão sync 27/11/2026 ~03:30 UTC

Bases: Exp `e55c008f` · INT **`e856104b`** (CI `d835751d` + trigger só INT `79e7092c` + esta revisão).
Frentes **não** editadas. Sem dispatch manual de `build.yml` / sem `main`.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; sombra esgotada / espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL; GPU-01…04 CPU-first) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~03:00

- INT avançou só com CI: `d835751d` (`.github/workflows/ci-integration.yml`; `build.yml` intacto).
- Nenhuma frente ganhou tip nova vs o sync anterior (H4/H5 tips estáveis; H0–H3/R1/R2 já ancestrais).
- H3: sem commits novos; tip `3bdf1927` permanece integrada.

### Vereditos

- **Sem merge neste ciclo:** H4 ainda `standby` (hold GL); H5 blocked até API GPU sombra; demais tips já na INT.
- Próximo candidato quando Project/H4 marcar ready: **H4** (`80825c7c`, +2, conflitos ~docs/CMake) → depois só deltas novos.
- Sem comentários em PR (fila vazia).

## Revisão sync 27/11/2026 ~04:00 UTC

Bases: Exp `0efebf99` · INT **`d07dc87b`** · default `main` **`bf7f4638`**.
Frentes **não** editadas. Sem dispatch de Actions / sem merge em `main` neste ciclo.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~03:30

- Tips das frentes **inalteradas** (H3 sem commits novos).
- CI discovery: workflow registado na default via [PR #2](https://github.com/Modding-VK/OpenStarbound/pull/2) → `main` `bf7f4638` (API `CI Integration` active); PR #1 em Experimental permanece.
- Branches temporárias dos PRs CI removidas no remoto; INT não tocada por isso.

### Vereditos

- **Sem merge de frentes:** H4 continua hold GL; H5 blocked; restantes já ancestrais da INT.
- Próximo quando ready: **H4** `80825c7c`. Sem comentários em PR (fila vazia).

## Revisão sync 27/11/2026 ~04:30 UTC

Bases: Exp `0efebf99` · INT **`ac805ffd`**. Frentes **não** editadas.
Neste ciclo de revisão: sem merge em `main`; sem `workflow_dispatch` manual; sem editar frentes.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~04:00

- Tips das frentes **inalteradas** (H3 sem commits novos).
- INT avançou por CI/correções (não por merge de frente): PreparedSector dedupe `1bb1f03b`; Windows NoAssets/fast-math+NamePool `ac805ffd`.
- Ahead frentes vs INT: inalterado (H4 +2 / H5 +3).

### Vereditos

- **Sem merge de frentes** neste ciclo (H4 hold GL; H5 blocked).
- Próximo quando ready: **H4** `80825c7c`. Sem comentários em PR (fila vazia).

## Revisão sync 27/11/2026 ~05:00 UTC

Bases: Exp `0efebf99` · INT **`232c389c`**. Frentes **não** editadas.
Neste ciclo: sem merge em `main`; sem `workflow_dispatch` manual; sem editar frentes.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~04:30

- Tips das frentes **inalteradas** (H3 sem commits novos).
- INT tip `232c389c` (= revisão 04:30 + ancestral dos fixes CI). Ahead frentes inalterado.
- Build Windows na tip `232c389c`: **success** (run 45940701449). CI Integration Linux ainda in_progress no watch.

### Vereditos

- **Sem merge de frentes** (H4 hold GL; H5 blocked).
- Próximo quando ready: **H4** `80825c7c`. Sem comentários em PR (fila vazia).

## Revisão sync 27/11/2026 ~05:30 UTC

Bases: Exp `0efebf99` · INT **`706d385e`**. Frentes **não** editadas.
Neste ciclo: sem merge em `main`; sem `workflow_dispatch` manual; sem editar frentes.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~05:00

- Tips das frentes **inalteradas** (H3 sem commits novos).
- INT tip canônica `706d385e` = StarMemory jemalloc guard (`16c468f5`) + CI `-DSTAR_USE_JEMALLOC=OFF` (`706d385e`).
- Ahead frentes inalterado (H4 +2 / H5 +3).

### Vereditos

- **Sem merge de frentes** (H4 hold GL; H5 blocked).
- Próximo quando ready: **H4** `80825c7c`. Sem comentários em PR (fila vazia).


## Revisão sync 27/11/2026 ~06:25 UTC

Bases: Exp `0efebf99` · INT **`89ada4ee`**. Frentes **não** editadas.
Neste ciclo: sem merge em `main`; sem merge H4/H5; flow-doc push liberado após gates verdes.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~05:30

- Tips das frentes **inalteradas** (H3 sem commits novos).
- INT avançou por CI: tip `89ada4ee` = fix NoAssets signed-zero sob `-ffast-math` (`EmptyObjectPreservesCallerFallbacks`); jemalloc OFF + StarMemory já na tip anterior.
- CI #9 (`4a4d5283`) falhou só no signed-zero fallback float ABI (não jemalloc). Build #80 + CI #11 **success** em `89ada4ee`.
- Revisão 06:00 ficou só no store enquanto gates rodavam (política anti-cancel).

### Vereditos

- **Sem merge de frentes** (H4 hold GL; H5 blocked).
- Gates tip: Build #80 success · CI Integration #11 success.
- Próximo quando ready: **H4** `80825c7c`. Sem comentários em PR (fila vazia).

## Revisão sync 27/11/2026 ~07:00 UTC

Bases: Exp `0efebf99` · INT **`372a0127`**. Frentes **não** editadas.
Neste ciclo: sem merge em `main`; sem `workflow_dispatch` manual; sem editar frentes.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~06:25

- Tips das frentes **inalteradas** (H3 sem commits novos).
- INT tip `372a0127` (revisão 06:25); gates tip: Build #81 + CI #12 **success**.
- Conflitos preliminares merge-tree vs INT (hold, sem merge): H4 ~docs+`CMakeLists.txt`; H5 ~docs (`progress`/`roadmap`/`compatibility`/…).

### Vereditos

- **Sem merge de frentes** (H4 hold GL; H5 blocked).
- Próximo quando ready: **H4** `80825c7c` (resolver conflitos doc/CMake no merge). Sem comentários em PR (fila vazia).

## Revisão sync 27/11/2026 ~08:00 UTC

Bases: Exp `0efebf99` · INT **`d98a173b`**. Frentes **não** editadas.
Neste ciclo: sem merge em `main`; sem `workflow_dispatch` manual; sem editar frentes.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~07:00

- Tips das frentes **inalteradas** (H3 sem commits novos).
- INT tip `d98a173b` permanece; gates Build #82 + CI #13 **success**.
- Sync 07:30 foi store-only (sem delta de frentes).

### Vereditos

- **Sem merge de frentes** (H4 hold GL; H5 blocked).
- Próximo quando ready: **H4** `80825c7c`. Sem comentários em PR (fila vazia).

## Revisão sync 27/11/2026 ~09:00 UTC

Bases: Exp `0efebf99` · INT **`0161e3f8`**. Frentes **não** editadas.
Neste ciclo: sem merge em `main`; sem editar frentes; sem merge H4/H5.
PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs Exp | vs INT | Ancestral INT | phase (store) |
| --- | --- | ---: | ---: | --- | --- |
| H0 | `c124f899` | 21 | 0 | sim | ready-for-integration (já na INT) |
| H1 | `4d014af2` | 8 | 0 | sim | standby (já na INT) |
| H2 | `b1162b5d` | 3 | 0 | sim | ready-for-integration (já na INT) |
| H3 | `3bdf1927` | 19 | 0 | sim | ready-for-integration (já na INT; espera live-publish) |
| H4 | `80825c7c` | 2 | **+2** | não | standby (hold GL) |
| H5 | `cfd1aa5e` | 3 | **+3** | não | blocked(h4) |
| R1 | `6947bbb9` | 7 | 0 | sim | standby (já na INT) |
| R2 | `f86b4d17` | 4 | 0 | sim | ready-for-integration (já na INT) |

### Delta vs sync ~08:00

- Tips das frentes **inalteradas** (H3 sem commits novos).
- Tip INT `0161e3f8` gates: Build #83 + CI #14 **success** (linux-release ~29m legítimo).
- Sync 08:30 foi store-only enquanto CI #14 ainda compilava.

### Vereditos

- **Sem merge de frentes** (H4 hold GL; H5 blocked).
- Próximo quando ready: **H4** `80825c7c`. Sem comentários em PR (fila vazia).

## Revisão pós-gates 27/11/2026 ~18:15 UTC

INT tip base: **`8155297f`** (PR#3 `ci-sector-runtime.yml`). Frentes **não**
editadas; **sem** merge de frentes H*/R*; **sem** `build.yml`.

### Estado integrado

| Item | Valor |
| --- | --- |
| Stack na INT | H0+H1+H2+H3+H4+H5+R1+R2 |
| Tip material (jogo) | `33628d9b` — merge H5; **Build #86 SUCCESS** (instalador) |
| Tip YAML | `8155297f` — merge PR#3 |
| CI Integration #20 | **SUCCESS** (`46043261908`) |
| CI Sector Runtime #2 | **SUCCESS** (`46043262209`; Smoke RTX skipped) |
| Programa | **25,5%** (tip YAML não cria marco) |
| Smoke jogo | **Hold** até base/instalador local do usuário |

### Frentes vs INT (`8155297f`) — ahead=0

H0 `c124f899` · H1 `4d014af2` · H2 `b1162b5d` · H3 `3bdf1927` ·
H4 `c44b1a38` · H5 `a2ec01f0` · R1 `6947bbb9` · R2 `f86b4d17`.

### Política Build / merge

- Tip material = código/assets do jogo. `.github/workflows/*` e flow-doc **não**
  disparam `build.yml`. Build #86 @ `33628d9b` permanece instalador.
- Watch: CI Integration + CI Sector Runtime.
- **Não mergear frentes** até ahead>0.
- **PR #4** (`cursor/ci-sector-runtime-rtx-docs-4d5a`): documenta placeholders
  smoke RTX (Desktop-Vinicius ≠ GHA runner). Merge na INT **só após CI do PR
  verde**; tip `8155297f` já verde autoriza o merge quando o CI do #4 terminar.

### Vereditos

- Hold frentes. Hold smoke local. Sem `build.yml`.
- PR #4: aguardar CI → merge YAML docs.

## Revisão sync 27/11/2026 ~19:00 UTC

Bases: INT **`7e4a5baa`**. Frentes **não** editadas; **sem** merge em `main`;
**sem** disparo de Actions neste ciclo. PRs GitHub com base na integração:
**nenhum**.

| Frente | Tip remoto | vs INT | Ancestral INT |
| --- | --- | ---: | --- |
| H0 | `c124f899` | 0 | sim |
| H1 | `4d014af2` | 0 | sim |
| H2 | `b1162b5d` | 0 | sim |
| H3 | `3bdf1927` | 0 | sim |
| H4 | `2bbbda52` | 0 | sim |
| H5 | `a2ec01f0` | 0 | sim |
| R1 | `da79144b` | 0 | sim |
| R2 | `f86b4d17` | 0 | sim |

### Delta desde sync ~18:15

- H4 mergeado na INT até `2bbbda52` (Gt/Geu/Xnor + cancelAll + verdict names;
  36/36) e fatias anteriores (Lt…/AbsDiff…).
- R1 `da79144b` (ThreadStub Windows CI wait harden) mergeado; conteúdo já
  presente na INT.
- Tip INT atual: `7e4a5baa`. Build `46053000345` @ `6cf7cd4b` ainda
  **in_progress** (intacto); política: 1 Build na tip final após terminal.
- Frentes **ahead=0**; H3 sem commits novos.

### Vereditos

- **Hold frentes** (nada ahead). Sem conflito/bloqueio de merge.
- Sem comentários em PR (fila vazia). Sem `build.yml` neste ciclo docs.

## Revisão sync 27/11/2026 ~19:30 UTC

Bases: INT **`a5f91c9b`**. Frentes **não** editadas; **sem** merge em `main`;
**sem** disparo de Actions neste ciclo. PRs GitHub com base na integração:
**nenhum**.

| Frente | Tip remoto | vs INT | Ancestral INT |
| --- | --- | ---: | --- |
| H0 | `c124f899` | 0 | sim |
| H1 | `4d014af2` | 0 | sim |
| H2 | `b1162b5d` | 0 | sim |
| H3 | `3bdf1927` | 0 | sim |
| H4 | `b3e61151` | 0 | sim |
| H5 | `a2ec01f0` | 0 | sim |
| R1 | `da79144b` | 0 | sim |
| R2 | `f86b4d17` | 0 | sim |

### Delta desde sync ~19:00

- H4 mergeado continuamente na INT até `b3e61151` (Neg/Abs/RolBytes +
  fingerprintStagingSnapshot; 54/54) e fatias anteriores (BitSet…/Sext…/Ilog2…).
- R1 ThreadStub wait harden validado: Build `46055210354` @ `deb9c018`
  **SUCCESS**, `WorldRegionThreadStubTest` **PASSED**.
- Tip INT atual: `a5f91c9b`. Build `46057059059` @ tip **in_progress**.
- Frentes **ahead=0**; H3 sem commits novos.
- Política H4: asset-free / **não hold**; merge lotes quando pedirem; smoke hold.

### Vereditos

- **Hold frentes** (nada ahead). Sem conflito/bloqueio.
- H4 continua (próximo lote quando H4 entregar). Sem comentários em PR.
- Sem `build.yml` neste ciclo docs (Build tip já em voo).

## Sync ~20:00 UTC

- Tip INT pós-merge H4 `32414ed0`: `0e08cc90` (flow doc + merge `ffe2b640`).
- Ahead h*/r* = 0; sem conflito/bloqueio. Branch remota H3 ausente (não bloqueio).
- Build `46059053926` @ `6350308f` **SUCCESS** (Windows). Disparado 1 Build
  `46060790428` @ tip `0e08cc90`.

## Revisão sync 27/11/2026 ~20:00 UTC

Bases: INT **`0e08cc90`**. Frentes **não** editadas; **sem** merge em `main`;
**sem** disparo de Actions neste ciclo de revisão (Build tip já disparado
após terminal do `460590`). PRs GitHub com base na integração: **nenhum**.

| Frente | Tip remoto | vs INT | Ancestral INT |
| --- | --- | ---: | --- |
| H0 | `c124f899` | 0 | sim |
| H1 | `4d014af2` | 0 | sim |
| H2 | `b1162b5d` | 0 | sim |
| H3 | _(remote ausente)_ | — | — |
| H4 | `32414ed0` | 0 | sim |
| H5 | `a2ec01f0` | 0 | sim |
| R1 | `da79144b` | 0 | sim |
| R2 | `f86b4d17` | 0 | sim |

### Delta desde sync ~19:30

- H4 mergeado até `32414ed0` (SmearMsb/ByteOr/ByteAnd + listReady; 72/72) e
  fatias SmearLsb…/ClearLsb…/Isolate…/Dup16…/RevNibbles…/SwapHalves….
- Build `46059053926` @ `6350308f` **SUCCESS**; 1 Build `46060790428` @ tip.
- Frentes **ahead=0**; H3 remote branch ausente (não bloqueio de merge).
- Política H4: asset-free / **não hold**; merge lotes quando pedirem.

### Vereditos

- **Hold frentes** (nada ahead). Sem conflito/bloqueio.
- H4 continua. Sem comentários em PR.
- Sem `build.yml` neste ciclo docs (Build tip já em voo).

## Revisão sync 27/11/2026 ~20:30 UTC

Bases: INT **`ba8e7be5`**. Frentes **não** editadas; **sem** merge em `main`;
**sem** disparo de Actions neste ciclo de revisão. PRs GitHub com base na
integração: **nenhum**.

| Frente | Tip remoto | vs INT | Ancestral INT |
| --- | --- | ---: | --- |
| H0 | `c124f899` | 0 | sim |
| H1 | `4d014af2` | 0 | sim |
| H2 | `b1162b5d` | 0 | sim |
| H3 | _(remote ausente)_ | — | — |
| H4 | `2b185c3c` | 0 | sim |
| H5 | `a2ec01f0` | 0 | sim |
| R1 | `da79144b` | 0 | sim |
| R2 | `f86b4d17` | 0 | sim |

### Delta desde sync ~20:00

- H4 mergeado até `2b185c3c` (SwapAdjacent/Rol/RorNibbles + anyCancelled; 86/86)
  e fatias Extract/Insert/BroadcastNibble…/NibbleSum…/NibbleXor…/Interleave…/
  Compress…/KeepBits….
- Build `46060790428` @ `0e08cc90` **PASS** (ThreadStub PASS); Build
  `46062954171` @ `bf7aab20` **in_progress** — 1 Build tip final após terminal.
- Frentes **ahead=0**; H3 remote ausente (não bloqueio).
- Política H4: asset-free / **não hold**.

### Vereditos

- **Hold frentes** (nada ahead). Sem conflito/bloqueio.
- H4 continua. Sem comentários em PR.
- Sem `build.yml` neste ciclo docs (Build tip já em voo).

## Revisão 2026-11-27T21:02Z
- INT tip pré-merge: `eeb70ccd`; pós-merge H4 `0eb034a9` → tip nova (este commit).
- Ahead vs INT pré-merge: H0/H1/H2/H3/H5/R1/R2 = 0; H4 = 3 (`0eb034a9` HalfDiv/Mod/CeilDiv + countFailedOpenKernels).
- Build `46067036173` @ `2d4b047f` in_progress — intacto; 1 Build tip final após terminal; sem spam.
- Conflito só `progress.md` (padrão 4 blocos); reconciliado. Sem PRs abertos para INT nesta checagem.
- H3 remoto presente (`3bdf1927`) ahead=0.

## Revisão 2026-11-27T21:05Z
- Merge H4 `e9aadc63` (HalfIsDivisible/HalfGcd/HalfSelect + anyFailedOpenKernels; 112/112).
- Build `46067036173` @ `2d4b047f` intacto; 1 Build tip final após terminal; sem spam.
- Conflito só `progress.md`; reconciliado.

## Revisão 2026-11-27T21:07Z
- Merge H4 `ca61eb1f` (HalfLcm/HalfCoprime/HalfAlignUp + listCancelledOpenKernels; 114/114).
- Build `46067036173` @ `2d4b047f` intacto; 1 Build tip final após terminal; sem spam.
- Conflito só `progress.md`; reconciliado.

## Revisão 2026-11-27T21:10Z
- Merge H4 `66536948` (HalfAlignDown/HalfRotl/HalfRotr + countCancelledOpenKernels; 116/116).
- Build `46067036173` @ `2d4b047f` intacto; 1 Build tip final após terminal; sem spam.
- Conflito só `progress.md`; reconciliado.

## Revisão 2026-11-27T21:12Z
- Merge H4 `3f16abd3` (HalfPopcount/HalfClz/HalfCtz + anyCancelledOpenKernels; 118/118).
- Build `46067036173` @ `2d4b047f` intacto; 1 Build tip final após terminal; sem spam.
- Conflito só `progress.md`; reconciliado.

## Revisão 2026-11-27T21:14Z
- Merge H4 `987e6fca` (HalfParity/HalfBitReverse/HalfBswap + listPendingOpenKernels; 120/120).
- Build `46067036173` @ `2d4b047f` intacto; 1 Build tip final após terminal; sem spam.
- Conflito só `progress.md`; reconciliado.

## Revisão 2026-11-27T21:16Z
- Merge H4 `160170aa` (HalfNot/HalfNeg/HalfAbs + countPendingOpenKernels; 122/122).
- Build `46067036173` @ `2d4b047f` intacto; 1 Build tip final após terminal; sem spam.
- Conflito só `progress.md`; reconciliado.

## Revisão 2026-11-27T21:18Z
- Merge H4 `1a7570cd` (HalfSignMask/HalfIsZero/HalfIsNonzero + anyPendingOpenKernels; 124/124).
- Build `46067036173` @ `2d4b047f` SUCCESS; 1 Build tip final autorizado após este merge.
- Conflito só `progress.md`; reconciliado.

## Revisão 2026-11-27T21:30Z (FREEZE)
- INT tip `41aa2c64` (H4 `1a7570cd` 124/124 absorvido). Ahead h0–h5/r1/r2 = 0; H3 `3bdf1927` ahead=0.
- FREEZE user: STOP merges H4 e novos Build; sem próximas frentes até liberar pós in-game.
- Build em voo único: `46069374906` @ tip (acompanhar; sem spam). Build `460670` SUCCESS (ThreadStub PASS).
- Sem PRs abertos com base na INT nesta checagem. Sem merge; sem dispatch.

## Revisão 2026-11-27T22:00Z (FREEZE)
- INT tip `ef1c6ead`. Ahead h0–h5/r1/r2/H3 = 0. Sem PRs abertos na base INT.
- FREEZE mantido: STOP merges H4 e novos Build; sem frentes até liberar pós in-game.
- Build `46069374906` SUCCESS @ `41aa2c64` (ThreadStub PASS). CI tip docs verdes. Sem dispatch.

## Revisão 2026-11-27T22:20Z
- FREEZE levantado. Merge H4 `653b865f` (HalfClo/HalfCto/HalfIsPow2 + listPendingOpenTicketsByKernel; 126/126) — tip material.
- Policy B: 1 Build na tip material após push; sem spam.

## Revisão 2026-11-27T22:22Z
- Merge H4 `c5620193` (HalfIlog2/HalfNextPow2/HalfMaskLow + pendingOpenTicketsByKernel; 128/128).
- Build `46075958751` @ `a4a84b28` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T22:24Z
- Merge H4 `8b1b9591` (HalfMaskHigh/HalfBitTest/HalfIsolateLsb + anyPendingOpenTicketsByKernel; 130/130).
- Build `46075958751` @ `a4a84b28` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T22:26Z
- Merge H4 `a4792b8f` (HalfIsolateMsb/HalfClearLsb/HalfLsbMask + discardPendingOpenTicketsByKernel; 132/132).
- Build `46075958751` @ `a4a84b28` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T22:28Z
- Merge H4 `d4798e06` (HalfGrayEncode/HalfGrayDecode/HalfSmearLsb + countPendingOpenTickets; 134/134).
- Build `46075958751` @ `a4a84b28` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T22:31Z
- Sync: H4 ahead absorvido `8e48af1c` (136/136). H5 ahead=1 `24bdb5e6` (só anotar). Desktop bootstrap: não apareceu.
- Build `46075958751` @ `a4a84b28` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T22:31Z (front-review)
- INT tip `d8ce2438` (H4 `8e48af1c` 136/136). Ahead: H0–H4/R1/R2/H3=0; H5=`24bdb5e6` ahead=1 (scale metrics bridge — só anotar, sem ready).
- Build `46075958751` in_progress @ `a4a84b28` intacto; 1 Build tip final após terminal; sem spam/dispatch nesta revisão.
- Sem PRs abertos na base INT. Desktop bootstrap: ausente. Sem merge main; sem editar frentes.

## Revisão 2026-11-27T22:34Z
- Merge H4 `3289c13d` (HalfKeepEvenBits/HalfSwapAdjacentBits/HalfCompressOddBits + listReadyOpenTickets; 138/138).
- Build `46075958751` @ `a4a84b28` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T22:35Z
- Merge H4 `90e2138c` (HalfCompressEvenBits/HalfExpandLow8ToOdd/Even + countReadyOpenTickets; 140/140).
- Build `46075958751` @ `a4a84b28` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T22:37Z
- Merge H4 `3bdb3ffe` (HalfInterleaveBytes/HalfDeinterleaveBytes/HalfSwapBytes + anyReadyOpenTickets; 142/142).
- Build `46075958751` @ `a4a84b28` SUCCESS; 1 Build tip final disparado após merge; sem spam.

## Revisão 2026-11-27T22:39Z
- Merge H4 `b94ca245` (HalfNibbleXor/Or/And + discardReadyOpenTickets; 144/144).
- Build `46077733827` @ `b862351c` intacto; 1 Build tip final material após terminal; sem spam.
- H5 Instalaveis `96ab005a` ainda ahead (docs/scripts) — merge separado; sem Build se só script/doc.

## Revisão 2026-11-27T22:44Z
- Merge H4 `3ebdf921` (HalfNibbleSum/Max/Min + discardFailedOpenTickets; 146/146).
- Build `46077733827` @ `b862351c` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T22:45Z
- Merge H4 `9e93aa31` (HalfSwapAdjacentNibbles/Rol/Ror + discardCancelledOpenTickets; 148/148).
- Build `46077733827` @ `b862351c` intacto; 1 Build tip final após terminal; sem spam.

## Política H4 lote — 2026-11-27T22:43Z
- Não mergear H4 tip-a-tip.
- Agrupar 2–3 tips ready (ou janela ~15–20 min) num único merge `--no-ff`.
- 1 Build tip material só após Build em voo terminal; sem spam.
- Catch-up: se houver fila atrasada, merge só o tip H4 mais novo ready; depois esperar lote.
- Estado: INT `43869cf3` absorveu `9e93aa31` (148/148); h4 ahead=0; Build `46077733827` intacto @b862351c.

## Preferência CI paths — 2026-11-27T22:47Z
- User: filtrar **CI Integration** e **CI Sector Runtime** para NÃO rodar em tip só-docs.
- Implementado: `paths` allowlist em `ci-integration.yml` + `ci-sector-runtime.yml`
  (`source/`, `assets/`, `cmake/`, `toolchains/`, `triplets/`, `scripts/`, próprio YAML).
- Ignora tip só `doc/`, progress, flow-doc, `*.md`.
- Build continua material-only (manual); sem spam Build neste tip YAML.
- `workflow_dispatch` permanece sem filtro de paths.

## Revisão 2026-11-27T23:00Z — lote H4 ≥15min
- Merge H4 tip `828a45d2` (agrupa 3 tips: discardTerminal/discardPending/anyPending; 154/154).
- H5: Desktop probe PASS @ `62fba03a` (44/44); `da430a98` sem probe Desktop → corte em `62fba03a`.
- Build `46079365811` intacto; 1 Build tip final após terminal; sem spam.

## Sync 2026-11-27T23:00Z
- Frentes: h0–h4/r1/r2 ahead=0; h5 ahead restante = `da430a98` (sem probe Desktop).
- Merge lote: H4 `828a45d2` + H5 até `62fba03a` (Desktop 44/44).
- Build `46079365811` @ `46a2da76` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T23:02Z — H5 Desktop probe catch-up
- Correção: Desktop validou `da430a98` 46/46 e `ffce0834` 48/48.
- Merge H5 tip `ffce0834` (inclui profile advice) na INT.
- Build `46079365811` intacto; 1 Build tip final após terminal; sem spam.

## Revisão 2026-11-27T23:04Z — H5 f1bddd2b + standby
- Merge H5 `f1bddd2b` (Desktop 49/49 evidence fingerprint).
- H5 em standby depois deste tip.
- Build `46079365811` intacto; 1 Build tip final após terminal; sem spam.

## Front review 2026-11-27T23:04Z
- INT tip: `7d8d0101` (H5 `f1bddd2b` 49/49 + H4 lote `828a45d2`; H5 standby).
- Ahead vs INT: h0–h3/h5/r1/r2 = 0; **H4 ahead=3** tip `929e0147` (HalfBroadcastNibbleAt/Replace/Reverse + listFailedOpenTickets).
- H4 janela ~4 min desde `dbc19fa6` — **hold** (<15 min; sem tip-a-tip).
- H3: sem commits novos.
- PRs abertos base INT: nenhum.
- Build `46079365811` @ `46a2da76` in_progress (intacto); 1 Build tip final após terminal.
- Sem merge main; sem Actions dispatch neste review.

## Revisão 2026-11-27T23:15Z — lote H4 ≥15min
- Merge H4 tip `81a04ab6` (168/168; agrupa 7 tips desde `929e0147`).
- Build `46081346603` @ `edec53e1` intacto; 1 Build tip final após terminal; sem spam.

## Sync 2026-11-27T23:31Z
- Merge H4 tip `5e2e848b` (186/186; inclui `68f8cb3a`+; janela ≥15 min desde `9df5c3e7`).
- Build `46081346603` SUCCESS @ `edec53e1` → 1 Build tip final; sem spam.
- H5 standby; frentes restantes ahead=0 pós-merge H4 (salvo novos).

## Front review 2026-11-27T23:32Z
- INT tip: `9aca17c8` (H4 lote `5e2e848b` 186/186; H5 standby).
- Ahead: h0–h3/h5/r1/r2=0; **H4 ahead=3** tip `a132e680` (HalfLsbMask/KeepOdd/Even + countOpenTickets).
- H4 janela ~0 min desde `9aca17c8` — **hold** (<15 min).
- H3: sem commits novos. PRs base INT: nenhum.
- Build `46083012409` @ `9aca17c8` in_progress. Sem Actions neste review; sem merge main.
