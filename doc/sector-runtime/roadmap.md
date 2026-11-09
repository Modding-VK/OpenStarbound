# Roadmap, dependências e critérios de conclusão

[Índice](README.md) · [Baseline](baseline.md) · [Validação](validation-and-telemetry.md)

## Estado e regra de progresso

Este é o **plano principal de execução**, adotado em 08/11/2026. H0 é a frente
ativa; listas anteriores não estabelecem uma sequência concorrente.

Os itens abaixo estão **pendentes**, salvo quando o estado e um relatório
informarem implementação e validação. Os fundamentos atuais
estão no baseline; os previews existentes não equivalem ao executor proposto.
Este roadmap detalha H0–H5 do
[desenho de hardware](../sector-generation-hardware-design-2026-11-08.md).

Cada ID é definido no seu módulo. Não criar outro contrato para o mesmo ID em
um relatório. Registrar avanço com estado, commit, evidência e limitação.
Estados: pendente → implementação → revisão/testes → validado no escopo →
elegível para adoção. Um bloqueio explicita dependência e próximo passo.

## H0 — Baseline automatizado e invariantes

Propósito: reproduzir o problema rapidamente e impedir amplificação de uma
falha ao aumentar paralelismo.

- [ ] VAL-01: benchmark de streaming com rota/save/config fixos.
- [ ] VAL-02: comparador de outputs e controle de conclusão de produtores.
- [ ] DAG-01: IDs estáveis para colocações e cumprimento único de promises.
- [ ] DAG-04: teste e correção da interação continuação/avanço/encerramento.
- [ ] RAM-01: contrato de reserva/bytes reais antes de ampliar admissão.
- [ ] MOD-01: fronteira inicial de efeitos e inputs puros documentada no código.

### Andamento de H0

DAG-01: **implementado e validado no escopo de testes sem assets**, para a
identidade das colocações pendentes. Os caminhos síncrono e assíncrono usam o
mesmo registro; a aceitação completa continua aberta até validar geração com
assets/mods e integrar o comparador de outputs. Ver
[relatório e limites](../sector-runtime-h0-placement-identities-2026-11-08.md).
Não há nova API de cancelamento de colocações ou executor de grafo nesta entrega.

DAG-04, recorte de continuação/avanço: **implementado e validado sem assets**
em `2131ead3`. A continuação deixa de interromper um avanço visível permitido;
cada candidato externo é tentado no máximo uma vez por passagem com o preview
ativo, e a primeira continuação preserva seu follow-up. O caminho legado
continua sem a nova seleção. VAL-01/02 recebem um harness determinístico de
passagens com produtos sintéticos, relógio virtual e produtores controlados:
18 testes novos, incluindo 192 combinações; 26 testes de H0 aprovados no total.
Ver [relatório e limites](../sector-runtime-h0-streaming-pass-2026-11-09.md).
Isso não conclui VAL-01/02 nem a mailbox executiva de DAG-04: falta executar
o streaming real de `WorldStorage`, comparar outputs de mundo/mods e medir A/B.

RAM-01, recorte inicial em `5c3c4c8f`: reserva antes das cópias de BaseTiles/halo,
rollback de preparação e registro da pendência antes do dispatch implementados.
Os limites anteriores são mantidos; bytes de arrays não representam toda
a RAM do pipeline. A build CMake dos contratos passou 75 testes, além de sete
casos de hash selecionados e sete grupos CTest, com sanitizers estritos.
Uma sentinela nula inválida foi corrigida; o overflow do RNG automático foi
registrado para CPU-01, não corrigido. Ver
[relatório atual e limites](../sector-runtime-h0-memory-admission-2026-11-09.md).

RAM-01, segundo recorte em `65bae55c`: ledger processual observacional de arrays privados,
com reserva única por wrapper e retenção após fechar o mundo, integrado a
BaseTiles/halo. Inventário de produtos/caches e limites está em
[memória](memory-and-prefetch.md); o
[relatório atual](../sector-runtime-h0-process-memory-2026-11-09.md) delimita
validação e contadores. São 20 testes novos, 95 testes de contratos no total,
400 repetições adicionais aprovadas e oito grupos CTest, com sanitizers
estritos. O objeto real de geração também compilou. Isso ainda não impõe
teto global nem mede toda a RAM.

Próximo trabalho executável: RAM-01, medir/versionar tamanhos e storage único
de templates compartilhados, metadados e saídas variáveis; depois definir
limites processuais por hardware/configuração e reação à pressão.
Manter MOD-01 explícito antes de mudar admissão ou ownership.
Estender o harness para dependências reais e o baseline com rota/save/config
fixos. Não ampliar admissão nem ligar previews por padrão antes desses gates.

Saída: baseline executável, pendências de identidade cobertas e causas de
espera reproduzíveis. Não depende de uma nova amostra TMX para começar.

## H1 — Grafo executivo e preparação CPU

Propósito: transformar orçamento e núcleos livres em progresso de setores.
Depende de H0 para identidade e comparação.

- [ ] DAG-02: nós compartilhados, demanda por consumidor e grafo em sombra.
- [ ] DAG-03: prioridade herdada, justiça e distância com wrap.
- [ ] DAG-04/05: mailbox, revalidação, retry e cancelamento com drain.
- [ ] CPU-01: contexto privado com dependências/RNG de inicialização auditados.
- [ ] CPU-02 e MD-01: candidatos leves e resultado vazio versionado.
- [ ] CPU-03: capacidade processual e granularidade inicial.
- [ ] WT-01/02: drenagem produtiva, orçamento e motivos de saída explícitos.
- [ ] CPU-05: primeira política baseada em fila executável e custo observado.

Saída: mais tarefas independentes em voo, menor pronto→commit e equivalência
no streaming. Publicação mantém a ordem autoritativa aprovada.

## H2 — RAM residente e antecipação

Propósito: preparar a rota antes da demanda urgente e evitar recomputação.
RAM-01 começa em H0; as caches dependem das versões/produtos de H1.

- [ ] CPU-02: compartilhar terreno/campos/candidatos entre consumidores.
- [ ] RAM-02/03: caches versionadas, retenção por demanda e pressão/histerese.
- [ ] RAM-04/05: rota prevista, cancelamento e consumo compartilhado.
- [ ] WT-03: entrada síncrona usando admissão da região inteira.
- [ ] IO-01: promover tiles decodificados sem refazer leitura/decode.

Saída: ganho em antecipação e revisita, bytes únicos observáveis e ausência
de crescimento persistente após mudança de rota ou fechamento do mundo.

## H3 — Kernels adicionais, estruturas e publicação

Propósito: reduzir trabalho serial após o desbloqueio do pipeline.

- [ ] CPU-04: extrair parte pura de CaveLiquid e Finalize por contrato.
- [ ] MD-02/03: formato estático TMX separado da ligação que consome RNG.
- [ ] MD-04: mapa completo de efeitos, footprints e comandos de estruturas.
- [ ] MD-05 e WT-04: aplicação em batches e staging quando necessário.
- [ ] IO-02/03/04: banco, checkpoint e unload coerentes com novos produtos.
- [ ] IO-05: pacote confirmado e medição até aplicação no cliente.

Saída: custo serial menor por setor/estrutura, com save, scripts e rede
equivalentes. Cada kernel pode avançar separadamente; uma dungeon composta
complexa não bloqueia o kernel simples já validado de setor padrão.

## H4 — Backend GPU e comparação em sombra

Propósito: criar o caminho concreto de uso da GPU. O protótipo de interface e
capacidade pode avançar em paralelo a H1/H2; publicação exige contrato estável
do kernel CPU e do resultado versionado.

- [ ] GPU-01: interface CPU/GPU, capacidades e build CPU independente.
- [ ] GPU-02: thread de contexto, buffers, fences e recuperação.
- [ ] GPU-03: primeiro kernel exato e lotes apropriados.
- [ ] GPU-04: comparação em sombra e corpus de bordas/precisão/drivers.

Saída: equivalência e custo completo demonstrados, sem publicar divergências.
A existência de shaders/uso de GPU no gráfico não conclui esta entrega.

## H5 — Seleção heterogênea e escala

Propósito: escolher recursos pelo menor tempo de entrega e usar hardware com
folga de forma sustentada. Depende de H1/H2 e H4 para backends autoritativos.

- [ ] GPU-05: roteamento por custo de transporte/execução e prazo.
- [ ] CPU-05: política completa de CPU/RAM/VRAM, perfis e limites manuais.
- [ ] VAL-03/04/05: métricas de recursos e campanhas de escala equivalentes.
- [ ] MOD-02/03/04: adoção, fallback e migração de configuração documentados.

Saída: ganho de entrega ponta a ponta, resposta do jogo dentro dos alvos e
CPU completa em hardware sem compute. Nenhuma meta fixa de uso de hardware
substitui o resultado do benchmark.

## Evolução regional — R1 e R2

Esta frente mantém a decisão de uma thread dedicada por região 2×2 carregada.
Pode aproveitar produtos de H1–H3, mas depende de auditoria completa de
propriedade e efeitos; não é liberada por um kernel GPU aprovado.

- [ ] R1: REG-01/02, MOD-01 e IO-03 — lifecycle autoritativo, estado local,
  snapshots de borda e checkpoint, inicialmente com passo lógico comum.
- [ ] R1: WT-05 — WorldThread como coordenadora dos donos regionais.
- [ ] R2: REG-03/04/05 — intenções multirregião, handoff, falhas e custo real.
- [ ] R2 posterior: avaliar independência de passos após relógio/rede/save
  específicos; registrar decisão antes de remover barreira comum.

Saída: updates locais realmente sobrepostos, sem dupla autoridade e com
compatibilidade declarada por perfil. Não abrir NPC externo ou trocar a
unidade de thread para setor físico durante essa migração.

## Mapa de IDs e vínculos anteriores

| Família | Fonte do contrato | Relação histórica |
| --- | --- | --- |
| WT | [WorldThread](worldthread.md) | reestruturação do update e coordenação |
| DAG | [Grafo](dependency-scheduler.md) | WRG-03/04/05 e schedulers em preview |
| CPU | [Etapas](cpu-and-stages.md) | BaseTiles e planejamento privado existentes |
| RAM | [Memória](memory-and-prefetch.md) | WRG-04/06 e prefetch |
| GPU | [Compute](gpu-compute.md) | novo backend H4/H5 |
| IO | [Persistência/rede](io-persistence-network.md) | WRG-08 e auditoria de IO |
| MD | [Estruturas](microdungeons-and-tmx.md) | WRG-07 e plano de dungeons retomáveis |
| REG | [Regiões](regional-threads.md) | WRG-09/10/12, unidade 2×2 vigente |
| MOD | [Compatibilidade](compatibility.md) | requisito transversal, não fase opcional |
| VAL | [Validação](validation-and-telemetry.md) | evidência, benchmark e retomada |

WRG-11 e referências antigas a NPCs não retomam a externalização suspensa.
Essa correspondência permite localizar história; não declara um item WRG
inteiro concluído ao concluir apenas uma tarefa H.

## Procedimento para a próxima sessão

1. Ler este índice/roadmap e o último relatório associado ao item ativo.
2. Confirmar branch, commit e alterações locais; localizar os símbolos do
   módulo antes de editar.
3. Executar o primeiro item pendente cujas dependências estejam satisfeitas.
4. Rodar verificações apropriadas ao risco e registrar o escopo real.
5. Atualizar contrato se necessário, estado do item, relatório e checklist.
6. Informar commit e limitações; manter Actions manuais e revisão antes de main.

Retomar pelo andamento de H0 e pelo relatório vinculado acima. A prioridade
de entrega permanece setores comuns; uma coleta TMX inconclusiva não impede
avançar essa sequência.
