# Progressão do projeto — percentuais e ordem das tarefas

Atualizado em **09/11/2026**. Branch: `Experimental`.
Estado inspecionado: `bcbe56c68b70455b3e60dde030963b75c7fa2766`;
última implementação: `cad311d1`.

[Programa principal](README.md) · [Roadmap e dependências](roadmap.md)
· [Último relatório técnico](../sector-runtime-h0-planning-storage-2026-11-09.md)
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
| Progresso de maturidade do programa | **3,1%** — 6 marcos verificados de 196 |
| Contratos com avanço registrado | **6 de 49**; todos ainda parciais |
| Contratos integralmente aceitos | **0 de 49 (0%)** no novo programa |
| Última evidência de testes | 204 testes, 380 execuções adicionais e 14 grupos CTest, ASan/UBSan |
| Ganho de setores/s da nova arquitetura | Ainda não demonstrado por benchmark integrado equivalente |

**Os 3,1% não significam que o jogo esteja só 3,1% pronto.** O runtime anterior
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
- **Programa:** soma das notas dos 49 contratos ÷ 49. Hoje: 150 ÷ 49 = 3,1%
  arredondado; equivalentemente, 6 ÷ (49 × 4) × 100.
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
| 2 | H1 — grafo executivo, CPU e entrega | 10 | 0 / 40 | 0% | H0 aplicável; grafo começa em sombra, publicação só após equivalência |
| 3 | H2 — produtos residentes e prefetch | 6 | 0 / 24 | 0% | Identidade/versionamento de H1 e orçamento de RAM |
| 4 | H3 — kernels adicionais, estruturas e publicação | 10 | 0 / 40 | 0% | Contrato por kernel e publicação validável; TMX não bloqueia setor comum |
| 5 | H4 — backend GPU e comparação em sombra | 4 | 0 / 16 | 0% | Protótipo pode acompanhar H1/H2; publicação depende do kernel CPU equivalente |
| 6 | H5 — seleção de hardware e escala | 7 | 0 / 28 | 0% | Produtos/backends validados e custo completo de entrega medido |
| 7 | R1 — donos regionais e coordenadora | 3 | 0 / 12 | 0% | Auditoria de autoridade/efeitos e checkpoint; não basta aprovar GPU |
| 8 | R2 — operações e transferências entre regiões | 3 | 0 / 12 | 0% | Lifecycle/estado de R1, recovery e matriz de multiplayer |
| — | **Total** | **49** | **6 / 196** | **3,1%** | Não equivale a ganho de desempenho ou autorização de release |

Essa é uma ordem de prioridade/dependência, não uma proibição de frentes
paralelas autorizadas pelo roadmap. Setores comuns permanecem prioritários.
O alvo regional é uma thread dedicada por região lógica 2×2 de setores 32×32,
distinta dos workers de preparação; ainda não está implementado como simulação
autoritativa. GPU é opcional, com fallback CPU completo.

## Próximas ações — fila imediata

1. **RAM-01, em execução:** cobrir payloads de candidatos/temporários e variants
   opacos, preservando o histórico de cópias; explicitar lacunas de layout/
   biomas/seletores e o envelope de memória antes de ampliar admissão.
2. **VAL-02, próxima entrega e apoio à primeira:** ampliar o comparador para
   candidatos, IDs, prioridades, ordem e outputs completos. Manter os controles
   de colisões de hash como gate de qualquer eliminação de cópias.
3. **MOD-01:** completar a fronteira de efeitos/inputs puros do próximo kernel,
   incluindo inicialização/RNG/dependências. Achados CPU-01 de seed automática
   e Perlin não inicializado continuam abertos.
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

As seis notas de 25% usam as evidências indicadas após o catálogo. Demais 0%
significam **sem marco do novo contrato reconhecido nesta leitura do roadmap**,
não ausência de código relacionado no baseline. Não foi feita nova auditoria
de todas as implementações antigas para conceder crédito retroativo.

| Ordem | Entrada | ID / contrato | Tarefa | Progresso | Próxima entrega ou lacuna principal |
| ---: | --- | --- | --- | ---: | --- |
| 1 | H0 | [DAG-01](dependency-scheduler.md) | Identidades estáveis | 25% | Completar identidade/versionamento e comparação integrada; IDs de colocações já testados |
| 2 | H0 | [DAG-04](dependency-scheduler.md) | Conclusões e progresso dentro da passagem | 25% | Mailbox executiva/reação a conclusões; continuação/avanço já corrigidos no recorte |
| 3 | H0 | [RAM-01](memory-and-prefetch.md) | Reserva e contabilização de bytes | 25% | Payloads/temporários opacos, envelope processual e reação à pressão |
| 4 | H0 | [VAL-02](validation-and-telemetry.md) | Comparador de outputs e produtores controlados | 25% | Fluxo completo de candidatos/outputs e dependências reais |
| 5 | H0 | [MOD-01](compatibility.md) | Classificar operações por efeito | 25% | Auditoria completa do escopo que ganhará concorrência |
| 6 | H0 | [VAL-01](validation-and-telemetry.md) | Benchmark de streaming | 25% | Rota/save/config fixos, streaming real e A/B equivalente |
| 7 | H1 | [DAG-02](dependency-scheduler.md) | Grafo persistente e demanda compartilhada | 0% | Grafo executivo em sombra, separado das dependências de publicação |
| 8 | H1 | [DAG-03](dependency-scheduler.md) | Prioridade, justiça e distância correta | 0% | Prioridade herdada e justiça entre consumidores com wrap |
| 9 | H1 | [DAG-05](dependency-scheduler.md) | Cancelamento, falha e invalidação | 0% | Retry/drain e retenção até término físico no novo executor |
| 10 | H1 | [CPU-01](cpu-and-stages.md) | Contexto privado de cálculo | 0% | Auditar inicialização/dependências/RNG do contexto completo |
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
| DAG-01 | `d72b7239`, [identidades de colocações](../sector-runtime-h0-placement-identities-2026-11-08.md) | Recorte de colocações, não todo o contrato de identidade/integração |
| DAG-04 | `2131ead3`, [passagem/continuação](../sector-runtime-h0-streaming-pass-2026-11-09.md) | Não implementa a mailbox executiva nem reação a conclusões na mesma passagem |
| RAM-01 | Recortes `5c3c4c8f` a `cad311d1`, [inventário](memory-and-prefetch.md) e [último relatório](../sector-runtime-h0-planning-storage-2026-11-09.md) | Heaps parciais, estimativas não hard e ausência de teto processual completo |
| VAL-02 | [Harness inicial](../sector-runtime-h0-streaming-pass-2026-11-09.md) e [sampler/controles de hash](../sector-runtime-h0-planning-storage-2026-11-09.md) | Comparação ainda não cobre o streaming/mundo completo |
| MOD-01 | [Leitores imutáveis](../sector-runtime-h0-distribution-bundles-2026-11-09.md) e [proteção da seleção](../sector-runtime-h0-planning-storage-2026-11-09.md) | Auditoria restrita, não de todos os efeitos/inicializadores |
| VAL-01 | [Harness de passagens](../sector-runtime-h0-streaming-pass-2026-11-09.md), `2131ead3` | Primeiro instrumento auxiliar, não benchmark de streaming real/A-B |

Os 204 testes e demais números do resumo são **evidência herdada do último
relatório técnico**, não testes reexecutados ao criar este painel. Nenhuma
captura/config/log novo foi analisado. A criação do painel é documental;
não muda runtime, prioridades do roadmap, defaults, workflows ou adoção.

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

Compatibilidade com mods/saves/multiplayer e fallback CPU são gates essenciais.
Externalização de NPCs permanece suspensa; Linux Clang continua adiado.
Oceanos/clima e demais itens históricos permanecem no backlog geral e não
entram silenciosamente no denominador dos 49 contratos deste programa.

Conferência desta criação: 49 IDs únicos correspondentes aos contratos dos
módulos/roadmap, ordem 1–49 sem lacunas, oito totais de fase e fórmulas
recalculados; 355 links locais em 27 documentos sem destino ausente e todos
os 14 documentos da coleção alcançáveis pelo índice. `git diff --check`
aprovado. Nenhuma mudança em `source/` ou workflows; sem build, reexecução de
testes de runtime ou disparo de Actions nesta entrega documental.
