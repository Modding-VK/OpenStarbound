# Plano de ação para reestruturação da IA dos NPCs

Plano elaborado em 25/10/2026 na branch `perf/threading-audit`, com base na
[pesquisa de paralelismo de NPCs](npc-ai-parallelism-hypotheses.md) e no
histórico de decisões ali registrado. Integra o
[backlog de desempenho](performance-backlog.md).

**Estado: fase 0 em execução; fases 1–7 pendentes.** A
[instrumentação detalhada](npc-profiling.md) foi acrescentada; baseline,
latência de reação, inventário e aprovação da fase 0 ainda estão pendentes.
O primeiro marco executável compreende as fases 0–2: medição,
fronteira de leitura e comparação paralela sem efeitos. Este documento não
comprova ganho de desempenho nem significa que os NPCs já usam workers.

## 1. Objetivo e decisões iniciais

Reduzir o trabalho de NPCs na thread do mundo e melhorar sua resposta durante
ticks lentos, preservando jogabilidade, mods, persistência e multiplayer.
O custo observado de ITDs do Frackin' Universe não demonstra custo equivalente
de NPCs; a decisão de migrar cada comportamento depende de medição própria.

Decisões para a implementação futura:

- Recursos experimentais desativados por padrão e habilitados por etapas.
- Scripts existentes mantêm o caminho atual. Participação na API assíncrona
  será explícita; não migrar automaticamente scripts pelo nome do mod.
- O servidor continua autoritativo para os NPCs comuns. Decisões e posições
  provisórias não confirmam dano, inventário ou alterações do mundo.
- O primeiro marco não altera protocolo de rede, formato de saves ou HUD.
- Manter Lua; esta proposta não exige migração para Pluto.
- Reservar recursos e filas limitadas para NPCs, sem colocar diretamente suas
  tarefas no pool de geração de setores. Medir também a competição por CPU
  com esse pool e com o cliente na mesma máquina.
- Transferir autoridade de portas, baús ou outros objetos é outra iniciativa.
  O coordenador aqui proposto registra intenções e respostas; não passa a
  executar arbitrariamente scripts de objetos ou transações de inventário.
- Manter a pesquisa como histórico de hipóteses. Este plano acompanha tarefas,
  dependências, evidências e critérios para aprovação de cada fase.

### Limite da hipótese de resposta independente

Uma thread mais frequente pode calcular usando a última visão publicada,
mas não torna essa visão mais recente. Quando o mundo atrasa, terreno e
interações confirmadas também podem atrasar. Caminhos próprios de entrada e
apresentação são uma fase posterior, com estados provisórios identificados.
Acelerar cálculo, atualizar conhecimento e confirmar efeitos são objetivos
distintos e precisam de métricas separadas.

## 2. Arquitetura e contratos

Separar publicação de dados estáveis, cálculo, coordenação de intenções e
aplicação autoritativa. O primeiro protótipo cobre somente publicação e cálculo.

| Componente proposto | Responsabilidade | Restrição |
| --- | --- | --- |
| Fonte de leitura do pathfinding | Fornecer geometria, colisão, gravidade, líquidos e obstáculos relevantes por um contrato comum | Adaptador serial para o mundo e adaptador de snapshot; worker sem `World*` ou referência mutável a entidades |
| Publicador de snapshots | Publicar regiões imutáveis com identidade do mundo, revisões e instante de captura | Captura consistente com o dono do estado; retenção e região limitadas; publicação atômica de uma visão completa |
| Executor de tarefas NPC | Calcular rotas e decisões isoladas | Admissão limitada, cancelamento lógico e identidade da encarnação do NPC |
| Coordenador de interações | Associar alvo, intenção, solicitantes e resultado | Agrupar apenas operações equivalentes; mutex mantido brevemente; não executar callbacks sob o lock |
| Aplicador autoritativo | Validar e aplicar propostas na ordem definida pelo servidor | Conferir existência, revisões, permissões e efeitos; deduplicar aplicação e rejeitar proposta obsoleta |
| Entrada e apresentação independentes | Entregar entradas recentes e publicar movimento provisório em fase posterior | Negociação de capacidade, reconciliação e distinção explícita entre provisório e confirmado |

```mermaid
flowchart LR
    W[Estado autoritativo do mundo] --> P[Publicação consistente]
    P --> S[Snapshots imutáveis e versionados]
    S --> N[Cálculo dos NPCs]
    N --> R[Resultados ou intenções limitados]
    R --> C[Coordenação de interações]
    R --> V[Validação autoritativa]
    C --> V
    V --> W
    V --> A[Confirmação ou rejeição]
    A --> N
```

No modo de comparação, os resultados seguem apenas para análise e descarte:
não entram no coordenador nem na aplicação autoritativa.

### Invariantes obrigatórias

1. Identificar tarefas por mundo/época, encarnação do NPC e sequência. Um ID
   de entidade reutilizado não pode receber o resultado de um NPC removido.
2. Registrar as revisões dos dados realmente lidos. Alterações de tiles em
   memória, obstáculos dinâmicos e mudanças de proteção devem invalidar o que
   depender delas; não basta verificar se o setor ainda existe.
3. Ausência de região significa dado indisponível, nunca terreno vazio ou
   transitável. Solicitar cobertura adicional com limites ou recorrer ao serial.
4. Nenhuma espera por worker ou resposta de objeto pode manter o mutex do
   mundo. O coordenador não mantém locks ao chamar scripts ou outros serviços.
5. Limitar tarefas, bytes retidos, região, tempo de captura, resultados e
   tentativas. Coalescer pedidos substituídos por NPC, rejeitar pedidos antigos
   e oferecer progresso previsível sob sobrecarga, sem espera circular.
6. Cancelamento lógico invalida a aplicação, mesmo que o cálculo ainda termine.
   Descarregar um mundo deve invalidar sua época e liberar os recursos com
   segurança; workers nunca conservam ponteiros para o mundo destruído.
7. Deduplicar efeitos por identidade de operação durante sua vida útil. Persistir
   apenas estado confirmado; não salvar tarefas em voo como efeitos concluídos.
   Reinício invalida operações antigas e não as reaplica automaticamente.
8. Frequência de cálculo não é tempo de simulação: mais ciclos não podem
   aumentar velocidade, ataques ou regeneração. Definir relógio, passo de
   simulação e limite de recuperação de atraso antes da fase 5.
9. Nunca permitir que mundo e serviço NPC alterem simultaneamente o mesmo
   estado do NPC. A transferência de propriedade exige protocolo explícito.
10. Preservar ordem causal de mensagens, interações e dano. Ordem de conclusão
    dos workers não define a ordem autoritativa. RNG e desempates devem ter
    estado e regras explícitos, sem acesso concorrente ao gerador do mundo.
11. Callbacks Lua e bibliotecas acessíveis pelo protótipo devem ser auditados;
    uma cópia superficial de estado ou um `const` não garante segurança.

### Contrato inicial de tarefa e resposta

| Registro | Dados mínimos a definir na implementação |
| --- | --- |
| Pedido | Mundo/época, NPC/encarnação, sequência, origem/destino, parâmetros de movimento, snapshot, prioridade, instante de envio e prazo |
| Snapshot | Identidade, captura, cobertura, revisões, dados imutáveis e tamanho retido |
| Resultado | Identidade do pedido, revisões consultadas, rota/proposta, custo, estado de conclusão e motivo de falha/descarte |
| Intenção de interação | Identidade da operação e do alvo, precondições, ação solicitada, solicitantes e prazo |
| Resposta de interação | Confirmada, rejeitada, cancelada ou expirada; versão confirmada e motivo |

Em uma porta, pedidos equivalentes de **abrir** podem compartilhar uma operação.
Pedidos de abrir e fechar não são equivalentes. O NPC só considera a passagem
confirmada após observar o estado autoritativo; aceitar a intenção não significa
que a porta abriu. Jogadores e scripts externos também podem mudar esse estado.
O mutex protege os registros de coordenação, não substitui essa validação.

## 3. Fases, dependências e critérios de avanço

As fases começam pendentes; a fase 0 já recebeu instrumentação inicial.
A fase seguinte depende da aprovação da
anterior; descobrir uma limitação pode exigir retornar à fronteira de leitura.
Não iniciar migração ampla enquanto o primeiro marco não produzir evidências.

| Fase | Entrega | Dependência | Estado |
| --- | --- | --- | --- |
| 0 | Baseline e inventário de acessos | Nenhuma | Em execução; coleta pendente |
| 1 | Contrato de leitura e snapshots | 0 aprovada | Pendente |
| 2 | Comparação paralela sem efeitos | 1 aprovada | Pendente |
| 3 | Pathfinding ativo opcional | 2 aprovada | Pendente |
| 4 | Interações coordenadas limitadas | 3 aprovada | Pendente |
| 5 | Simulação própria para grupo controlado | 4 aprovada | Pendente |
| 6 | Entrada e apresentação independentes | 5 aprovada | Pendente |
| 7 | Ampliação de IA e API para mods | 6 aprovada | Pendente |

### Fase 0 — Medição e inventário

- [x] Acrescentar escopos de etapas NPC, callbacks, pathfinding, interações e
      percentis limitados em memória; ver [cobertura e limites](npc-profiling.md).
- [ ] Separar custo de Lua periódico, outros callbacks, pathfinding, movimento,
      status, logging e interações, evitando somar tempos inclusivos duplicados.
- [ ] Mapear leituras, escritas, chamadas síncronas, RNG e produtores de mudanças
      no estado do NPC e nos dados que ele consulta.
- [ ] Preparar cenários reproduzíveis com NPCs vanilla e Frackin' Universe,
      registrando quantidade e comportamentos, sem confundir NPCs com ITDs.
- [ ] Fixar commit, build, assets/mods, configuração, hardware e cópia de universo.
- [ ] Definir limites concretos de fila, memória, cobertura regional e captura,
      além de tolerâncias de regressão, antes de comparar protótipos.

**Aprovação:** baseline reproduzível com custo próprio dos NPCs e cobertura dos
acessos a estado compartilhado. Se o custo não justificar a migração, registrar
a decisão de suspender o caminho ativo. Não aumentar limites sem nova medição.

**Registro:** instrumentação: commit desta entrega (histórico de `npc-profiling.md`);
build Windows e cenário/artefatos: pendentes; métricas reais e limites: pendentes;
revisão/decisão: pendente; questões abertas: inventário e marcadores de reação.

### Fase 1 — Fronteira de leitura

- [ ] Introduzir interface de consultas e adaptador serial preservando resultados.
- [ ] Cobrir as leituras reais de `PlatformerAStar::PathFinder`, incluindo
      geometria, colisões dinâmicas, plataformas, líquidos e gravidade.
- [ ] Representar wrap horizontal, regiões ausentes e obstáculos sem referências
      mutáveis. Definir consistência de captura e publicação por região.
- [ ] Incluir revisões de alterações locais e ciclo de vida dos alvos;
      delimitar expansão de busca e custo máximo de captura.
- [ ] Documentar propriedade de memória e liberação de snapshots ainda em uso.

**Aprovação:** equivalência do adaptador serial e das consultas sobre estados
congelados conhecidos; nenhuma referência ao mundo mutável retida pelo worker.
Uma rota que alcance região ausente termina com motivo explícito.

**Registro:** commit/build: pendente; cenário/artefatos: pendentes; equivalência e
custo de captura: pendentes; revisão/decisão: pendente; questões abertas: cobertura.

### Fase 2 — Comparação paralela sem efeitos

- [ ] Criar uma thread experimental com admissão limitada e cancelamento lógico.
- [ ] Executar referência serial e cálculo paralelo sobre o **mesmo snapshot**,
      parâmetros e estado de RNG/desempate; manter a simulação normal autoritativa.
- [ ] Comparar rotas, estados finais da busca, decisões e razões de falha.
      Diferenças contra o mundo vivo são registradas separadamente como frescor.
- [ ] Medir captura, espera, execução, divergências, descartes e memória retida.
- [ ] Exercitar saturação, cancelamento, morte/remoção do NPC e descarga do mundo.
- [ ] Garantir por contrato que resultados experimentais nunca sejam instalados.

**Aprovação:** nenhuma divergência determinística sem explicação, recursos
limitados e encerramento seguro. Esta fase duplica cálculo e mede custo de
diagnóstico; não pode demonstrar ganho da otimização ativa.

**Registro:** commit/build: pendente; cenário/artefatos: pendentes; divergências e
overhead: pendentes; revisão/decisão: pendente; questões abertas: prova de isolamento.

### Fase 3 — Pathfinding ativo opcional

- [ ] Expor submissão, consulta de resultado e cancelamento para participantes
      explicitamente habilitados; consumidores legados continuam seriais.
- [ ] Validar NPC, encarnação, alvo, sequência e revisões antes de instalar rota.
- [ ] Manter Lua, movimento, status e dano no caminho autoritativo existente.
- [ ] Definir limites para retries, prazo e fallback serial sem repetir efeitos
      nem executar várias buscas substituídas do mesmo NPC no tick.
- [ ] Comparar custo total e reação visível, incluindo captura e invalidações.

**Aprovação:** melhoria líquida maior que a dispersão do baseline, sem regressão
consistente de rota, reação, frame do cliente ou carregamento de setores visíveis.

**Registro:** commit/build: pendente; cenário/artefatos: pendentes; ganho líquido e
fallbacks: pendentes; revisão/decisão: pendente; questões abertas: API e admissão.

### Fase 4 — Interações coordenadas

- [ ] Começar por abertura de porta controlada; não migrar inventários ou ITDs.
- [ ] Implementar pendência, solicitantes, resposta causal, timeout e cancelamento.
- [ ] Coalescer aberturas equivalentes respeitando alvo, versão e precondições;
      não coalescer operações não idempotentes ou solicitações opostas.
- [ ] Observar alterações por jogadores e scripts na validação autoritativa.
- [ ] Garantir que um NPC possa desistir sem cancelar o interesse de outro.

**Aprovação:** dois NPCs recebem resposta coerente sem duplicar abertura;
fechar, destruir, proteger ou descarregar a porta invalida a passagem dependente.
Rejeições de scripts não deixam NPCs esperando indefinidamente.

**Registro:** commit/build: pendente; cenário/artefatos: pendentes; conflitos e
timeouts: pendentes; revisão/decisão: pendente; questões abertas: equivalência de ações.

### Fase 5 — Simulação própria para grupo controlado

- [ ] Selecionar poucos NPCs com comportamento explícito e dependências auditadas.
- [ ] Definir transferência exclusiva de estado, ordem entre NPCs e tempo de
      simulação; impedir atualização duplicada pelo mundo e pelo serviço.
- [ ] Calcular movimento provisório e reconciliar mudanças externas, inclusive
      colisões entre entidades, empurrões, dano, morte e teleporte.
- [ ] Manter efeitos confirmados no servidor e tratar regiões obsoletas/ausentes
      conservadoramente; não atravessar uma porta pela mera intenção de abertura.
- [ ] Medir divergência de posição, frequência e tamanho das correções.

**Aprovação:** ausência de atualização dupla, aceleração artificial de tempo e
dano prematuro; correções dentro dos limites definidos na fase 0 ou revisados
explicitamente antes da avaliação. Demonstrar recuperação para o caminho serial.

**Registro:** commit/build: pendente; cenário/artefatos: pendentes; correções e
propriedade de estado: pendentes; revisão/decisão: pendente; questões abertas: física.

### Fase 6 — Entrada e apresentação independentes

- [ ] Mapear entrada de jogadores e entrega de posições que hoje dependem do tick
      do mundo; criar caminho próprio apenas onde medição justificar.
- [ ] Diferenciar entrada validada, posição provisória e estado confirmado,
      com sequências, prazos, limites de tráfego e reconciliação.
- [ ] Negociar capacidade entre host e clientes e manter fallback legado.
- [ ] Medir idade dos dados, latência percebida e custo de rede sob atrasos
      controlados do mundo, perda/reordenação e desconexão.

**Aprovação:** melhoria percebida demonstrável durante ticks lentos, sem conceder
autoridade de combate a estados provisórios. Compatibilidade de protocolo é
decisão explícita desta fase; não presumir transparência para clientes antigos.

**Registro:** commit/build: pendente; cenário/artefatos: pendentes; latência e
tráfego: pendentes; revisão/decisão: pendente; questões abertas: negociação de rede.

### Fase 7 — Ampliação de IA e API para mods

- [ ] Migrar um comportamento por vez, com custo e dependências documentados.
- [ ] Definir contextos Lua isolados, bindings permitidos, RNG, mensagens e
      contratos de resultado, cancelamento, falha e limites.
- [ ] Preservar execução serial para comportamentos incompatíveis; chamadas
      síncronas não podem ser transformadas silenciosamente em assíncronas.
- [ ] Publicar documentação e exemplos originais para adesão explícita dos mods.
- [ ] Relacionar a API com o [plano de objetos](object-worker-architecture-plan.md)
      sem presumir que NPCs e transações de storages tenham a mesma semântica.

**Aprovação:** cada comportamento migrado tem evidência de compatibilidade,
benefício e recuperação. Nenhuma promessa de paralelizar automaticamente mods
existentes ou de executar todo script com segurança fora do mundo.

**Registro:** commit/build: pendente; cenário/artefatos: pendentes; cobertura de
mods: pendente; revisão/decisão: pendente; questões abertas: isolamento Lua.

## 4. Validação e aceitação

As verificações abaixo são trabalho futuro das fases correspondentes. Não foram
executadas pela criação deste documento. Incluir testes de unidade para consultas,
revisões, filas e coordenador; integração para publicação/aplicação; e cenários
de jogo para comportamento, desempenho e recuperação.

| Grupo | Casos obrigatórios | Resultado esperado |
| --- | --- | --- |
| Locomoção | Caminhar, saltar, cair, plataformas, líquidos, gravidade variável e wrap horizontal | Consulta e movimento coerentes com a referência; sem atravessar obstáculos |
| Frescor | Tile alterado após captura, setor descarregado, alvo movido, resultados fora de ordem | Rejeição/replanejamento limitado; não instalar rota obsoleta |
| Ciclo de vida | NPC morto, removido, teleportado, recriado e ID reutilizado | Nenhuma resposta entregue à encarnação errada |
| Disputa | NPCs A/B na mesma porta, jogador abrindo/fechando, proteção, destruição e script rejeitando | Respostas causais, sem duplicação, deadlock ou espera sem prazo |
| Combate | NPC contra jogador/NPC, projéteis, dano e efeitos sobre vários tiles | Autoridade e ordem preservadas; posição provisória não confirma dano |
| Persistência | Salvar, recarregar e encerrar com tarefas pendentes; desativar o recurso | Estado confirmado íntegro; sem reaplicação de intenções antigas |
| Multiplayer | Single-player, servidor dedicado, latência, desconexão e cliente legado | Reconciliação e fallback definidos; sem divergência autoritativa |
| Sobrecarga | Muitos NPCs, fila cheia, região grande, CPU limitada, setores sendo gerados | Limites respeitados, progresso e consumo de memória previsíveis |
| Atraso controlado | Pausas instrumentadas no mundo em ambiente de teste | Separar latência de cálculo, idade do dado e espera pela confirmação |

### Protocolo de medição

1. Fazer pelo menos cinco repetições pareadas por cenário, restaurando a mesma
   cópia de universo e mantendo mods, configuração, população e rota equivalentes.
   Registrar aquecimento e alternar ordem das versões para reduzir viés de cache.
2. Separar três condições: serial com diagnóstico desligado; comparação sem
   efeitos; modo ativo opcional. Usar medições externas/comuns para comparação
   justa e registrar o custo da instrumentação detalhada separadamente.
3. Registrar p50/p95/p99 do tick, latência de reação, idade do snapshot, espera
   em fila, captura/publicação, execução, validação, descartes, retries, correções,
   memória retida, tráfego e frame do cliente. Informar amostras e duração;
   percentis de poucas amostras não sustentam conclusões sobre caudas.
4. Avaliar hardware com diferentes quantidades de núcleos e memória. Não assumir
   que uma melhoria no computador original justifica ativação para todos.
5. Promover modo ativo somente se o ganho superar a variabilidade observada e
   não houver regressão consistente de mundo, cliente ou setores visíveis.
   Registrar tolerâncias antes da comparação; não remover limites para passar.

Qualquer efeito duplicado, perda de estado, corrupção de save, corrida de dados
ou divergência de autoridade bloqueia avanço. Inspeção de locks e ferramentas
de detecção de concorrência complementam os testes; ausência de crash não
comprova segurança. Aplicar ferramentas compatíveis com cada build e registrar
as limitações de cobertura.

## 5. Entrega, acompanhamento e recuperação

### Entrega documental atual

- [x] Registrar arquitetura, invariantes e dependências neste plano.
- [x] Definir primeiro marco sem aplicação de efeitos (fases 0–2).
- [x] Vincular pesquisa de NPCs e backlog geral a este documento.
- [x] Iniciar fase 0 com a instrumentação detalhada.
- [ ] Preencher o registro da fase 0 com coleta e evidência de comportamento.

### Processo para futuras implementações

- Entregar commits pequenos e revisáveis na branch de trabalho, com flags
  desativadas por padrão e distinção entre build normal e diagnóstica.
- Atualizar checklist e registro da fase a cada entrega. Usar commit exato,
  configuração, cenário, artefatos, métricas, limitações e decisão de revisão.
  Código entregue, compilação aprovada e comportamento validado são estados
  diferentes; um não substitui o outro.
- Exigir revisão independente para concorrência, persistência e rede antes
  de integrar em `main`, conforme as instruções do projeto.
- Iniciar a build Windows somente quando solicitada pelo usuário. Os gatilhos
  de push e pull request foram removidos; não fazer dispatch após cada alteração.
  Essa decisão substitui a preferência anterior de início automático.
  No último estado verificado, Actions
  estava desabilitado para este repositório; enquanto isso persistir, registrar
  o bloqueio. Compilação parcial Linux não valida o executável Windows.
- Usar somente dependências oficiais, fixadas e verificadas conforme
  [dependency-sources.md](dependency-sources.md). Não copiar implementações
  externas; registrar referências e escrever a implementação do projeto.
- Revisar traces e artefatos antes de compartilhar; não incluir dados pessoais
  ou caminhos privados desnecessários. Builds não autorizam publicação de release.

### Desativação e recuperação

Definir a transição de retorno antes de ativar cada fase. Desativar deve parar
admissão, invalidar a época de tarefas, impedir commits tardios e liberar
resultados pendentes sem repetir efeitos. O NPC retoma o caminho serial a partir
do último estado confirmado. Na simulação própria, transferir propriedade de
estado em uma fronteira segura; nunca ligar dois atualizadores ao mesmo tempo.

Uma confirmação já aplicada deve permanecer aplicada, mesmo que sua resposta
tenha se perdido. Consultar o registro autoritativo antes de repetir a intenção.
Não depender de rollback genérico de scripts, inventários ou dano para resolver
conflitos. Se a recuperação segura não estiver demonstrada, manter a fase
desativada e limitar o trabalho à comparação sem efeitos.

### Registro de decisão por fase

Expandir o registro compacto da fase com estes campos ao iniciar sua execução:

```text
Fase / estado (pendente, em execução, entregue, validada, bloqueada):
Commit e opções de build:
Cenário, hardware, assets/mods e configuração:
Pré-condições e limites definidos antes da medição:
Artefatos e procedimento para reprodução:
Resultados pareados, dispersão e custo da instrumentação:
Falhas, limitações e questões abertas:
Recuperação/fallback exercitado:
Revisor, data e decisão (avançar, ajustar, suspender):
```

O plano documental está concluído quando os vínculos estão corretos e as
etapas distinguem proposta, entrega e validação. O rework só estará concluído
quando as fases aprovadas tiverem evidências preenchidas; fases suspensas ou
descartadas devem registrar a razão, sem serem marcadas como implementadas.
