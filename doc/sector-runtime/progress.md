# Progressão do projeto — percentuais e ordem das tarefas

Atualizado em **27/11/2026**. Branch de integração:
`cursor/integration-sector-runtime-7342` (H0+H1+H2+H3+H4+H5+R1+R2; não `main`).
Tip INT: merge H4 `91dd5d50` (HalfLeu/HalfGeu/HalfAvg + countReadyOpenKernels; 104/104).
Build `46065085671` @ `89c6d6e0` intacto se in_progress; 1 Build tip final após
terminal. H4 asset-free (sem smoke). Sem smoke até base local.
Última H4: GPU-03 HalfLeu/HalfGeu/HalfAvg + GPU-02 countReadyOpenKernels (`91dd5d50`); H4 25%.
Última H5: scale foundations + probe 27/27 RTX (`a2ec01f0`); H5 25%.
Última H3: TMX cache/reload + flora/replaceBiome + decode/codec (`3bdf1927`); H3 25%.
Última R2: AuthoritativeOps + REG-03/04/05 sombra (`f86b4d17`).
Última R1: ThreadStub CPU/OS + wait sleep fix — Build `460629` PASS.
Última H0: MOD-01 EffectBoundary profundo; VAL-02 BiomeItem completo (25%).
Última H1: PreparedSector payload/WorldStorage path (`4d014af2`); DAG-03 50%.
Última H2: consume sobre API H1 (`b1162b5d`); PreparedSector canônico = H1.

[Programa principal](README.md) · [Roadmap e dependências](roadmap.md)
· [Relatório H0](../sector-runtime-h0-effect-boundary-deep-2026-11-27.md)
· [Relatório H1](../sector-runtime-h1-prepared-sector-2026-11-27.md)
· [Relatório H2](../sector-runtime-h2-prepared-sector-consume-2026-11-27.md)
· [Relatório H3](../sector-runtime-h3-tmx-shadow-cache-2026-11-27.md)
· [Relatório H4](../sector-runtime-h4-gpu02-03-halfincl-countreadyopenkernels-2026-11-27.md)
· [Relatório H5](../sector-runtime-h5-scale-foundations-2026-11-27.md)
· [Relatório R1](../sector-runtime-r1-thread-stub-cpu-2026-11-27.md)
· [Relatório R2](../sector-runtime-r2-authoritative-ops-2026-11-27.md)
· [Backlog geral](../performance-backlog.md)

Este é o **painel canônico de progressão** do novo programa de geração/runtime
de setores, adotado em 08/11/2026. O roadmap continua definindo contratos,
dependências e aceitação; este documento traduz seu estado em percentuais e
ordem de trabalho. Não é uma estimativa da conclusão de todo o OpenStarbound.

## Resumo atual

| Indicador | Estado |
| --- | --- |
| Frente ativa | Integração — H0+H1+H2+H3+H4+H5+R1+R2 |
| Progresso de maturidade de H0 | **25,0%** — 6 marcos verificados de 24 |
| Progresso de maturidade de H1 | **27,5%** — 11 marcos verificados de 40 |
| Progresso de maturidade de H2 | **25,0%** — 6 marcos verificados de 24 |
| Progresso de maturidade de H3 | **25,0%** — 10 marcos verificados de 40 |
| Progresso de maturidade de H4 | **25,0%** — 4 marcos verificados de 16 |
| Progresso de maturidade de H5 | **25,0%** — 7 marcos verificados de 28 |
| Progresso de maturidade de R1 | **25,0%** — 3 marcos verificados de 12 |
| Progresso de maturidade de R2 | **25,0%** — 3 marcos verificados de 12 |
| Progresso de maturidade do programa | **25,5%** — 50 marcos verificados de 196 |
| Contratos com avanço registrado | **49 de 49**; todos ainda parciais |
| Contratos integralmente aceitos | **0 de 49 (0%)** no novo programa |
| Última evidência de testes | H0–H5 + R1/R2; H4 104/104 (HalfLeu/HalfGeu/HalfAvg + countReadyOpenKernels + …) + probe GL PASS; H5 27/27 RTX; Build `46065085671` em voo @ `89c6d6e0`; Actions manuais |
| Ganho de setores/s da nova arquitetura | Ainda não demonstrado por benchmark integrado equivalente |
| Smoke / instalador local | Pendente base local do usuário; sem smoke até lá |

**Os 25,5% não significam que o jogo esteja só 25,5% pronto.** O runtime anterior
já possui workers, schedulers, IO/decode e previews. O denominador aqui são os
49 contratos completos do rework, incluindo GPU e simulação regional futura.

## Como os percentuais são calculados

Cada contrato possui quatro marcos cumulativos, de mesmo peso administrativo:

| Percentual | Marco exigido para conceder a nota |
| --- | --- |
| 0% | Sem recorte executável/testado registrado para o novo contrato; desenho ou recurso anterior não basta |
| 25% | Primeiro recorte funcional do contrato implementado, testado no escopo declarado e documentado com commit/evidência |
| 50% | Requisitos completos do contrato implementados e cobertos por testes locais; sem lacunas funcionais conhecidas naquele contrato |
| 75% | Integração, equivalência e recuperação aprovadas nos cenários aplicáveis, incluindo assets/mods/saves/multiplayer e builds necessários |
| 100% | Critérios de aceitação satisfeitos, desempenho/ausência de regressão demonstrados quando aplicável, revisão independente e decisão de adoção documentadas |

Planejamento escrito sozinho não recebe 25%. Uma correção parcial com testes
não recebe 50% por aproximação. Não pular marcos ou atribuir casas decimais a
uma tarefa para sugerir precisão inexistente. Reabrir/reduzir a nota se uma
evidência perder validade, registrando a razão no histórico.

Fórmulas:

- **Contrato:** número de marcos consecutivos verificados × 25%.
- **Fase de entrada:** soma das notas dos contratos nela cadastrados ÷ número
  desses contratos.
- **Programa:** soma das notas dos 49 contratos ÷ 49. Hoje: 1250 ÷ 49 = 25,5%
  arredondado; equivalentemente, 50 ÷ (49 × 4) × 100.

- **Aceitação integral:** contratos em 100% ÷ 49. Hoje: 0%.

São percentuais de **maturidade comprovada**, não de horas, esforço, linhas de
código, memória coberta, ocupação de hardware ou aceleração. Os contratos têm
tamanhos diferentes; os pesos iguais tornam a conta auditável, não uma previsão
de prazo. Não há data de conclusão estimada neste painel.

## Ordem das fases

Cada ID aparece uma única vez no catálogo, na sua fase de primeira entrega.
Alguns contratos continuam em fases posteriores: DAG-04 em H1, CPU-02 em H2,
CPU-05 em H5, MOD-01/IO-03 na frente regional. Não contar novamente o mesmo ID.
Por isso, o percentual do agrupamento não substitui os gates do roadmap nem
obriga concluir todo trabalho transversal para iniciar um protótipo em sombra.

| Ordem | Fase de entrada | Contratos | Marcos verificados / total | Progresso | Condição de avanço |
| ---: | --- | ---: | ---: | ---: | --- |
| 1 | H0 — fundações e validação | 6 | 6 / 24 | 25,0% | Invariantes, limites explicitados, comparador e baseline reproduzível para o escopo que será ampliado |
| 2 | H1 — grafo executivo, CPU e entrega | 10 | 11 / 40 | 27,5% | DAG-03 em 50% sombra; demais 9 contratos H1 em 25%; 75% exige caminho vivo |
| 3 | H2 — produtos residentes e prefetch | 6 | 6 / 24 | 25,0% | RAM-02..IO-01 em 25%; PreparedSector canônico = H1 |
| 4 | H3 — kernels adicionais, estruturas e publicação | 10 | 10 / 40 | 25,0% | CPU-04..IO-05 sombra; equivalência viva/publish pendente |
| 5 | H4 — backend GPU e comparação em sombra | 4 | 4 / 16 | 25,0% | GPU-01…04 a 25%; probe GL PASS RTX 3050; publicação/ganho pendentes |
| 6 | H5 — seleção de hardware e escala | 7 | 7 / 28 | 25,0% | Sete contratos em 25%; backends GPU, campanhas reais e adoção pendentes |
| 7 | R1 — donos regionais e coordenadora | 3 | 3 / 12 | 25,0% | REG-01/02 + WT-05 harness/owner/stub sombra; donas OS reais pendentes |
| 8 | R2 — operações e transferências entre regiões | 3 | 3 / 12 | 25,0% | REG-03/04/05 sombra + AuthoritativeOps; multiplayer vivo pendente |
| — | **Total** | **49** | **50 / 196** | **25,5%** | Não equivale a ganho de desempenho ou autorização de release |

Essa é uma ordem de prioridade/dependência, não uma proibição de frentes
paralelas autorizadas pelo roadmap. Setores comuns permanecem prioritários.
O alvo regional é uma thread dedicada por região lógica 2×2 de setores 32×32,
distinta dos workers de preparação; ainda não está implementado como simulação
autoritativa. GPU é opcional, com fallback CPU completo.

## Próximas ações — fila imediata

0. **Integração:** H4 `91dd5d50` (HalfLeu/HalfGeu/HalfAvg +
   countReadyOpenKernels; 104/104). H4 asset-free (não hold). Build
   `46065085671` @ `89c6d6e0` intacto se in_progress; 1 Build tip final após
   terminal — sem spam. Smoke só após base local.
1. **H0: DAG-01/RAM-01 com MOD-01/VAL-02:** catálogo compacto e binding de fontes
   reais, com clock/binding 1 e scope 2, agora comparados aos leitores reais de
   WorldTemplate/MaterialDatabase e corpo independente com metadata controlada.
   [Contrato](../sector-runtime-base-tiles-contract.md): hooks Root/Assets/material,
   strong handles e checagem pré/pós, não read lease ou commit. Refused não escreve;
   Stale posterior descarta output privado. Catálogo fora do produtor normal;
   Root real agora executa construtor/manutenção/factory, parser de materiais,
   reload/hotReload/loadMods e listeners/falhas/recuperação com fontes mínimas,
   em alvo opt-in padrão OFF. Custom terrain Root-free e config Json lazy já
   compõem o primeiro recorte de CPU-01 (25%). Builder terrestre comparado à
   referência pré-snapshot com assets mínimos. Em 26/11/2026,
   [`WorldGenerationRegionSelectorContext`](../sector-runtime-h0-selector-context-2026-11-26.md)
   e em seguida
   [`WorldGenerationCompositeContext`](../sector-runtime-h0-composite-context-2026-11-26.md)
   e
   [`WorldGenerationSectorShadow`](../sector-runtime-h0-sector-shadow-2026-11-26.md)
   compõem seletores/static inputs/identidade BaseTiles e admitem array privado
   por setor com revalidação e descarte sem publicação; auditoria de defaults
   registrada, não corrigida.    Próximo: gates H0 aplicáveis restantes antes de
   H1/mailbox. Não repetir o mesmo recorte
   de Root, não adotar fita por tile, não mudar cópias/RNG nem normalizar
   defaults auditados sem decisão de compatibilidade. Depois demanda real,
   mailbox/DAG, provider, promoção/fairness e pressão externa. A
   [cobertura de memória](../sector-runtime-memory-envelope.md) não é heap completo;
   não ampliar filas/defaults por teste controlado.
2. **VAL-02, próxima entrega e apoio à primeira:** sequência + payload
   BiomeItem completo em diagnóstico; falta planeta/mods e streaming.
   Manter os controles de colisões de hash como gate de qualquer
   eliminação de cópias.
3. **MOD-01:** catálogo + enforcement por papel (Worker recusa Authoritative)
   + reconstruct POD do composto; defaults auditados sinalizados. Falta
   amarrar call sites de produção e consumo Json/custom-terrain — não VAL-01.
4. **VAL-01 + validação de DAG-01/DAG-04:** executar streaming com rota, save,
   config, assets/mods e estado inicial fixos; medir setores inéditos entregues/s,
   interesse→entrega e resposta do update/frame. Não pedir apenas outra run
   sem definir hipótese e comparador.
5. **H1 nesta branch:** DAG-03 em 50% sombra; demais contratos H1 em 25%.
   Payload/WorldStorage path sombra entregue para H2 (`payloadView` /
   `probeHandoff`). 75%/install vivo só via Project/integração. Não publicar
   no mundo vivo.

RAM-01 não exige contabilizar cada heap do motor antes de qualquer protótipo
H1; exige identificar o escopo coberto, as exclusões e os limites antes da
ampliação correspondente. Esta fila não troca o roadmap por uma prioridade
concorrente.

## Catálogo ordenado das 49 tarefas

Quarenta e oito notas de 25% e uma de 50% (DAG-03) usam as evidências após o catálogo.
Demais 0% significam **sem marco do novo contrato reconhecido nesta leitura do
roadmap**, não ausência de código relacionado no baseline. Não foi feita nova
auditoria de todas as implementações antigas para conceder crédito retroativo.

| Ordem | Entrada | ID / contrato | Tarefa | Progresso | Próxima entrega ou lacuna principal |
| ---: | --- | --- | --- | ---: | --- |
| 1 | H0 | [DAG-01](dependency-scheduler.md) | Identidades estáveis | 25% | IDs de colocações e ticket BaseTiles testados; contexto profundo/integração geral pendentes |
| 2 | H0 | [DAG-04](dependency-scheduler.md) | Conclusões e progresso dentro da passagem | 25% | Mailbox executiva/reação a conclusões; continuação/avanço já corrigidos no recorte |
| 3 | H0 | [RAM-01](memory-and-prefetch.md) | Reserva e contabilização de bytes | 25% | Json retido medido/deduplicado no contexto estático; polígonos/wrappers e admissão/contexto/consumidor globais pendentes |
| 4 | H0 | [VAL-02](validation-and-telemetry.md) | Comparador de outputs e produtores controlados | 25% | BaseTiles + sombra 2×2 + candidatos BiomeItem (incl. TileDamage); falta planeta/mods e streaming |
| 5 | H0 | [MOD-01](compatibility.md) | Classificar operações por efeito | 25% | Catálogo+enforcement+reconstruct POD; falta call sites produção e Json/custom-terrain |
| 6 | H0 | [VAL-01](validation-and-telemetry.md) | Benchmark de streaming | 25% | Rota/save/config fixos, streaming real e A/B equivalente |
| 7 | H1 | [DAG-02](dependency-scheduler.md) | Grafo persistente e demanda compartilhada | 25% | Interesse/unload/antecipação sombra ok; interesse vivo/WorldStorage pendente |
| 8 | H1 | [DAG-03](dependency-scheduler.md) | Prioridade, justiça e distância correta | 50% | TTL/load/eta/unlock/trace sombra ok; 75% exige integração viva |
| 9 | H1 | [DAG-05](dependency-scheduler.md) | Cancelamento, falha e invalidação | 25% | Retry/drain sombra; retenção até término físico no executor vivo pendente |
| 10 | H1 | [CPU-01](cpu-and-stages.md) | Contexto privado de cálculo | 25% | Composto+sombra+handoff+hotReload/loadMods; falta gates H0 antes de H1 |
| 11 | H1 | [CPU-02](cpu-and-stages.md) | Produto PreparedSector | 25% | payload/path H1 + consume H2 sobre API canônica H1; install vivo pendente |
| 12 | H1 | [MD-01](microdungeons-and-tmx.md) | Seleção leve e identidade | 25% | Candidatos sem halo na sombra; path MicroDungeons vivo pendente |
| 13 | H1 | [CPU-03](cpu-and-stages.md) | Executor processual e granularidade | 25% | Scheduler sombra startNext/complete; pool/afinidade/lotes multi-setor pendentes |
| 14 | H1 | [WT-01](worldthread.md) | Serviço de conclusões e drenagem | 25% | drainMailbox sombra; pontos no caminho vivo e fora-de-ordem autoritativo pendentes |
| 15 | H1 | [WT-02](worldthread.md) | Orçamento produtivo e resposta do jogo | 25% | runPass com exit reasons; separação sim/prep/publish viva e overrun tipado pendentes |
| 16 | H1 | [CPU-05](cpu-and-stages.md) | Governador de recursos e perfis | 25% | suggest Balanced/HighThroughput; pools físicas/render/GPU e perfis dinâmicos pendentes |
| 17 | H2 | [RAM-02](memory-and-prefetch.md) | Camadas de cache e invalidação | 25% | Cache por kind/setor/revisões + invalidação; payloads reais pendentes |
| 18 | H2 | [RAM-03](memory-and-prefetch.md) | Política de retenção | 25% | Soft/hard + histerese + demanda/benefício; justiça multi-mundo pendente |
| 19 | H2 | [RAM-04](memory-and-prefetch.md) | Prefetch previsto e dependências | 25% | Plano por velocidade/wrap/orçamento; sem WorldStorage |
| 20 | H2 | [RAM-05](memory-and-prefetch.md) | Cancelamento e reaproveitamento | 25% | Consumidores ativos preservados; limpeza de processo pendente |
| 21 | H2 | [WT-03](worldthread.md) | Entrada síncrona em lote | 25% | Batch plan/admit/ready; sem WorldThread/startup bench |
| 22 | H2 | [IO-01](io-persistence-network.md) | Pipeline único de leitura e promoção | 25% | Ticket Prefetch→Visible; sem SQLite/install |
| 23 | H3 | [CPU-04](cpu-and-stages.md) | Extração por etapa | 25% | CaveLiquid + Finalize normalize/intents sombra; equivalência viva pendente |
| 24 | H3 | [MD-02](microdungeons-and-tmx.md) | Formato intermediário estático TMX | 25% | Produto estático sombra; decode TMX real e MD-03 pendentes |
| 25 | H3 | [MD-03](microdungeons-and-tmx.md) | Ligação e RNG autoritativos | 25% | Plano de escolhas ordenadas sombra; stream RNG vivo e efeitos pendentes |
| 26 | H3 | [MD-04](microdungeons-and-tmx.md) | Comandos, footprint e reserva | 25% | Comandos+footprint+reservas sombra; writer vivo e MD-05 pendentes |
| 27 | H3 | [MD-05](microdungeons-and-tmx.md) | Aplicação mais barata e retomada | 25% | Batches+staging invisível sombra; writer vivo/WT-04 pendentes |
| 28 | H3 | [WT-04](worldthread.md) | Publicação mais barata | 25% | Passagem orçada + dirty diferido sombra; WorldThread vivo pendente |
| 29 | H3 | [IO-02](io-persistence-network.md) | Serviço de banco e transações | 25% | Harness sombra commit/rollback/checkpoint; SQLite vivo pendente |
| 30 | H3 | [IO-03](io-persistence-network.md) | Checkpoint e recuperação | 25% | Unidade lógica + recovery sombra; disco/R1 pendentes |
| 31 | H3 | [IO-04](io-persistence-network.md) | Unload, drain e retorno ao legado | 25% | Unload/drain sombra; GPU/donas reais pendentes |
| 32 | H3 | [IO-05](io-persistence-network.md) | Pacotes e aplicação no cliente | 25% | Pacotes/marcos sombra; rede/cliente vivos pendentes |
| 33 | H4 | [GPU-01](gpu-compute.md) | Interface e capacidades CPU/GPU | 25% | Serviço submit/poll/cancel + CPU reference; probe GL PASS |
| 34 | H4 | [GPU-02](gpu-compute.md) | Contexto e buffers em voo | 25% | Fila/pool/pump/failContext (recover) + harness GL; thread dona WorldThread pendente |
| 35 | H4 | [GPU-03](gpu-compute.md) | Kernels e equivalência | 25% | DenseUint32 …/HalfLt/HalfGt/HalfNe/HalfLeu/HalfGeu/HalfAvg + batch; terreno/ganho pendentes |
| 36 | H4 | [GPU-04](gpu-compute.md) | Comparação em sombra | 25% | compareShadowBatch + compareShadowNamed + summarizeShadowReports + fingerprintShadowSummary + fingerprintIdentity + catalog; Equal RTX; multi-driver/publicação pendentes |
| 37 | H5 | [GPU-05](gpu-compute.md) | Seleção heterogênea e render | 25% | Seletor por custo completo + fallback CPU; backend GPU/ganho medido pendentes |
| 38 | H5 | [VAL-03](validation-and-telemetry.md) | Métricas e marcos de entrega | 25% | Recorder dos marcos; sem bus de telemetria nem Tracy |
| 39 | H5 | [VAL-04](validation-and-telemetry.md) | Matriz de cenários | 25% | Cenários tipados; campanhas reais/streaming pendentes |
| 40 | H5 | [VAL-05](validation-and-telemetry.md) | Gates de adoção e relatório | 25% | Checklist avalia evidência; adoção permanece fechada no parcial |
| 41 | H5 | [MOD-02](compatibility.md) | Equivalência e versionamento | 25% | Épocas/máscaras; sem comparador de mundo completo |
| 42 | H5 | [MOD-03](compatibility.md) | Perfis de execução e fallback | 25% | Migração explícita dos modes existentes; regional exige checkpoint |
| 43 | H5 | [MOD-04](compatibility.md) | Subsistemas e plataformas | 25% | Matriz de suporte; Clang adiado; GPU opcional |
| 44 | R1 | [REG-01](regional-threads.md) | Lifecycle e identidade da dona | 25% | + OS thread CPU stub; sem WorldServer/WorldStorage vivo |
| 45 | R1 | [REG-02](regional-threads.md) | Estado local e visões de borda | 25% | + contorno parcial/ForeignCell; sem estado vivo integrado |
| 46 | R1 | [WT-05](worldthread.md) | Coordenadora do runtime regional | 25% | + perimeter skeleton; sem WorldServerThread |
| 47 | R2 | [REG-03](regional-threads.md) | Tempo e operações entre regiões | 25% | Sombra + gate Authoritative; rede/vivo pendente |
| 48 | R2 | [REG-04](regional-threads.md) | Transferência de entidades | 25% | Handoff + gate Authoritative; índice/vivo pendente |
| 49 | R2 | [REG-05](regional-threads.md) | Falha, checkpoint e custo de escala | 25% | Recovery + gate Authoritative; escala viva pendente |

## Evidências das notas atuais

| Contrato | Evidência que sustenta 25% | Por que não recebe 50% ou mais |
| --- | --- | --- |
| DAG-01 | `d72b7239`, [identidades de colocações](../sector-runtime-h0-placement-identities-2026-11-08.md) e [ticket BaseTiles](../sector-runtime-base-tiles-contract.md), `f7785963` | Não cobre contexto profundo/assets nem todo o contrato de identidade/integração |
| DAG-04 | `2131ead3`, [passagem/continuação](../sector-runtime-h0-streaming-pass-2026-11-09.md) + mailbox sombra em [cancel/mailbox](../sector-runtime-h1-executive-cancel-mailbox-2026-11-27.md) | Mailbox sombra não é reação na mesma passagem ao vivo; permanece 25% |
| RAM-01 | Recortes `5c3c4c8f` a `e076d96a`, [inventário](memory-and-prefetch.md), [envelope](../sector-runtime-memory-envelope.md) e [último relatório](../sector-runtime-h0-real-sector-sampling-2026-11-10.md) | Json v2 acompanha capturas rastreadas; polígonos/wrappers/layout, admissão completa, consumidor/demanda/provider/fairness e pressão externa pendentes |
| VAL-02 | [Harness inicial](../sector-runtime-h0-streaming-pass-2026-11-09.md), [comparador de tiles](../sector-runtime-h0-base-tiles-identity-2026-11-09.md), [kernel blockInfo](../sector-runtime-h0-block-info-kernel-2026-11-10.md) e [amostragem/BaseTiles reais](../sector-runtime-h0-real-sector-sampling-2026-11-10.md) | Noise/caches mutáveis, builder de planeta/assets/mods completos/streaming pendentes; controles de hash continuam gates |
| MOD-01 | Bundles/planning/lookup + [EffectBoundary](../sector-runtime-h0-effect-boundary-2026-11-27.md) + [profundo](../sector-runtime-h0-effect-boundary-deep-2026-11-27.md) | Catálogo+enforcement+reconstruct POD; defaults não normalizados; call sites/Json e integração abertas |
| VAL-01 | [Harness de passagens](../sector-runtime-h0-streaming-pass-2026-11-09.md), `2131ead3` | Primeiro instrumento auxiliar, não benchmark de streaming real/A-B |
| CPU-01 | `bfbbf403` … [seletores](../sector-runtime-h0-selector-context-2026-11-26.md) e [composto](../sector-runtime-h0-composite-context-2026-11-26.md) | Falta gates H0 restantes antes de H1 |
| RAM-02 | [Product revision cache](../sector-runtime-h2-product-revision-cache-2026-11-27.md) | Sem payloads BaseTiles/candidatos reais |
| RAM-03 | [Retenção/prefetch/batch/IO](../sector-runtime-h2-resident-prefetch-batch-io-2026-11-27.md) | Sem justiça multi-mundo nem pressão RSS externa |
| RAM-04 | idem | Sem carga WorldStorage nem encolher por stale de rota ao vivo |
| RAM-05 | idem | Sem limpeza de fechamento de mundo/processo |
| WT-03 | idem | Sem WorldThread nem benchmark de startup |
| IO-01 | idem | Sem SQLite, install de tiles nem callbacks |
| CPU-04 | [CaveLiquid](../sector-runtime-h3-cave-liquid-plan-2026-11-27.md) + [Finalize](../sector-runtime-h3-finalize-normalize-2026-11-27.md) | Sem flora/spawn, sem equivalência viva nem publicação |
| MD-02 | [TMX static product](../sector-runtime-h3-tmx-static-product-2026-11-27.md) | Sem parser TMX real, sem cache de reload |
| MD-03 | [TMX binding plan](../sector-runtime-h3-tmx-binding-plan-2026-11-27.md) | Sem Random vivo, sem spawn/facade |
| MD-04 | [Structure command plan](../sector-runtime-h3-structure-command-plan-2026-11-27.md) | Sem facade viva; cobertura truncada não autoriza commit |
| MD-05 | [Structure batch staging](../sector-runtime-h3-structure-batch-staging-2026-11-27.md) | Sem publish vivo; equivalência de notificações pendente |
| WT-04 | [Publication pass](../sector-runtime-h3-publication-pass-2026-11-27.md) | Sem WorldThread/IO vivo; fronteira confirmada só no staging |
| IO-02 | [Shadow DB](../sector-runtime-h3-shadow-db-2026-11-27.md) | Sem disco/SQLite; SQLite vivo pendente |
| IO-03 | [Shadow checkpoint](../sector-runtime-h3-shadow-checkpoint-2026-11-27.md) | Sem disco/donas reais; recovery vivo pendente |
| IO-04 | [Shadow unload](../sector-runtime-h3-shadow-unload-2026-11-27.md) | Sem GPU/donas reais; unload vivo pendente |
| IO-05 | [Shadow packet](../sector-runtime-h3-shadow-packet-2026-11-27.md) | Sem rede/cliente vivos; compressão/workers pendentes |
| REG-01 | [AuthoritativeOwner](../sector-runtime-r1-authoritative-owner-2026-11-27.md) + [ThreadStubCpu](../sector-runtime-r1-thread-stub-cpu-2026-11-27.md) | Sem WorldServer/WorldStorage vivo; próximo marco exige mundo vivo |
| REG-02 | [Bordas/outline](../sector-runtime-r1-border-outline-epoch-2026-11-27.md) + inventário | Sem cópia de estado vivo do mundo nem equivalência integrada |
| WT-05 | [RuntimeCoordinator](../sector-runtime-r1-border-runtime-2026-11-27.md) | Sem integração WorldServerThread nem thread dedicada por região |
| GPU-01 | [interface+CPU reference](../sector-runtime-h4-gpu01-compute-interface-2026-11-27.md); probe PASS Desktop-Vinicius | Sem 50%: publicação versionada e caminho vivo pendentes |
| GPU-02 | [fila/voo](../sector-runtime-h4-gpu02-03-04-2026-11-27.md) + [failContext](../sector-runtime-h4-gpu02-04-failcontext-reproducer-2026-11-27.md) + [staging/VRAM](../sector-runtime-h4-gpu02-staging-accounting-2026-11-27.md) + [ownerThread](../sector-runtime-h4-gpu02-owner-thread-2026-11-27.md) + [cancel/pollBatch](../sector-runtime-h4-gpu02-03-rotl-cancel-poll-batch-2026-11-27.md) + [completeBatch/kernelName](../sector-runtime-h4-gpu01-02-03-clz-complete-kernelname-2026-11-27.md) + [listOpenTickets](../sector-runtime-h4-gpu02-03-04-eq-fingerprint-opentickets-2026-11-27.md) | Sem WorldThread viva; sem 50% |
| GPU-03 | [Xor/Add](../sector-runtime-h4-gpu02-03-04-2026-11-27.md) + [And/Or/Mul](../sector-runtime-h4-gpu03-dense-and-or-mul-2026-11-27.md) + [Min/Max/Sub](../sector-runtime-h4-gpu03-04-minmax-rng-commitbatch-2026-11-27.md) + [Shl/Shr](../sector-runtime-h4-gpu01-03-04-shl-catalog-shadowbatch-2026-11-27.md) + [Rotl/Rotr/Not](../sector-runtime-h4-gpu02-03-rotl-cancel-poll-batch-2026-11-27.md) + [Clz/Ctz/Popcount](../sector-runtime-h4-gpu01-02-03-clz-complete-kernelname-2026-11-27.md) + [Eq/Ne/BitReverse](../sector-runtime-h4-gpu02-03-04-eq-fingerprint-opentickets-2026-11-27.md) + [Lt/Leu/AbsDiff](../sector-runtime-h4-gpu01-03-04-lt-backend-summary-2026-11-27.md) + [Gt/Geu/Xnor](../sector-runtime-h4-gpu01-02-03-gt-cancelall-verdictnames-2026-11-27.md) + [Nand/Nor/MulHi](../sector-runtime-h4-gpu02-03-nand-ticket-reject-2026-11-27.md) + [AndNot/SatAdd/SatSub](../sector-runtime-h4-gpu01-03-sat-capfingerprint-2026-11-27.md) + [OrNot/Avg/IsPow2](../sector-runtime-h4-gpu02-03-ornot-drain-peak-2026-11-27.md) + [Bswap/Parity/BitTest](../sector-runtime-h4-gpu01-03-bswap-named-backend-2026-11-27.md) + [HasZeroByte/NextPow2/MaskLow](../sector-runtime-h4-gpu02-03-haszerobyte-bykernel-2026-11-27.md) + [Ilog2/BroadcastByte/ByteSum](../sector-runtime-h4-gpu02-03-ilog2-cancelbykernel-2026-11-27.md) + [Sext8/Sext16/ExtractByte](../sector-runtime-h4-gpu02-03-sext-drainbykernel-2026-11-27.md) + [BitSet/BitClear/BitToggle](../sector-runtime-h4-gpu01-03-bitset-submitnamed-2026-11-27.md) + [Neg/Abs/RolBytes](../sector-runtime-h4-gpu02-03-neg-stagingfp-2026-11-27.md) + [RorBytes/SignMask/IsZero](../sector-runtime-h4-gpu02-03-ror-completebykernel-2026-11-27.md) + [IsNonzero/MaskHigh/SatMul](../sector-runtime-h4-gpu03-04-nonzero-shadowfp-2026-11-27.md) + [SwapHalves/Clo/Cto](../sector-runtime-h4-gpu03-04-swaphalves-shadownamed-2026-11-27.md) + [ReverseNibbles/ByteMax/ByteMin](../sector-runtime-h4-gpu02-03-revnibbles-inflightbykernel-2026-11-27.md) + [DupLow16/DupHigh16/ReverseBitsInBytes](../sector-runtime-h4-gpu02-03-dup16-readybykernel-2026-11-27.md) + [IsolateLsb/IsolateMsb/LsbMask](../sector-runtime-h4-gpu02-03-isolate-cancelledbykernel-2026-11-27.md) + [ClearLsb/GrayEncode/GrayDecode](../sector-runtime-h4-gpu02-03-gray-anybykernel-2026-11-27.md) + [SmearLsb/ClearMsb/ByteXor](../sector-runtime-h4-gpu02-03-smear-failedbykernel-2026-11-27.md) + [SmearMsb/ByteOr/ByteAnd](../sector-runtime-h4-gpu02-03-smearmsb-listready-2026-11-27.md) + [KeepOddBits/KeepEvenBits/SwapAdjacentBits](../sector-runtime-h4-gpu02-03-keepbits-listfailed-2026-11-27.md) + [CompressOddBits/CompressEvenBits/ExpandLow16ToOdd](../sector-runtime-h4-gpu02-03-compress-listcancelled-2026-11-27.md) + [ExpandLow16ToEven/InterleaveHalves/DeinterleaveHalves](../sector-runtime-h4-gpu02-03-interleave-listinflight-2026-11-27.md) + [NibbleXor/NibbleOr/NibbleAnd](../sector-runtime-h4-gpu02-03-nibble-anyinflight-2026-11-27.md) + [NibbleSum/NibbleMax/NibbleMin](../sector-runtime-h4-gpu02-03-nibblestats-anyready-2026-11-27.md) + [ExtractNibble/InsertNibble/BroadcastNibble](../sector-runtime-h4-gpu02-03-nibbleops-anyfailed-2026-11-27.md) + [SwapAdjacentNibbles/RolNibbles/RorNibbles](../sector-runtime-h4-gpu02-03-nibblerot-anycancelled-2026-11-27.md) + [HasZeroNibble/SextNibble/CountZeroNibbles](../sector-runtime-h4-gpu02-03-nibblezero-discardready-2026-11-27.md) + [PackLowNibbles/PackHighNibbles/ExpandLowNibbles](../sector-runtime-h4-gpu02-03-nibblepack-discardfailed-2026-11-27.md) + [ExpandHighNibbles/ReplaceNibble/MaskNibbles](../sector-runtime-h4-gpu02-03-nibblemask-discardcancelled-2026-11-27.md) + [AllNibblesEqual/NibbleRange/BroadcastNibbleAt](../sector-runtime-h4-gpu02-03-nibblerange-discardterminal-2026-11-27.md) + [HalfSum/HalfMax/HalfMin](../sector-runtime-h4-gpu02-03-halfstats-listopenkernels-2026-11-27.md) + [HalfXor/HalfOr/HalfAnd](../sector-runtime-h4-gpu02-03-halfbitwise-countopenkernels-2026-11-27.md) + [BroadcastHalf/HalfEq/HalfRange](../sector-runtime-h4-gpu02-03-halfcmp-anyopenkernels-2026-11-27.md) + [HalfLt/HalfGt/HalfNe](../sector-runtime-h4-gpu02-03-halfcmp2-listreadyopenkernels-2026-11-27.md) + [HalfLeu/HalfGeu/HalfAvg](../sector-runtime-h4-gpu02-03-halfincl-countreadyopenkernels-2026-11-27.md) | Sem terreno/bioma nem ganho medido |
| GPU-04 | [compareShadow](../sector-runtime-h4-gpu02-03-04-2026-11-27.md) + [reproducers](../sector-runtime-h4-gpu02-04-failcontext-reproducer-2026-11-27.md) + [commit](../sector-runtime-h4-gpu04-commit-revalidate-2026-11-27.md) + [RNG/commitBatch](../sector-runtime-h4-gpu03-04-minmax-rng-commitbatch-2026-11-27.md) + [catalog/shadowBatch](../sector-runtime-h4-gpu01-03-04-shl-catalog-shadowbatch-2026-11-27.md) + [fingerprintIdentity](../sector-runtime-h4-gpu02-03-04-eq-fingerprint-opentickets-2026-11-27.md) + [summarizeShadowReports](../sector-runtime-h4-gpu01-03-04-lt-backend-summary-2026-11-27.md) + [fingerprintShadowSummary](../sector-runtime-h4-gpu03-04-nonzero-shadowfp-2026-11-27.md) + [compareShadowNamed](../sector-runtime-h4-gpu03-04-swaphalves-shadownamed-2026-11-27.md); Equal RTX | Sem campanha multi-driver / publicação; permanece 25% |
| GPU-05 | [hardware selection](../sector-runtime-h5-hardware-selection-2026-11-27.md), `StarWorldGenerationHardwareSelection.hpp`, 7 testes de contrato | Sem backend GPU, sem custos observados de runtime, sem ganho ponta a ponta nem publicação |
| VAL-03 | [fundações H5](../sector-runtime-h5-scale-foundations-2026-11-27.md), delivery metrics recorder | Sem instrumentação no WorldThread/Tracy nem cobertura completa de pipeline |
| VAL-04 | mesma entrega, scenario matrix tipada | Sem campanhas executadas nem A/B de streaming |
| VAL-05 | mesma entrega, adoption gates | Adoção permanece falsa sob evidência parcial; sem gate de pipeline completo |
| MOD-02 | mesma entrega, equivalence version/mask | Sem comparador de saves/multiplayer/conteúdo completo |
| MOD-03 | mesma entrega, execution profile binding | Sem ativação regional nem migração de config persistida |
| MOD-04 | mesma entrega, platform/subsystem matrix | Sem builds por plataforma nem matriz CI |
| CPU-05 | mesma entrega, resource governor advice | Sem resize ao vivo de WorkerPool nem orçamento processual ligado |

R1 REG-01/02/WT-05 em 25% harness/owner/stub sombra; donas autoritativas OS reais pendentes.
PreparedSector na integração segue a API H1.

CPU-01 e os demais contratos H1 (exceto DAG-03) permanecem em 25% sombra.
DAG-03 alcançou 50% com TTL/carga/herança/eta/trace locais; 75% continua
bloqueado sem caminho vivo.
H2/RAM-02..IO-01, H3/CPU-04..IO-05 e H4/GPU-01…04 recebem o primeiro marco;
PreparedSector na integração segue a API H1. Probe GL PASS não eleva H4 a 50%.

Os números de testes e seus limites provêm dos relatórios das respectivas etapas,
no cenário NoAssets, não de streaming integrado. A criação original do painel
foi documental; os avanços posteriores de runtime estão associados aos commits/
relatórios do histórico. Atualizar este painel não acrescenta uma medição de
desempenho nem muda prioridades do roadmap, defaults, workflows ou adoção.

## Política de atualização e histórico

Após uma entrega material ou mudança de escopo/prioridade, atualizar este
painel junto com roadmap, relatório e checklist. A regra também consta no
[AGENTS.md](../../AGENTS.md).

1. Ler o contrato e o último relatório, confirmar commit/branch e limitações.
2. Só mudar a nota quando o próximo marco cumulativo tiver evidência explícita.
   Uma entrega parcial pode exigir atualização de estado sem alterar o percentual.
3. Registrar próximo passo, dependências, evidência e data. Não marcar 100%
   por build verde, push ou quantidade de testes; Actions seguem manuais.
4. Conferir os 49 IDs únicos, ordem 1–49 sem lacunas, totais por fase e fórmulas.
   Se o roadmap criar/remover contratos, atualizar o denominador e explicar
   a mudança: queda por escopo maior não é regressão de código.
5. Preservar o histórico abaixo e os relatórios anteriores. Não editar notas
   antigas para simular uma série temporal que não foi registrada.

| Data | Base técnica | Movimento | Nota do programa |
| --- | --- | --- | ---: |
| 09/11/2026 | `bcbe56c6` / implementação `cad311d1` | Primeiro levantamento formal: 6 contratos com marco inicial, 43 sem marco novo reconhecido, 0 integralmente aceitos | 3,1% |
| 09/11/2026 | Implementação `25cecf95` | RAM-01: backing conhecido de candidatos/factory e campos públicos dos seis variants; 223 testes/15 grupos. Continua parcial: nenhum marco adicional completo, RAM-01/H0 mantidos em 25% | 3,1% |
| 09/11/2026 | Implementação `c4d7ddb3` | RAM-01: listas potenciais, cópia de validação e scratch com ownership/registro agregado; 246 testes/16 grupos. Cobertura parcial e teto pendente, sem novo marco completo; RAM-01/H0 mantidos em 25% | 3,1% |
| 09/11/2026 | Implementação `730e36ba` | RAM-01: buckets completos de nomes e pares ocupados de fatores de dano; 266 testes/17 grupos. Json/nós e orçamento profundo seguem parciais, sem novo marco completo; RAM-01/H0 mantidos em 25% | 3,1% |
| 09/11/2026 | Implementação `45db2402` | RAM-01: JsonObject com buckets/chaves por owner e omissão limitada sem tamanho conflitante; 294 testes/18 grupos. Coleta diagnóstica, variants/nós e teto pendentes, sem novo marco completo; RAM-01/H0 mantidos em 25% | 3,1% |
| 09/11/2026 | Implementação `49a12fbf` | RAM-01: Json dos variants enfileirados acompanha original/bundle/consumidores sem rescan; 320 testes/19 grupos. Coleta diagnóstica e escopo parcial; consolidar envelope/pressão sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 09/11/2026 | Implementações `ada755bf`/`0915af25` | RAM-01: envelope das 14 classes e modelo isolado de pressão/histerese, preservado em entradas inválidas; 354 testes/20 grupos. Só métricas no runtime, reserva/controlador e progresso pendentes; sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 09/11/2026 | Implementação `4b5aed18` | RAM-01: ledger opt-in com check/charge atômicos e demanda em tokens/allocators; 388 testes/21 grupos. Singleton desligado, reserva composta e progresso pendentes; sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 09/11/2026 | Implementação `dd59f3b4` | RAM-01: batch e BaseTiles com array/template inicial antes do dispatch; cache sem recobrança e isolado por ledger/demanda. 420 testes/22 grupos; saída variável/progresso real pendentes, pressão desligada. Sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 09/11/2026 | Implementação `8b68ab6a` | RAM-01: status atômicos e retry limitado de produtores puros, com fences físicas e dois jobs/resultados retidos. 452 testes/23 grupos; falta integração/eventos/demanda real, pressão desligada. Sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 09/11/2026 | Implementação `8e190742` | RAM-01/MOD-01/VAL-02: sinal de capacidade com marker atômico e retry 2, subkernel de líquido real comparado à lógica anterior. 484 testes/25 grupos; identidade/contexto e consumidor completo pendentes, pressão desligada. Sem novo marco completo, H0 em 25% | 3,1% |
| 09/11/2026 | Implementação `f7785963` | H0: ticket BaseTiles no fluxo real e comparador dos campos inicializados de tiles, com guards vivos preservados. 524 testes/26 grupos; contexto profundo/produtor completo pendentes e biomeTransition/default/cópia herdado registrado, não corrigido. Sem novo marco completo, H0 em 25% | 3,1% |
| 09/11/2026 | Implementação `2ea41cf8` | H0: input escalar e adaptador de consultas BaseTiles preservam sequência, NoMod e estado parcial nas exceções. 548 testes/27 grupos; resolver/contexto profundo e replay do produtor completo pendentes. Sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 09/11/2026 | Implementação `abfc3834` | H0: observações versionadas/replay protegido, API diagnóstica fora do produtor normal. 575 testes/28 grupos; microcusto separado revelou overhead/176 bytes por tile, próximo contexto compartilhado/compacto com revisão/assets. Sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 09/11/2026 | Implementação `4e5cc6c1` | H0: catálogo selado de respostas únicas/scope, capacidade explícita e preflight sem heap. 601 testes/29 grupos; redução de storage sintética, gerações/lifetime/leitores reais e integração pendentes. Sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 10/11/2026 | Implementação `5413e13a` (testada em 09/11) | H0: clocks reais em Root/Assets, versão de materiais, binding forte e scope 2. 629 testes/31 grupos com fontes controladas; leitores/reload/assets reais e consumidor do catálogo ainda pendentes. Encerramento documental sem novo marco completo, RAM-01/H0 em 25% | 3,1% |
| 10/11/2026 | Implementação `061ce68c` | H0: mesmos leitores de produção comparados a corpo independente/catálogo/fita, com metadata controlada; exceções/NoMod/revisão de layout e 1.024 tiles em quatro workers. 649 testes/32 grupos, 3.300 execuções adicionais; não Root/reload/assets ou streaming integrado. Sem marco adicional, RAM-01/H0 em 25% | 3,1% |
| 10/11/2026 | Implementação `571bb01e` | H0: Assets real com arquivos locais, hotReload/cache/invalidação/retenção e workers; corrige fill vazio de ByteArray e janela de lost-wakeup no stop. 666 testes/34 grupos, 3.640 execuções adicionais; Root real/listeners/streaming ainda pendentes. Nenhum marco completo adicional, RAM-01/H0 em 25% | 3,1% |
| 10/11/2026 | Implementação `7001ae6d` | H0: Root normal/factory/manutenção, materiais carregados, reload/hotReload/loadMods e listeners/falhas/recuperação/retire reais com arquivos mínimos. Corrige seed automática preservando sequências explícitas; 669 casos padrão +13 Root opt-in, 3.960 execuções adicionais, 35+1 grupos. Contexto/streaming pendentes, sem novo marco completo; H0/RAM-01 em 25% | 3,1% |
| 10/11/2026 | Implementação `0871d13c` | H0: kernel blockInfo separado do cache no caminho normal; 28 testes novos, 4.096 inputs em um caso, campos/consultas/falhas contra corpo herdado. 697 padrão +13 Root opt-in, 4.325 execuções adicionais, 36+1 grupos, ASan/UBSan. Adapter/layout real e contexto privado pendentes; sem novo marco ou ganho medido, H0/RAM-01 em 25% | 3,1% |
| 10/11/2026 | Implementação `bfbbf403` | Apoio a H0: primeiro contexto proprietário funcional de CPU-01 (0→25%), custom terrain Root-free com geometria real, cópias profundas e leitura após destruição da origem. 26 testes novos; 723 padrão +13 Root opt-in, 4.845 execuções adicionais, 37+1 grupos. Contexto completo/admissão/jobs/streaming pendentes, sem ganho medido; H0/RAM-01 25%, H1 2,5% | 3,6% |
| 10/11/2026 | Implementação `e076d96a` | Config lazy/custom terrain compostos, Json v2 opcional com ownership/deduplicação e comparação no kernel. 29 novos; 752 padrão +13 Root opt-in, 5.425 execuções adicionais, 38+1 grupos. Layout/seletores controlados; admissão/contexto completo/jobs/streaming pendentes. Sem novo marco ou ganho medido; CPU-01/H0/RAM-01 25% | 3,6% |
| 10/11/2026 | Implementação `b0caadce` | Root/construtor normal, layout/Constant/FlatSurface e LRU reais, arrays BaseTiles 32×32 completos comparados; metadata controlada, sem planeta/streaming. 19 novos opt-in; 752 padrão +32 opcionais, 5.520 execuções adicionais, 40 grupos, ASan/UBSan. Perlin/caches mutáveis, contexto/admissão/jobs pendentes; nenhum novo marco ou ganho medido | 3,6% |
| 10/11/2026 | Investigação de RNG | Manutenção de Root explicava interferência na medição; com Root encerrado, fixture preserva RNG. Parâmetros default do builder real consomem uma seed antes do override. 33 casos opt-in e 30 repetições de RNG aprovados com ASan/UBSan; captura/admissão e equivalência integrada pendentes | 3,6% |
| 10/11/2026 | Factory real em três recortes | Comparação dos seletores, caches de cavernas com eviction/reconstrução e falhas/RNG. 764 padrão +49 opcionais, 42 grupos e 15 repetições adicionais, ASan/UBSan; builder de planeta completo e admissão pendentes | 3,6% |
| 10/11/2026 | Builder real de região | Dez cenários com bioma, patch, líquido, cache, falha e BaseTiles, 764 padrão +59 opcionais, ASan/UBSan. Não são dez contratos concluídos; builder procedural/streaming pendentes | 3,6% |
| 10/11/2026 | Builder terrestre em validação | Dez verificações e dois testes da região contra referência anterior; 764 padrão +71 opcionais, ASan/UBSan. Builder terrestre com assets mínimos, Json/reload, RNG e layouts privados; referência independente do planeta, mods/saves/multiplayer e streaming pendentes | 3,6% |
| 10/11/2026 | Referência terrestre em validação | Builder terrestre comparado ao corpo pré-snapshot em três seeds e falha: layout, tiles e próximo RNG; 764 padrão +73 opcionais, ASan/UBSan. Contexto profundo/admissão, assets/mods representativos e streaming pendentes | 3,6% |
| 26/11/2026 | Contexto de seletores | Identidade por admissão + reserva POD antes do RNG legado; 772 padrão, 8 novos, 10 repetições, ASan/UBSan. Auditoria de defaults registrada; composição com static inputs/BaseTiles e sombra pendentes. Sem novo marco completo; CPU-01/H0/RAM-01 em 25% | 3,6% |
| 26/11/2026 | Contexto composto | Seletores+static inputs+identidade BaseTiles no mesmo ledger; 777 padrão, 5 novos, 10 repetições, ASan/UBSan. Comparação Root/builder e sombra pendentes. Sem novo marco completo; CPU-01/H0/RAM-01 em 25% | 3,6% |
| 26/11/2026 | Composto vs região Root | Parâmetros e amostras get() do composto = buildRegion real; 61 RealBlockInfo, 5 repetições, ASan/UBSan. Terrestre/BaseTiles/sombra pendentes. Sem novo marco completo | 3,6% |
| 26/11/2026 | Composto vs BaseTiles terrestre | staticInputs do composto = blockInfo/BaseTiles 32×32 terrestres; 62 RealBlockInfo, 5 repetições. Sombra pendente. Sem novo marco completo | 3,6% |
| 26/11/2026 | Sombra por setor | Admissão BaseTiles privados do composto, matches/descarte sem publicação; 782 padrão (5 novos), 63 RealBlockInfo, ASan/UBSan. Oceano/setor adjacente e noises pendentes. Sem novo marco completo | 3,6% |
| 26/11/2026 | Sombra oceano/adjacente | Oceano e setor {1,0} via composto+sombra; 65 RealBlockInfo, 5 repetições. Noises privados pendentes. Sem novo marco completo | 3,6% |
| 26/11/2026 | Sombra noises/caches | Árvore Cache/Perlin privada + composto/sombra 32×32; 66 RealBlockInfo, 5 repetições. IslandSurface/concorrência pendentes. Sem novo marco completo | 3,6% |
| 26/11/2026 | Sombras concorrentes | Dois setores no mesmo ledger (contrato+Root terrestre); 783 padrão, 67 RealBlockInfo. IslandSurface/pressão pendentes. Sem novo marco completo | 3,6% |
| 26/11/2026 | IslandSurface+pressão | Island sob composto/sombra + ledger apertado; 784 padrão, 68 RealBlockInfo. Stale de template pendente. Sem novo marco completo | 3,6% |
| 26/11/2026 | Sombra template stale | matches falha após bump de generationRevision; payload até discard; 785 padrão, 69 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Handoff task+sombra | WorkerPool + Tasks Stale/Cancelled sem publicação; 787 padrão, 70 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Handoffs concorrentes | Duas tasks/sombras no mesmo ledger; 788 padrão. Sem novo marco completo | 3,6% |
| 26/11/2026 | Handoffs concorrentes Root | Dois setores terrestres com handoff paralelo; 71 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Sombra vs hotReload | LookupSources stale após hotReload; identidade do produto permanece até discard; 72 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Sombra vs loadMods | loadMods deferido + discard; reload aplica override; 73 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Invalidate no handoff | Task invalidate + Cancelled + discard; 789 padrão. Sem novo marco completo | 3,6% |
| 26/11/2026 | Abandono de preparação | TaskPreparation sem handoff + discard; 790 padrão. Sem novo marco completo | 3,6% |
| 26/11/2026 | Abandono Root terrestre | Preparação abandonada após fill 32×32; 74 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Falha de produtor | fail+throw+Failed+discard; 791 padrão. Sem novo marco completo | 3,6% |
| 26/11/2026 | Falha de produtor Root | Fill terrestre + Failed + discard; 75 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | loadMods+reload sob sombra | Override imediato; produto matches até discard; 76 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Invalidate Root terrestre | Fill + invalidate + Cancelled + discard; 77 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | IslandSurface handoff | Island + task Stale + discard; 78 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Oceano handoff | Oceano + task Stale + discard; 79 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Noise handoff | Cache/Perlin + task Stale + discard; 80 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Ocean invalidate | Oceano + invalidate Cancelled + discard; 81 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Ocean abandon/fail | Oceano + preparation abandon + producer Failed; 83 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Island lifecycle | IslandSurface invalidate/abandon/fail; 86 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Noise lifecycle | Cache/Perlin invalidate/abandon/fail; 89 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Adjacent handoff | Setor {1,0} handoff Stale + invalidate; 91 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Adjacent lifecycle | Setor {1,0} abandon + producer Failed; 93 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Ocean/Island hotReload | Sombra sobrevive hotReload até discard; 95 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Ocean/Island loadMods | Sombra sobrevive loadMods até discard; 97 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Noise assets | Cache/Perlin hotReload + loadMods; 99 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Adjacent assets | Setor {1,0} hotReload + loadMods; 101 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Ocean/Island stale | TemplateRevision bump sob sombra oceano/Island; 103 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Noise/adjacent stale | TemplateRevision bump sob noise e setor {1,0}; 105 RealBlockInfo. Sem novo marco completo | 3,6% |
| 26/11/2026 | Concurrent ocean | Duas sombras oceano no ledger; 106 RealBlockInfo. Sem novo marco completo | 3,6% |
| 27/11/2026 | Integração R2 | Merge `--no-ff` R2 `f86b4d17` em `cursor/integration-sector-runtime-7342`; R2 25%; programa 18,4%→19,9% | 19,9% |
| 27/11/2026 | H3 CPU-04 CaveLiquid plan | Plano/delta privado sem publish; 7 testes NoAssets. CPU-04 0→25%; H3 2,5%; programa 4,1% | 4,1% |
| 27/11/2026 | H3 CPU-04 Finalize normalize | Delta normalização privado + estágio FinalizePlan; 7+7 NoAssets. Sem novo marco (CPU-04 permanece 25%) | 4,1% |
| 27/11/2026 | H3 CPU-04 Finalize intents | Placement intents ordenados sem spawn; 8 Finalize NoAssets. Sem novo marco | 4,1% |
| 27/11/2026 | H3 MD-02 TMX static | Produto intermediário camadas/conectores sem RNG/publish; 6 NoAssets. MD-02 0→25%; H3 5%; programa 4,6% | 4,6% |
| 27/11/2026 | H3 MD-03 TMX binding | Escolhas ordenadas sem facade; 5 NoAssets. MD-03 0→25%; H3 7,5%; programa 5,1% | 5,1% |
| 27/11/2026 | H3 MD-04 structure commands | Footprint/comandos/reservas sombra; 6 NoAssets. MD-04 0→25%; H3 10%; programa 5,6% | 5,6% |
| 27/11/2026 | H3 MD-05 batch staging | Batches ordenados + staging invisível/resume; 6 NoAssets. MD-05 0→25%; H3 12,5%; programa 6,1% | 6,1% |
| 27/11/2026 | H3 WT-04 publication pass | Orçamento de passagem + dirty diferido sombra; 5 NoAssets. WT-04 0→25%; H3 15%; programa 6,6% | 6,6% |
| 27/11/2026 | H3 IO-02 shadow DB | Harness commit/rollback/checkpoint + frontier blob; 7 NoAssets. IO-02 0→25%; H3 17,5%; programa 7,1% | 7,1% |
| 27/11/2026 | H3 IO-03 shadow checkpoint | Unidade lógica + seal/recover/reject; 6 NoAssets. IO-03 0→25%; H3 20%; programa 7,7% | 7,7% |
| 27/11/2026 | H3 IO-04 shadow unload | Unload/drain/keepAlive/handoff bloqueado; 6 NoAssets. IO-04 0→25%; H3 22,5%; programa 8,2% | 8,2% |
| 27/11/2026 | H3 IO-05 shadow packet | Pacotes confirmados + marcos + clocks; 6 NoAssets. IO-05 0→25%; H3 25%; programa 8,7% | 8,7% |
| 27/11/2026 | H3 shadow integration | WT-04 frontier→IO-03→IO-05; 4 NoAssets. Sem novo marco % (contratos seguem 25%) | 8,7% |
| 27/11/2026 | H3 CPU-04 flora/spawn | Grass-mod + spawn intents sombra; 6 NoAssets. Sem novo marco % (CPU-04 25%) | 8,7% |
| 27/11/2026 | H3 CPU-04 replaceBiome | Placeholders→materiais/hues sombra; 6 NoAssets. Sem novo marco % (CPU-04 25%) | 8,7% |
| 27/11/2026 | H3 MD-02 TMX Json decode | Decode Json controlado→produto estático; 6 NoAssets. Sem novo marco % (MD-02 25%) | 8,7% |
| 27/11/2026 | H3 MD-02 TMX codec | gzip/zlib/zstd base64 + flip strip; 9 NoAssets. Sem novo marco % (MD-02 25%) | 8,7% |
| 27/11/2026 | H3 MD-02 TMX Assets-fixture | Store in-memory + tileset refs/hash; NoAssets. Sem novo marco % (MD-02 25%) | 8,7% |
| 27/11/2026 | H3 MD-02 TMX cache/reload | Budget LRU + invalidate/reload epoch; 8 NoAssets. Sem novo marco % (MD-02 25%) | 8,7% |
| 27/11/2026 | Integração H3 delta | Merge `--no-ff` H3 `3bdf1927` (flora/replaceBiome/TMX decode/cache) em INT; H3 permanece 25% | 19,9% |
| 27/11/2026 | GPU-01 interface | `WorldGenerationComputeService` + CPU `DenseUint32Xor`; 6 testes. GPU-01 0→25%; H4 6,3% | 4,1% |
| 27/11/2026 | GPU-02/03/04 CPU-first | Fila/batch/sombra; 14 testes. GPU-02/03/04 0→25%; H4 25% | 5,6% |
| 27/11/2026 | GPU probe harness opt-in | `STAR_ENABLE_WORLDGEN_GL_COMPUTE`, shaders DenseUint32*, checklist Windows/RTX; H4 permanece 25% até PASS local | 5,6% |
| 27/11/2026 | Integração H4 | Merge `--no-ff` H4 `c44b1a38` (probe PASS `c7f6e605` + gtest opt-in) em INT; H4 25%; programa 19,9%→21,9% | 21,9% |
| 27/11/2026 | H5 GPU-05 seletor | `WorldGenerationHardwareSelector` por custo completo + fallback CPU; 7 testes. GPU-05 0→25%; H5 0→3,6%; programa 3,6→4,1% | 4,1% |
| 27/11/2026 | H5 fundações VAL/MOD + CPU-05 | Métricas, cenários, gates, perfis, equivalência, plataformas, governador; 27 testes. H5 25%; H1 5%; programa 4,1→7,7% | 7,7% |
| 27/11/2026 | Integração H5 | Merge `--no-ff` H5 `a2ec01f0` (probe 27/27 @ `254cc22e` + PS1 fix) em INT; tip `33628d9b`; Build #86 + CI #19 SUCCESS; H5 25%; programa 21,9%→25,5% | 25,5% |
| 27/11/2026 | CI Sector Runtime | Merge PR#3 (`ci-sector-runtime.yml`) → tip `8155297f`; CI Integration #20 + CI Sector Runtime #2 SUCCESS; Build #86 @ `33628d9b` permanece instalador; tip YAML ≠ tip material (`build.yml` não disparado); sem smoke até base local; % inalterado | 25,5% |
| 27/11/2026 | GPU-03 And/Or/Mul | Família densa asset-free ampliada (CPU+GL opt-in); 16/16 WorldGenerationComputeTest; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 GPU-03 | Merge `--no-ff` H4 `7754b2ca` em INT; tip material → Build autorizado; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-02/04 failContext+reproducers | recover failContext CPU + shadowEdgeCorpus/Reproducer; 17/17 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 GPU-02/04 | Merge `--no-ff` H4 `6b210ba0` em INT; Build `4741d691` permanece; 1 Build na tip nova após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-04 commit revalidate | fingerprintInput/commit revalidate gate; 18/18 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 GPU-04 revalidate | Merge `--no-ff` H4 `2f140d39` em INT; Build `4741d691` intacto; 1 Build na tip final após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-02 staging/VRAM | stagingSnapshot/peak/VRAM accounting; 19/19 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 GPU-02 staging | Merge `--no-ff` H4 `29a45b8a` em INT; Build `4741d691` intacto; 1 Build tip final após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-02 adoptOwnerThread | Affinity de mutações compute na thread dona; 20/20 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 GPU-02 owner | Merge `--no-ff` H4 `ef0e7f7c` em INT; Build `4741d691` intacto; 1 Build tip final após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Min/Max/Sub | DenseUint32 Min/Max/Sub asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-04 RNG/commitBatch | Approved RNG streams + commitBatch; 23/17→23 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 8c1bf6e6 | Merge `--no-ff` 3 commits H4 (Min/Max/Sub, RNG/commitBatch, docs) em INT; Build `4741d691` intacto; 1 Build tip final após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Shl/Shr | DenseUint32 Shl/Shr asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-04 catalog/shadowBatch | supportedKernels + compareShadowBatch; 26/26 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 2d4d3d21 | Merge `--no-ff` 3 commits H4 em INT; Build `46051218699` @ `312ff214` intacto; 1 Build tip nova após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Rotl/Rotr/Not | DenseUint32 Rotl/Rotr/Not asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 cancel/pollBatch | cancelBatch + pollBatch ticket control; 28/28 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 43c24859 | Merge `--no-ff` 3 commits H4 em INT; Build `46051218699` @ `312ff214` intacto; 1 Build tip final após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Clz/Ctz/Popcount | DenseUint32 Clz/Ctz/Popcount asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 completeBatch/kernelName | completeBatch + kernel name catalog; 30/30 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 ba1f3ab0 | Merge `--no-ff` 3 commits H4 em INT; Build `46051218699` @ `312ff214` intacto; 1 Build tip final após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Eq/Ne/BitReverse | DenseUint32 Eq/Ne/BitReverse asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02/04 fingerprint/opentickets | fingerprintIdentity + listOpenTickets; 32/32 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 ce4ced16 | Merge `--no-ff` 3 commits H4 em INT; Build `46051218699` @ `312ff214` intacto; 1 Build tip final após terminal; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Lt/Leu/AbsDiff | DenseUint32 Lt/Leu/AbsDiff asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-01/04 backend/summary | backendName + summarizeShadowReports; 34/34 testes; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 877d3e24 | Merge `--no-ff` 3 commits H4 em INT; Build `312ff214` FAILURE ThreadStub; wait sleep fix R1; programa 25,5% | 25,5% |
| 27/11/2026 | R1 ThreadStub CI flake | Wait loops usam Thread::sleep(1) em vez de yield puro (Windows CI); % inalterado | 25,5% |
| 27/11/2026 | GPU-03 Gt/Geu/Xnor | DenseUint32 Gt/Geu/Xnor asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-01/02 cancelAll/verdicts | cancelAll + poll/shadow/commit verdict names; 36/36; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 2bbbda52 | Merge `--no-ff` 3 commits H4 em INT; Build `460530` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Nand/Nor/MulHi | DenseUint32 Nand/Nor/MulHi asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 ticket helpers | ticketIsOpen + rejectedDuplicates; 38/38; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 32ecbf02 | Merge `--no-ff` 3 commits H4 em INT; Build `460530` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 AndNot/Sat* | DenseUint32 AndNot/SatAdd/SatSub asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-01 cap fingerprint | fingerprintCapabilities + supportedKernelCount; 40/40; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 dd6065c7 | Merge `--no-ff` 3 commits H4 em INT; Build `460530` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 OrNot/Avg/IsPow2 | DenseUint32 OrNot/Avg/IsPow2 asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 drain/peak tickets | drainOpenTickets + peakOpenTickets; 42/42; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 fd7f6ff4 | Merge `--no-ff` 3 commits H4 em INT; Build `460530` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Bswap/Parity/BitTest | DenseUint32 Bswap/Parity/BitTest asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-01 named/backend helpers | runNamedKernel + isBackendSupported; 44/44; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 da285a29 | Merge `--no-ff` 3 commits H4 em INT; Build `460530` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 HasZeroByte/NextPow2/MaskLow | DenseUint32 HasZeroByte/NextPow2/MaskLow asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 by-kernel tickets | listOpenTicketsByKernel + countOpenTicketsByKernel; 46/46; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 5c4c3216 | Merge `--no-ff` 3 commits H4 em INT; Build `460552` @deb9c018 intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Ilog2/BroadcastByte/ByteSum | DenseUint32 Ilog2/BroadcastByte/ByteSum asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 cancel-by-kernel | cancelOpenTicketsByKernel; 48/48; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 d95fdcc5 | Merge `--no-ff` 3 commits H4 em INT; Build `460552` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Sext8/Sext16/ExtractByte | DenseUint32 Sext8/Sext16/ExtractByte asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 drain-by-kernel | drainOpenTicketsByKernel; 50/50; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 0d6753e1 | Merge `--no-ff` 3 commits H4 em INT; Build `460552` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 BitSet/BitClear/BitToggle | DenseUint32 BitSet/BitClear/BitToggle asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-01 submitNamed | submitNamed ASCII kernel tickets; 52/52; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 573296c7 | Merge `--no-ff` 3 commits H4 em INT; Build `460552` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Neg/Abs/RolBytes | DenseUint32 Neg/Abs/RolBytes asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 staging fingerprint | fingerprintStagingSnapshot; 54/54; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 b3e61151 | Merge `--no-ff` 3 commits H4; Build `460552` SUCCESS ThreadStub PASS; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 RorBytes/SignMask/IsZero | DenseUint32 RorBytes/SignMask/IsZero asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 complete-by-kernel | completeOpenTicketsByKernel; 56/56; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 a653e2ed | Merge `--no-ff` 3 commits H4 em INT; Build `460570` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 IsNonzero/MaskHigh/SatMul | DenseUint32 IsNonzero/MaskHigh/SatMul asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-04 shadow fingerprint | fingerprintShadowSummary; 58/58; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 e366452d | Merge `--no-ff` 3 commits H4 em INT; Build `460570` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 SwapHalves/Clo/Cto | DenseUint32 SwapHalves/Clo/Cto asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-04 compareShadowNamed | compareShadowNamed ASCII kernels; 60/60; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 74f12e85 | Merge `--no-ff` 3 commits H4 em INT; Build `460570` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 ReverseNibbles/ByteMax/ByteMin | DenseUint32 ReverseNibbles/ByteMax/ByteMin asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 inFlightByKernel | inFlightByKernel pending counts; 62/62; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 907305a5 | Merge `--no-ff` 3 commits H4 em INT; Build `460570` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Dup16/ReverseBitsInBytes | DenseUint32 DupLow16/DupHigh16/ReverseBitsInBytes asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 ready-by-kernel | readyOpenTicketsByKernel; 64/64; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 30b43e50 | Merge `--no-ff` 3 commits H4 em INT; Build `460570` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 IsolateLsb/IsolateMsb/LsbMask | DenseUint32 IsolateLsb/IsolateMsb/LsbMask asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 cancelled-by-kernel | cancelledOpenTicketsByKernel; 66/66; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 0364a4bb | Merge `--no-ff` 3 commits H4 em INT; Build `460590` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 ClearLsb/GrayEncode/GrayDecode | DenseUint32 ClearLsb/GrayEncode/GrayDecode asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 any-by-kernel | anyOpenTicketsByKernel; 68/68; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 6fbec84b | Merge `--no-ff` 3 commits H4 em INT; Build `460590` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 SmearLsb/ClearMsb/ByteXor | DenseUint32 SmearLsb/ClearMsb/ByteXor asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 failed-by-kernel | failedOpenTicketsByKernel; 70/70; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 ff592cff | Merge `--no-ff` 3 commits H4 em INT; Build `460590` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 SmearMsb/ByteOr/ByteAnd | DenseUint32 SmearMsb/ByteOr/ByteAnd asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 listReady-by-kernel | listReadyOpenTicketsByKernel; 72/72; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 32414ed0 | Merge `--no-ff` 3 commits H4 em INT; Build `460590` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 KeepOdd/Even/SwapAdj | DenseUint32 KeepOddBits/KeepEvenBits/SwapAdjacentBits asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 listFailed-by-kernel | listFailedOpenTicketsByKernel; 74/74; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 6d02562f | Merge `--no-ff` 3 commits H4 em INT; Build `460607` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Compress/ExpandLow16 | DenseUint32 CompressOddBits/CompressEvenBits/ExpandLow16ToOdd asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 listCancelled-by-kernel | listCancelledOpenTicketsByKernel; 76/76; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 541751dc | Merge `--no-ff` 3 commits H4 em INT; Build `460607` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 ExpandEven/Interleave | DenseUint32 ExpandLow16ToEven/InterleaveHalves/DeinterleaveHalves asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 listInFlight-by-kernel | listInFlightByKernel; 78/78; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 68185068 | Merge `--no-ff` 3 commits H4 em INT; Build `460607` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 NibbleXor/Or/And | DenseUint32 NibbleXor/NibbleOr/NibbleAnd asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 anyInFlight-by-kernel | anyInFlightByKernel; 80/80; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 229460e4 | Merge `--no-ff` 3 commits H4 em INT; Build `460607` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 NibbleSum/Max/Min | DenseUint32 NibbleSum/NibbleMax/NibbleMin asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 anyReady-by-kernel | anyReadyOpenTicketsByKernel; 82/82; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 f64b127c | Merge `--no-ff` 3 commits H4 em INT; Build `460607` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 Extract/Insert/BroadcastNibble | DenseUint32 ExtractNibble/InsertNibble/BroadcastNibble asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 anyFailed-by-kernel | anyFailedOpenTicketsByKernel; 84/84; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 d9bf93c0 | Merge `--no-ff` 3 commits H4 em INT; Build `460607` PASS; Build `460629` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 SwapAdj/Rol/RorNibbles | DenseUint32 SwapAdjacentNibbles/RolNibbles/RorNibbles asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 anyCancelled-by-kernel | anyCancelledOpenTicketsByKernel; 86/86; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 2b185c3c | Merge `--no-ff` 3 commits H4 em INT; Build `460629` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 HasZero/Sext/CountZeroNibbles | DenseUint32 HasZeroNibble/SextNibble/CountZeroNibbles asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 discardReady-by-kernel | discardReadyOpenTicketsByKernel; 88/88; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 8946524d | Merge `--no-ff` 3 commits H4 em INT; Build `460629` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 PackLow/High/ExpandLowNibbles | DenseUint32 PackLowNibbles/PackHighNibbles/ExpandLowNibbles asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 discardFailed-by-kernel | discardFailedOpenTicketsByKernel; 90/90; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 464b2dc4 | Merge `--no-ff` 3 commits H4 em INT; Build `460629` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 ExpandHigh/Replace/MaskNibbles | DenseUint32 ExpandHighNibbles/ReplaceNibble/MaskNibbles asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 discardCancelled-by-kernel | discardCancelledOpenTicketsByKernel; 92/92; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 ccbabd50 | Merge `--no-ff` 3 commits H4 em INT; Build `460629` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 AllEqual/Range/BroadcastAt | DenseUint32 AllNibblesEqual/NibbleRange/BroadcastNibbleAt asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 discardTerminal-by-kernel | discardTerminalOpenTicketsByKernel; 94/94; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 e3dbcf4e | Merge `--no-ff` 3 commits H4 em INT; Build `460650` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 HalfSum/Max/Min | DenseUint32 HalfSum/HalfMax/HalfMin asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 listOpenKernels | listOpenKernels; 96/96; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 d4e11977 | Merge `--no-ff` 3 commits H4 em INT; Build `460650` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 HalfXor/Or/And | DenseUint32 HalfXor/HalfOr/HalfAnd asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 countOpenKernels | countOpenKernels; 98/98; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 5e0a8f71 | Merge `--no-ff` 3 commits H4 em INT; Build `460650` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 BroadcastHalf/HalfEq/HalfRange | DenseUint32 BroadcastHalf/HalfEq/HalfRange asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 anyOpenKernels | anyOpenKernels; 100/100; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 f1d26097 | Merge `--no-ff` 3 commits H4 em INT; Build `460650` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 HalfLt/Gt/Ne | DenseUint32 HalfLt/HalfGt/HalfNe asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 listReadyOpenKernels | listReadyOpenKernels; 102/102; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 452928b9 | Merge `--no-ff` 3 commits H4 em INT; Build `460650` intacto; 1 Build tip final; programa 25,5% | 25,5% |
| 27/11/2026 | GPU-03 HalfLeu/Geu/Avg | DenseUint32 HalfLeu/HalfGeu/HalfAvg asset-free; H4 permanece 25% | 25,5% |
| 27/11/2026 | GPU-02 countReadyOpenKernels | countReadyOpenKernels; 104/104; H4 permanece 25% | 25,5% |
| 27/11/2026 | Integração H4 91dd5d50 | Merge `--no-ff` 3 commits H4 em INT; Build `460650` intacto; 1 Build tip final; programa 25,5% | 25,5% |

Compatibilidade com mods/saves/multiplayer e fallback CPU são gates essenciais.
Externalização de NPCs permanece suspensa; Linux Clang continua adiado.
Oceanos/clima e demais itens históricos permanecem no backlog geral e não
entram silenciosamente no denominador dos 49 contratos deste programa.

Conferência da criação original (`1c6fa74c`): 49 IDs únicos correspondentes aos contratos dos
módulos/roadmap, ordem 1–49 sem lacunas, oito totais de fase e fórmulas
recalculados; 355 links locais em 27 documentos sem destino ausente e todos
os 14 documentos da coleção alcançáveis pelo índice. `git diff --check`
aprovado. Nenhuma mudança em `source/` ou workflows; sem build, reexecução de
testes de runtime ou disparo de Actions naquela entrega documental original.
