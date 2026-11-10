# Progressão do projeto — percentuais e ordem das tarefas

Atualizado em **10/11/2026**. Branch: `Experimental`.
Base inspecionada: `55b2d3691aad116f5421b5206faf170287b471fc`;
última implementação: `b0caadce`.

[Programa principal](README.md) · [Roadmap e dependências](roadmap.md)
· [Último relatório técnico](../sector-runtime-h0-region-selector-capture-2026-11-10.md)
· [Backlog geral](../performance-backlog.md)

Este é o **painel canônico de progressão** do novo programa de geração/runtime
de setores, adotado em 08/11/2026. O roadmap continua definindo contratos,
dependências e aceitação; este documento traduz seu estado em percentuais e
ordem de trabalho. Não é uma estimativa da conclusão de todo o OpenStarbound.

## Resumo atual

| Indicador | Estado |
| --- | --- |
| Frente ativa | H0 — fundações, invariantes e validação acelerada |
| Progresso de maturidade de H0 | **25,0%** — 6 marcos verificados de 24 |
| Progresso de maturidade do programa | **3,6%** — 7 marcos verificados de 196 |
| Contratos com avanço registrado | **7 de 49**; todos ainda parciais |
| Contratos integralmente aceitos | **0 de 49 (0%)** no novo programa |
| Última evidência de testes | 764 casos padrão; captura integrada a buildRegion, quatro testes repetidos dez vezes com ASan/UBSan e objeto normal compilado; builder real completo e Root opt-in pendentes |
| Ganho de setores/s da nova arquitetura | Ainda não demonstrado por benchmark integrado equivalente |

**Os 3,6% não significam que o jogo esteja só 3,6% pronto.** O runtime anterior
já possui workers, schedulers, IO/decode e previews. O denominador aqui são os
49 contratos completos do rework, incluindo GPU e simulação regional futura.
Recortes de RAM-01 podem estar implementados/testados e o contrato abrangente
ainda permanecer em 25%. Quantidade de commits/testes não aumenta essa nota
automaticamente. Ver o [baseline](baseline.md) para o que já existia.

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
- **Programa:** soma das notas dos 49 contratos ÷ 49. Hoje: 175 ÷ 49 = 3,6%
  arredondado; equivalentemente, 7 ÷ (49 × 4) × 100.
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
| 2 | H1 — grafo executivo, CPU e entrega | 10 | 1 / 40 | 2,5% | Primeiro recorte proprietário de CPU-01 em apoio a H0; grafo/executor ainda pendentes |
| 3 | H2 — produtos residentes e prefetch | 6 | 0 / 24 | 0% | Identidade/versionamento de H1 e orçamento de RAM |
| 4 | H3 — kernels adicionais, estruturas e publicação | 10 | 0 / 40 | 0% | Contrato por kernel e publicação validável; TMX não bloqueia setor comum |
| 5 | H4 — backend GPU e comparação em sombra | 4 | 0 / 16 | 0% | Protótipo pode acompanhar H1/H2; publicação depende do kernel CPU equivalente |
| 6 | H5 — seleção de hardware e escala | 7 | 0 / 28 | 0% | Produtos/backends validados e custo completo de entrega medido |
| 7 | R1 — donos regionais e coordenadora | 3 | 0 / 12 | 0% | Auditoria de autoridade/efeitos e checkpoint; não basta aprovar GPU |
| 8 | R2 — operações e transferências entre regiões | 3 | 0 / 12 | 0% | Lifecycle/estado de R1, recovery e matriz de multiplayer |
| — | **Total** | **49** | **7 / 196** | **3,6%** | Não equivale a ganho de desempenho ou autorização de release |

Essa é uma ordem de prioridade/dependência, não uma proibição de frentes
paralelas autorizadas pelo roadmap. Setores comuns permanecem prioritários.
O alvo regional é uma thread dedicada por região lógica 2×2 de setores 32×32,
distinta dos workers de preparação; ainda não está implementado como simulação
autoritativa. GPU é opcional, com fallback CPU completo.

## Próximas ações — fila imediata

1. **H0: DAG-01/RAM-01 com MOD-01/VAL-02:** catálogo compacto e binding de fontes
   reais, com clock/binding 1 e scope 2, agora comparados aos leitores reais de
   WorldTemplate/MaterialDatabase e corpo independente com metadata controlada.
   [Contrato](../sector-runtime-base-tiles-contract.md): hooks Root/Assets/material,
   strong handles e checagem pré/pós, não read lease ou commit. Refused não escreve;
   Stale posterior descarta output privado. Catálogo fora do produtor normal;
   Root real agora executa construtor/manutenção/factory, parser de materiais,
   reload/hotReload/loadMods e listeners/falhas/recuperação com fontes mínimas,
   em alvo opt-in padrão OFF. Versão de reload não congela conteúdo renovado por
   clearCache; capturar os inputs lidos. Fill vazio, stop Assets e overflow da seed
   automática corrigidos. O kernel de blockInfo já foi separado do cache no
   caminho normal, com fontes controladas em teste. Custom terrain já possui
   captura proprietária Root-free, inclusive após destruir a origem: primeiro
   recorte de CPU-01 em 25%, sem adoção nos jobs ou orçamento novo. Config Json
   lazy já compõe essa captura com lifetime/modelo Json v2 opcional. Agora o
   construtor normal, layout/Constant/FlatSurface e LRU reais são comparados e
   produzem BaseTiles 32×32 completos sob metadata controlada. Próximo:
   comparar a captura integrada no dono com o builder real/assets e
   preservar a seed global consumida pelo construtor default de parâmetros no
   builder real; a suspeita sobre a fixture foi confundida por entropia da
   thread de manutenção de Root;
   CacheSelector e cópia de Perlin vazio corrigidos; fechar
   contexto/inicialização e revalidar antes de sombra por setor
   com admissão/bytes. Não repetir o mesmo recorte de Root como nova entrega.
   Não adotar fita por tile, antecipar consultas, usar tile parcial ou mudar
   cópias/RNG. Auditar baseHeight/regionLiquids default, biomeTransition e SkyParameters::horizonClouds
   default/cópia antes de normalizar snapshots. Depois contexto/assets/demanda
   reais, mailbox/DAG, provider, promoção/fairness e pressão externa. A
   [cobertura de memória](../sector-runtime-memory-envelope.md) não é heap completo;
   não ampliar filas/defaults por teste controlado.
2. **VAL-02, próxima entrega e apoio à primeira:** ampliar o comparador para
   candidatos, IDs, prioridades, ordem e outputs completos. Manter os controles
   de colisões de hash como gate de qualquer eliminação de cópias.
3. **MOD-01:** completar a fronteira de efeitos/inputs puros do próximo kernel,
   incluindo inicialização/RNG/dependências. Seed automática corrigida e sequências
   explícitas verificadas; Perlin, biomeTransition/SkyParameters e a auditoria de
   consumo/reconstrução do contexto completo continuam abertos.
4. **VAL-01 + validação de DAG-01/DAG-04:** executar streaming com rota, save,
   config, assets/mods e estado inicial fixos; medir setores inéditos entregues/s,
   interesse→entrega e resposta do update/frame. Não pedir apenas outra run
   sem definir hipótese e comparador.
5. **H1, após os gates aplicáveis de H0:** DAG-02 em sombra, depois prioridade/
   cancelamento, contexto/produtos/executor CPU e drenagem produtiva, conforme
   a ordem abaixo. Não ampliar filas/defaults com contabilidade parcial.

RAM-01 não exige contabilizar cada heap do motor antes de qualquer protótipo
H1; exige identificar o escopo coberto, as exclusões e os limites antes da
ampliação correspondente. Esta fila não troca o roadmap por uma prioridade
concorrente.

## Catálogo ordenado das 49 tarefas

As sete notas de 25% usam as evidências indicadas após o catálogo. Demais 0%
significam **sem marco do novo contrato reconhecido nesta leitura do roadmap**,
não ausência de código relacionado no baseline. Não foi feita nova auditoria
de todas as implementações antigas para conceder crédito retroativo.

| Ordem | Entrada | ID / contrato | Tarefa | Progresso | Próxima entrega ou lacuna principal |
| ---: | --- | --- | --- | ---: | --- |
| 1 | H0 | [DAG-01](dependency-scheduler.md) | Identidades estáveis | 25% | IDs de colocações e ticket BaseTiles testados; contexto profundo/integração geral pendentes |
| 2 | H0 | [DAG-04](dependency-scheduler.md) | Conclusões e progresso dentro da passagem | 25% | Mailbox executiva/reação a conclusões; continuação/avanço já corrigidos no recorte |
| 3 | H0 | [RAM-01](memory-and-prefetch.md) | Reserva e contabilização de bytes | 25% | Json retido medido/deduplicado no contexto estático; polígonos/wrappers e admissão/contexto/consumidor globais pendentes |
| 4 | H0 | [VAL-02](validation-and-telemetry.md) | Comparador de outputs e produtores controlados | 25% | Root/layout/seletores simples/LRU reais e BaseTiles 32×32 comparados; noise/caches mutáveis, planeta/mods e streaming pendentes |
| 5 | H0 | [MOD-01](compatibility.md) | Classificar operações por efeito | 25% | Config lazy preserva ramos/falhas, custom terrain Root-free; adapter normal conserva Assets e restante do contexto é pendente |
| 6 | H0 | [VAL-01](validation-and-telemetry.md) | Benchmark de streaming | 25% | Rota/save/config fixos, streaming real e A/B equivalente |
| 7 | H1 | [DAG-02](dependency-scheduler.md) | Grafo persistente e demanda compartilhada | 0% | Grafo executivo em sombra, separado das dependências de publicação |
| 8 | H1 | [DAG-03](dependency-scheduler.md) | Prioridade, justiça e distância correta | 0% | Prioridade herdada e justiça entre consumidores com wrap |
| 9 | H1 | [DAG-05](dependency-scheduler.md) | Cancelamento, falha e invalidação | 0% | Retry/drain e retenção até término físico no novo executor |
| 10 | H1 | [CPU-01](cpu-and-stages.md) | Contexto privado de cálculo | 25% | Amostragem real simples e arrays BaseTiles completos comparados; Perlin/caches privados, ownership/versões, inicialização/RNG e admissão completa pendentes |
| 11 | H1 | [CPU-02](cpu-and-stages.md) | Produto PreparedSector | 0% | Produtos/versionamento compartilhados; continua em H2 |
| 12 | H1 | [MD-01](microdungeons-and-tmx.md) | Seleção leve e identidade | 0% | Candidatos/vazio sem copiar halo não lido |
| 13 | H1 | [CPU-03](cpu-and-stages.md) | Executor processual e granularidade | 0% | Capacidade global e tarefas com fronteira de publicação preservada |
| 14 | H1 | [WT-01](worldthread.md) | Serviço de conclusões e drenagem | 0% | Drenar trabalho elegível sem esperar rodadas artificiais |
| 15 | H1 | [WT-02](worldthread.md) | Orçamento produtivo e resposta do jogo | 0% | Usar o orçamento em progresso útil, com motivos de saída explícitos |
| 16 | H1 | [CPU-05](cpu-and-stages.md) | Governador de recursos e perfis | 0% | Primeira política por fila executável/custo; política completa em H5 |
| 17 | H2 | [RAM-02](memory-and-prefetch.md) | Camadas de cache e invalidação | 0% | Caches por produto/revisão, não substitutas de setor salvo |
| 18 | H2 | [RAM-03](memory-and-prefetch.md) | Política de retenção | 0% | Benefício, pressão, histerese e justiça entre mundos |
| 19 | H2 | [RAM-04](memory-and-prefetch.md) | Prefetch previsto e dependências | 0% | Antecipar rota e auxiliares com prioridade controlada |
| 20 | H2 | [RAM-05](memory-and-prefetch.md) | Cancelamento e reaproveitamento | 0% | Preservar produtos úteis a outros consumidores |
| 21 | H2 | [WT-03](worldthread.md) | Entrada síncrona em lote | 0% | Admissão consistente da região inteira |
| 22 | H2 | [IO-01](io-persistence-network.md) | Pipeline único de leitura e promoção | 0% | Promover tiles decodificados sem refazer leitura/decode |
| 23 | H3 | [CPU-04](cpu-and-stages.md) | Extração por etapa | 0% | Partes puras de CaveLiquid/Finalize, cada uma com seu contrato |
| 24 | H3 | [MD-02](microdungeons-and-tmx.md) | Formato intermediário estático TMX | 0% | Separar conteúdo estático da ligação autoritativa |
| 25 | H3 | [MD-03](microdungeons-and-tmx.md) | Ligação e RNG autoritativos | 0% | Preservar ordem/RNG/efeitos na aplicação |
| 26 | H3 | [MD-04](microdungeons-and-tmx.md) | Comandos, footprint e reserva | 0% | Mapa completo de efeitos/conflitos de estruturas |
| 27 | H3 | [MD-05](microdungeons-and-tmx.md) | Aplicação mais barata e retomada | 0% | Batches ou staging sem estado parcial observável |
| 28 | H3 | [WT-04](worldthread.md) | Publicação mais barata | 0% | Reduzir custo serial depois de validar os efeitos |
| 29 | H3 | [IO-02](io-persistence-network.md) | Serviço de banco e transações | 0% | Persistência consistente com os novos produtos |
| 30 | H3 | [IO-03](io-persistence-network.md) | Checkpoint e recuperação | 0% | Estado confirmado/recuperável; continua em R1 |
| 31 | H3 | [IO-04](io-persistence-network.md) | Unload, drain e retorno ao legado | 0% | Encerrar sem liberar buffers/produtores ainda ativos |
| 32 | H3 | [IO-05](io-persistence-network.md) | Pacotes e aplicação no cliente | 0% | Medir entrega até o cliente, não apenas commit |
| 33 | H4 | [GPU-01](gpu-compute.md) | Interface e capacidades CPU/GPU | 0% | Backend opcional com build CPU independente |
| 34 | H4 | [GPU-02](gpu-compute.md) | Contexto e buffers em voo | 0% | Thread de contexto, fences, lifetime e recuperação |
| 35 | H4 | [GPU-03](gpu-compute.md) | Kernels e equivalência | 0% | Primeiro kernel exato e lotes com ganho de custo completo |
| 36 | H4 | [GPU-04](gpu-compute.md) | Comparação em sombra | 0% | Corpus de precisão/bordas/drivers antes da publicação |
| 37 | H5 | [GPU-05](gpu-compute.md) | Seleção heterogênea e render | 0% | Escolher backend pelo custo de entrega, incluindo transporte |
| 38 | H5 | [VAL-03](validation-and-telemetry.md) | Métricas e marcos de entrega | 0% | Cobertura integral do novo pipeline e CPU/RAM/VRAM |
| 39 | H5 | [VAL-04](validation-and-telemetry.md) | Matriz de cenários | 0% | Escala, mundos, memória, GPU/fallback e multiplayer |
| 40 | H5 | [VAL-05](validation-and-telemetry.md) | Gates de adoção e relatório | 0% | Aplicar os gates ao novo pipeline completo; política de relatórios já existe |
| 41 | H5 | [MOD-02](compatibility.md) | Equivalência e versionamento | 0% | Comparar estado, comandos e identidades sem normalização indevida |
| 42 | H5 | [MOD-03](compatibility.md) | Perfis de execução e fallback | 0% | Adoção/migração com fronteiras seguras |
| 43 | H5 | [MOD-04](compatibility.md) | Subsistemas e plataformas | 0% | Matriz compatível de geração, scripts, save, rede e hardware |
| 44 | R1 | [REG-01](regional-threads.md) | Lifecycle e identidade da dona | 0% | Donos regionais autoritativos, não apenas preview observacional |
| 45 | R1 | [REG-02](regional-threads.md) | Estado local e visões de borda | 0% | Ownership local, snapshots de borda e passo comum |
| 46 | R1 | [WT-05](worldthread.md) | Coordenadora do runtime regional | 0% | WorldThread coordenando donos, não publicando tudo centralmente |
| 47 | R2 | [REG-03](regional-threads.md) | Tempo e operações entre regiões | 0% | Intenções multirregião e ordem autoritativa |
| 48 | R2 | [REG-04](regional-threads.md) | Transferência de entidades | 0% | Handoff sem dupla autoridade; não é externalização de NPCs |
| 49 | R2 | [REG-05](regional-threads.md) | Falha, checkpoint e custo de escala | 0% | Recuperação e ganho regional real; avaliar passos independentes só depois |

## Evidências das notas atuais

| Contrato | Evidência que sustenta 25% | Por que não recebe 50% ou mais |
| --- | --- | --- |
| DAG-01 | `d72b7239`, [identidades de colocações](../sector-runtime-h0-placement-identities-2026-11-08.md) e [ticket BaseTiles](../sector-runtime-base-tiles-contract.md), `f7785963` | Não cobre contexto profundo/assets nem todo o contrato de identidade/integração |
| DAG-04 | `2131ead3`, [passagem/continuação](../sector-runtime-h0-streaming-pass-2026-11-09.md) | Não implementa a mailbox executiva nem reação a conclusões na mesma passagem |
| RAM-01 | Recortes `5c3c4c8f` a `e076d96a`, [inventário](memory-and-prefetch.md), [envelope](../sector-runtime-memory-envelope.md) e [último relatório](../sector-runtime-h0-real-sector-sampling-2026-11-10.md) | Json v2 acompanha capturas rastreadas; polígonos/wrappers/layout, admissão completa, consumidor/demanda/provider/fairness e pressão externa pendentes |
| VAL-02 | [Harness inicial](../sector-runtime-h0-streaming-pass-2026-11-09.md), [comparador de tiles](../sector-runtime-h0-base-tiles-identity-2026-11-09.md), [kernel blockInfo](../sector-runtime-h0-block-info-kernel-2026-11-10.md) e [amostragem/BaseTiles reais](../sector-runtime-h0-real-sector-sampling-2026-11-10.md) | Noise/caches mutáveis, builder de planeta/assets/mods completos/streaming pendentes; controles de hash continuam gates |
| MOD-01 | [Leitores imutáveis](../sector-runtime-h0-distribution-bundles-2026-11-09.md), [proteção da seleção](../sector-runtime-h0-planning-storage-2026-11-09.md) e [lifecycle/scope 2](../sector-runtime-h0-lookup-source-generations-2026-11-09.md) | Metadata não é sincronização/commit nem auditoria de toda mutação; fronteira biomeTransition e integração abertas |
| VAL-01 | [Harness de passagens](../sector-runtime-h0-streaming-pass-2026-11-09.md), `2131ead3` | Primeiro instrumento auxiliar, não benchmark de streaming real/A-B |
| CPU-01 | `bfbbf403`, [captura custom terrain](../sector-runtime-h0-custom-terrain-context-2026-11-10.md), `e076d96a`, [config lazy](../sector-runtime-h0-block-info-static-inputs-2026-11-10.md), e `b0caadce`, [layout/cache reais e BaseTiles](../sector-runtime-h0-real-sector-sampling-2026-11-10.md) | Perlin/caches mutáveis, ownership/versões, inicialização/RNG, reutilização e admissão do contexto completo pendentes; composição ainda fora dos jobs |

CPU-01 recebe o primeiro marco porque agora existe cálculo com dados próprios
que independe da origem/Root após captura, não só callbacks/catálogos ou contagem
de testes. Não recebe 50%; não ativa jobs, orçamento ou nova fase de executor.

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
