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
| H3 | `cursor/h3-kernels-publication-9f77` | sem commits além da base (ainda) |
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

## Próximo passo do coordenador

Reabrir este relatório a cada push das frentes; quando os PRs existirem no
GitHub, comentar o veredito em cada PR e só então mergear na integração na
ordem acima.
