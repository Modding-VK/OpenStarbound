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
