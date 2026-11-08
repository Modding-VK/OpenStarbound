# Progresso da geração e contratos para threads por setor

Estado em 03/11/2026: identidade de pedido, comparação observacional e a
primeira política experimental de justiça implementadas. A política permanece
desligada por padrão e não altera o modo legado.
Branch Experimental. Externalização de NPCs
suspensa conforme o backlog. Este documento complementa a
[auditoria dos 41 logs](log-audit-2026-11-03-deep.md) e a
[auditoria de dungeons](dungeon-access-audit.md).

## Evidência Tracy de 05/11/2026

A captura `Tracy3` da build `5c4047d9` confirmou que o atraso de setores não
decorre de falha na infraestrutura de regiões: `spawnFailed`, `runtimeFailed`
e `dispatchRejected` ficaram em zero, enquanto a espera p95 das mailboxes ficou
entre 10 e 13 us. Foram observados pedidos com 10 a 16,4 s de idade marcados
principalmente como `notSelected`, e alguns bloqueados em `generation.baseTiles`.

O custo da fila de geração foi p95 19,60 ms, p99 54,99 ms e máximo 154,96 ms.
Microdungeons tiveram p95 16,65 ms, p99 59,38 ms e máximo 154,61 ms. Esses
valores justificam habilitar o recorte de prioridade persistente no próximo
teste: ele preserva uma promoção que perdeu sua tentativa por deadline, sem
alterar as dependências, a ordem de publicação ou o orçamento existente.

O mesmo Tracy identificou um pico separado de 638,8 ms durante uma transição de
mundo, composto por 101 atualizações de NPCs. Esse resultado pertence ao
backlog de NPCs e não muda a prioridade atual de threading por setores.

## Casos reconstruídos

| Evidência | Estado observado | Conclusão permitida |
|---|---|---|
| Parte 0017, linhas 870 e 872, 18:27:47.794–18:27:48.287 | `[408,134]`, nível 3, idade 19,5–20 s, zero tentativas, `notSelected` | O setor antigo não foi escolhido no intervalo acompanhado. |
| Parte 0017, linha 881, 18:27:58.310 | Mesmo setor, 30 s, 40 tentativas, zero completas, zero avanços | Visitas à fila não garantem oportunidade de finalizar. |
| Parte 0017, linhas 882 e 885 | `[407,134]`, nível 3, 29,7–30,1 s; três tentativas sem tentativa completa | O padrão alcança mais de um setor. |
| Parte 0018, linha 358, 18:46:47.250 | `[499,134]`, nível 3, 24,3 s, `notSelected` | O padrão reaparece em outro trecho da exploração. |
| Parte 0018, linha 371, 18:46:57.951 | Conclusão de `[499,134]` com espera de 3,723 s | Coordenada não basta para ligar uma conclusão ao pedido antigo. |

O diagnóstico reinicia os contadores ao trocar o setor mais antigo e após
emitir um aviso. Não se pode interpretar zero como ausência de tentativas em
toda a vida do pedido. Também não existe evidência suficiente para atribuir o
reset de espera de `[499,134]` exclusivamente à expiração.

## Mecanismo confirmado no código

1. `queueSectorActivation` renova o TTL e move o pedido para a frente. A data
   de entrada é criada somente com coleta de desempenho ativa.
2. `WorldServer::update` fornece ordenação por distância plana ao jogador mais
   próximo; idade e bloqueadores não participam dessa prioridade.
3. `generateQueue` drena BaseTiles prontos, consumindo prazo e cota de níveis.
4. O primeiro pedido não concluído faz a passagem entrar em `schedulingOnly`.
   Isso ocorre para qualquer retorno incompleto, não apenas espera de worker.
5. Os pedidos seguintes só solicitam BaseTiles. Um setor no nível CaveLiquid
   retorna sem avanço; `lastBlock=none` significa ausência de bloqueio nessa
   chamada limitada, não que ele recebeu autorização de finalizar.
6. A passagem seguinte reordena por distância. Mover uma entrada para o fim
   não fornece justiça persistente entre passagens.

Há, portanto, evidência conjunta de atraso de seleção e acesso restrito às
etapas superiores. Isso não exclui dependências legítimas ou orçamento curto.
O limite deve continuar existindo; simplesmente remover `schedulingOnly`
mudaria a ordem de efeitos de estruturas sem estabelecer sua independência.

### Integridade da fila e identidade (correção de 07/11/2026)

Cada entrada da fila de ativação precisa ter uma `GenerationRequest` da mesma
chave. Um crash reproduzível em mundo real revelou que a observação regional
tratava essa relação como garantida e chamava `Map::get()` para a cabeça da
fila. Uma entrada sem identidade convertia telemetria em `MapException`,
encerrando o `WorldServer` e devolvendo o jogador à nave.

A conclusão agora remove explicitamente o setor que acabou de ser gerado, em
vez de remover a cabeça que a fila possuir após a chamada de geração. Isso
preserva a identidade mesmo se a ordem mudar durante aquela chamada. A coleta
observacional também só percorre identidades que ainda estão na fila, não usa
`get()` para inferir a sequência e registra `headWithoutIdentity` em vez de
lançar. Uma nova ativação de uma entrada órfã recria sua identidade com uma
nova sequência, registrada como `identityRecovered`; a observação nunca
altera a fila para esconder a divergência.

Com profiling, os contadores de cardinalidade fixa são:
`generation.selection.headWithoutIdentity`,
`generation.requests.completionHeadChanged`,
`generation.requests.completionMissingFromQueue`,
`generation.requests.completedWithoutIdentity` e
`generation.requests.identityRecovered`. Eles são diagnóstico de invariável,
não sinal para alterar prioridade ou publicar trabalho adicional.

## Diretriz performance-first e entrega por CPU

O objetivo primário do agendador é maximizar a **velocidade de entrega de
setores completos** para a capacidade de CPU disponível na máquina do jogador.
Baixa utilização de CPU não é uma meta do runtime. A política deve usar a
capacidade disponível de forma agressiva quando há backlog, desde que isso não
reduza o throughput por contenção nem viole os contratos de publicação.

O teste de 07/11/2026 torna esta prioridade mensurável: quatro workers
executaram BaseTiles e microdungeons com atrasos máximos de fila de poucos
milissegundos, enquanto a fila autoritativa atingiu dezenas de segundos de
idade e esgotou repetidamente seu orçamento de geração. Portanto, o próximo
gargalo a tratar não é a admissão na pool, mas a entrega serial de resultados
e a seleção dos pré-requisitos que permitem concluir um setor.

Uma coleta posterior com seleção automática de 11 workers confirmou o mesmo
limite por outro ângulo: havia fila média de 41,9 setores e espera máxima de
75,6 s, embora as amostras de BaseTiles mostrassem apenas 0,22 worker ativo em
média. O orçamento adaptativo chegou a 24 ms e quase não recuou por pressão do
update. Essa coleta não mede porcentagem contínua de CPU do sistema, mas mostra
que a capacidade da pool não está sendo mantida ocupada pela cadeia serial
atual. A prioridade passa a ser separar admissão, preparação, resultados
prontos e commit, e não apenas aumentar workers ou orçamento.

O modelo pretendido é:

1. A pool dimensiona a preparação privada conforme a CPU disponível, sem
   saturar a thread dona do mundo. Uma configuração explícita continua
   permitindo comparação e diagnóstico; a política futura deve considerar
   núcleos físicos/lógicos, pressão de memória e carga da thread dona.
2. O orçamento atual de geração por passagem passa a ser uma base, não um teto
   imutável. Com backlog ou pedidos visíveis antigos, um preview de orçamento
   adaptativo poderá conceder bursts limitados; quando o update do mundo ou o
   frame estiverem pressionados, ele retorna ao orçamento base.
3. Resultados privados já calculados, a cadeia mínima de pré-requisitos do
   setor visível e pedidos antigos recebem prioridade de entrega antes de
   prefetch distante. O aumento de orçamento não autoriza mudar a ordem de
   publicação de efeitos observáveis.
4. Workers continuam sem escrever no mundo vivo. A thread dona valida
   revisões, confirma tiles, entidades, promises, rede e persistência. Maior
   uso de CPU é obtido preparando mais trabalho independente e entregando-o
   mais rapidamente, não executando Lua ou mutações concorrentes.

As métricas de aceitação são setores completos por segundo, profundidade e
idade p50/p95/p99 da fila, tempo entre resultado pronto e commit, backlog de
pré-requisitos, tempo do update da thread dona e uso de memória. Utilização de
CPU isolada não é métrica de sucesso: oversubscription que atrase a thread dona
ou reduza setores concluídos é regressão.

Antes de tornar qualquer política agressiva padrão, comparar a mesma seed,
assets, mods, rota e save com o caminho legado, inclusive unload/reload e
multiplayer. O modo legado permanece disponível como fallback até a equivalência
ser demonstrada.

### Primeiro recorte configurável

O primeiro recorte implementado mantém o padrão `legacy` inalterado e aceita
`worldThreading.performanceMode` com os valores `legacy`, `adaptive` e
`aggressive`. Em `adaptive` ou `aggressive`, `sectorWorkers: 0` seleciona uma
pool inicial a partir de `std::thread::hardware_concurrency()`, menos
`reservedSystemWorkers` (padrão 1). `maxSectorWorkers` limita somente essa
seleção automática; uma quantidade explícita em `sectorWorkers` continua útil
para A/B e é limitada a 32 no processo.

`adaptive` aumenta o orçamento de entrega de 12 ms em passos de 2 ms a cada
`generationBudgetBacklogStep` setores na fila (padrão 8), até
`generationBudgetMaximumMilliseconds` (padrão 24 ms). `aggressive` usa o teto
de orçamento enquanto a fila estiver não vazia (padrão 36 ms). Ambos preservam
o caminho de publicação proprietário e o limite absoluto de 50 ms por
passagem. Em `adaptive`, se o update da thread dona já consumiu
`generationBudgetUpdateTargetMilliseconds` antes da geração (padrão 16 ms), o
burst recua ao orçamento base naquela passagem. O modo `aggressive` ignora esse
recuo por ser uma escolha explícita de throughput. Os contadores
`generation.performancePolicy.*` registram passagens, fila, orçamento efetivo,
tempo já gasto no update e recuos por pressão.

Esse primeiro recorte dimensiona workers na criação da pool; ele ainda não
redimensiona threads durante a execução nem mede pressão de memória. Esses
sinais permanecem itens de evolução, e os modos continuam opt-in até a
comparação de equivalência.

### Preview de alimentador de preparação independente

`worldThreading.generationPreparationFeederPreview` permanece desligado por
padrão. Quando há workers habilitados, ele reserva até
`generationPreparationFeederMilliseconds` (padrão 2, limitado a 8) de cada
passagem para examinar uma cópia ordenada de até
`generationPreparationFeederCandidateLimit` setores (padrão 32, limitado a
128). Para setores abaixo de `BaseTiles`, carrega somente `Tiles` e solicita a
etapa privada de BaseTiles. Não drena resultados, não carrega entidades, não
executa microdungeons/Lua nem altera a ordem de commit.

`WorldGenerationTasks` continua limitando tarefas e bytes; portanto o preview
não cria reserva fora do contrato existente. Os contadores
`generation.preparationFeeder.*` registram passagens, candidatos, tentativas,
admissões e prazo do próprio alimentador. A validação deve demonstrar
mais workers ocupados e menor espera da fila sem diferença de mundo, mod, save
ou multiplayer.

### Preview de preparação da cadeia visível

`worldThreading.generationDependencyPreparationPreview` requer o alimentador
anterior e permanece desligado por padrão. Para até
`generationDependencyPreparationRootLimit` raízes visíveis (padrão 1, máximo
4), ele constrói uma descrição limitada da cadeia até `Complete` e acrescenta
à frente do alimentador somente os nós `BaseTiles` sem pré-requisito pendente.
Esses vizinhos não são promovidos a pedidos visíveis: não ganham TTL, entidades,
Lua, callbacks, commit ou prioridade persistente. O mesmo prazo do alimentador,
limite de candidatos e `WorldGenerationTasks` limita a preparação privada.

Planos truncados não autorizam trabalho adicional. Os contadores
`generation.preparationFeeder.dependency*` distinguem raízes, planos, fronteira
e truncamento. O objetivo é atacar a espera antes de um setor novo no céu ficar
pronto, não apenas reordenar o resultado já calculado.

### Preview de capacidade BaseTiles para setores padrão

`worldThreading.generationBaseTilesDeliveryCapacityPreview` amplia somente a
janela de preparação privada de BaseTiles, de `2 × workers` para
`generationBaseTilesPendingMultiplier × workers` (padrão 4, limitado a 2–4).
O resultado ainda entra na fila existente de resultados prontos e só é
instalado pela thread dona. MicroDungeons conserva seu limite próprio; a
reserva global de `WorldGenerationTasks` é dimensionada para não impedir
BaseTiles se um planejador de dungeon for criado primeiro.

O preview não muda TTL, orçamento serial, entidades, scripts, persistência ou
ordem de commit. `generation.baseTiles.pendingLimit` registra o teto efetivo;
validar memória, resultados stale, atraso pronto→commit, expirações e
setores/s antes de adotá-lo por padrão.

### Preview de planejamento MicroDungeons da cadeia visível

`worldThreading.generationDependencyMicroDungeonPreparationPreview` requer os
previews de cadeia, `microDungeonPlanPreview` e
`microDungeonPlanningWorkerPreview`; permanece desligado por padrão. Ele aceita
até `generationDependencyMicroDungeonCandidateLimit` candidatos (padrão 4,
máximo 16) que a análise já classificou como fronteira `MicroDungeons`:
BaseTiles do próprio setor e dos vizinhos necessários já existem.

O worker recebe o mesmo snapshot privado de halo/revisões do caminho normal e
calcula somente o plano. Aplicação de tiles, entidades, placements e promises
não é antecipada e continua em `generateMicroDungeons` na thread dona. Os
contadores `generation.preparationFeeder.microDungeonPlanAttempted` e
`.microDungeonPlanAdmitted` medem o recorte. Como o snapshot revalida um halo
3×3, a admissão exige `BaseTiles` em todo o halo, não somente os vizinhos
cardinais da fronteira. `.microDungeonPlanCommitted` e `.microDungeonPlanStale`
separam resultado útil de trabalho descartado.

O prefetch de setores usa a mesma interpretação: em `adaptive` ou
`aggressive`, `sectorWorkers: 0` habilita prefetch quando o padding da fidelidade
estiver configurado. Antes dessa correção, a pool automática podia existir e o
prefetch permanecer desligado por testar apenas o valor explícito zero.

Exemplo de teste:

```json
"worldThreading" : {
  "performanceMode" : "adaptive",
  "sectorWorkers" : 0,
  "reservedSystemWorkers" : 1,
  "maxSectorWorkers" : 0,
  "generationBudgetBacklogStep" : 8,
  "generationBudgetMaximumMilliseconds" : 24,
  "generationBudgetUpdateTargetMilliseconds" : 16,
  "generationPreparationFeederPreview" : true,
  "generationPreparationFeederMilliseconds" : 2,
  "generationPreparationFeederCandidateLimit" : 32,
  "generationDependencyPreparationPreview" : true,
  "generationDependencyPreparationRootLimit" : 1,
  "generationBaseTilesDeliveryCapacityPreview" : true,
  "generationBaseTilesPendingMultiplier" : 4,
  "generationDependencyMicroDungeonPreparationPreview" : true,
  "generationDependencyMicroDungeonCandidateLimit" : 4
}
```

### Preview de decode privado de tiles salvos

`worldThreading.savedTileDecodeAsyncPreview` permanece desligado por padrão.
Quando ativo e a pool setorial possui workers, o prefetch e o alimentador de
BaseTiles podem capturar o `ByteArray` já lido pela thread dona e deslocar
somente descompressão/desserialização para a pool. O resultado preserva ticket
e encarnação do storage; normalização por materiais/líquidos, instalação,
callbacks e entidades continuam na dona. Cargas completas permanecem síncronas.
`savedTileDecodePendingLimit` limita snapshots simultâneos; o padrão é duas
vezes a quantidade de workers. Unload e teardown descartam a promessa dona.

```json
"worldThreading" : {
  "savedTileDecodeAsyncPreview" : true,
  "savedTileDecodePendingLimit" : 16
}
```

## Contratos por etapa

| Etapa | Leituras e escritas atuais | Contrato necessário no runtime por setor |
|---|---|---|
| BaseTiles | Preparação isolada, template privado; instalação valida identidade e revisões do setor | Resultado imutável; instalação pela thread dona; tentativa única por identidade e etapa; descarte seguro após unload. |
| MicroDungeons | Template, colocações ordenadas, `m_queuedPlacements`, promises, facade do mundo, alterações de tiles e entidades | Planejar sobre snapshots; registrar footprint real; reservar os donos envolvidos; publicar efeitos e promises uma vez em ordem estável. |
| CaveLiquid | Flood fill com wrap horizontal, materiais e IDs de dungeon; grava líquido/background na área acolchoada, inclusive vizinhos | Snapshot do halo e versões; plano de escrita por setor; coordenação dos donos e publicação coerente. A etapa anterior dos vizinhos não é um lock. |
| Finalize | Ajustes de tiles e colisão, grass, plantas, objetos e tesouros; acesso a vizinhos e mapa de entidades; ativação de líquidos | Separar cálculo local, planejamento das colocações e publicação; footprint das plantas/objetos; callbacks de ativação respeitam autoridade e dependências. |
| Terraform | Caminho especial, alterações de bioma e estado persistente | Identidade e revisões próprias; preservar ordem das edições e revalidar entradas; não tratar como Finalize comum. |
| Load/unload e persistência | Carregamento de entidades, metadados, TTL, serialização e banco | Dono definido, handoff confirmado, revogação de pedidos e checkpoint coerente; nenhuma referência mutável sobrevive ao unload sem contrato. |

Fontes: `StarWorldStorage.cpp` (`queueSectorActivation`, `generateQueue`,
`generateSectorToLevel`), `StarWorldGeneration.cpp` (`generateSectorLevel`,
`generateMicroDungeons`, `generateCaveLiquid`, `prepareSector`) e
`StarWorldServer.cpp` (`update`). O acolchoamento das microdungeons não prova
o limite de todos os efeitos de objetos, plantas e callbacks.

## Recorte de implementação proposto

### A. Identidade de pedido independente do profiling

- Identificar cada permanência na fila por mundo/encarnação, setor e sequência
  de admissão. Separar essa identidade daquela da tarefa BaseTiles.
- Renovar interesse preserva sequência e idade; expiração, conclusão e unload
  encerram o episódio; nova entrada recebe nova sequência.
- A política de execução precisa funcionar com HUD e profiling desligados.
  Não reutilizar `m_generationQueuedAt` como única fonte de prioridade, pois
  seu ciclo de vida depende da coleta.
- Eventos limitados devem conter identidade, nível atual/alvo e motivo terminal.
  Contadores agregados continuam corretos quando os detalhes forem descartados.

### B. Seleção e dependências

- Separar candidato visível, preparação independente e elegibilidade para
  publicar efeitos. Preservar os critérios atuais de carregamento e TTL.
- Introduzir uma oportunidade limitada de tentativa completa para o pedido
  visível mais antigo ainda solicitado, além da prioridade por proximidade.
  O orçamento de tempo/níveis é compartilhado, sem trabalho extra ilimitado.
- Se esse pedido depender de vizinhos, transmitir sua prioridade aos
  pré-requisitos. Evitar repetir visitas que só confirmam BaseTiles já prontos.
- Desempatar com sequência e coordenadas normalizadas; a ordem de término de
  workers não define prioridade de aplicação.
- Primeiro comparar seleção atual/proposta sem executar os efeitos alternativos.
  Depois habilitar a política experimental mediante validação dos conflitos.
  Isso preserva uma referência para entender alterações na geração.

Esse desenho não garante espera máxima absoluta: uma dependência externa ou
etapa indivisível pode continuar atrasando. A garantia pretendida é oportunidade
de progresso para um conjunto finito e continuamente solicitado com orçamento
positivo, mais causa explícita quando esse progresso não for possível.

### C. Execução por threads donas

- Manter a premissa de uma thread por região lógica carregada. A pool de
  preparação auxilia os donos, mas não substitui a thread local da região.
- Avançar primeiro operações com leitura imutável e escrita local comprovadas.
  Publicação com efeitos em vizinhos exige o contrato correspondente da tabela.
- O grafo atual de dependências é observacional e pode ser truncado. Não usar
  sua fronteira incompleta para autorizar publicação. O grafo executivo precisa
  representar todas as dependências ou adiar a operação explicitamente.
- Chamadas legadas sem footprint conhecido precisam de caminho coordenado
  explícito. Não executar Lua ou callbacks arbitrários em setores concorrentes
  apenas por sua âncora estar em setores diferentes.

## Critérios de validação antes de adotar a política

1. Pedidos renovados mantêm idade; reentrada após expiração/unload tem nova
   identidade; não há instalação de resultado de encarnação antiga.
2. Com conjunto finito, orçamento positivo e dependências concluíveis, todos
   os pedidos recebem tentativas completas e chegam ao estado final.
3. Zero orçamento, saturação de pool, mudança de rumo e dois jogadores não
   geram loops, crescimento ilimitado de filas ou execução fora de interesse.
4. Comparar mesma seed, assets/mods e rota com referência: tiles, estruturas,
   IDs, entidades, líquidos, colisão, saves e reload. Qualquer diferença precisa
   ser explicada antes de declarar compatibilidade.
5. Reduzir idade dos pedidos e passagens sem avanço; medir também frame do
   cliente e ciclo completo da thread, incluindo `sync`, para não transferir
   a latência para outro ponto.
6. Separar ganhos do agendador dos ganhos de paralelismo. Workers ocupados ou
   mais setores preparados não demonstram conclusão mais rápida por si só.

## Política experimental de justiça

`worldThreading.generationFairnessSchedulerPreview` habilita uma promoção
limitada do pedido com menor sequência ainda presente na fila. A cada
`generationFairnessIntervalPasses` passagens (padrão 4; limitado a 1–60), a
fila é primeiro ordenada pela distância atual ao jogador e então esse único
pedido antigo é movido para a frente. O orçamento de tempo e de níveis continua
o mesmo; não há trabalho adicional garantido nem prioridade permanente.

Quando há uma promoção, `generationFairnessReserveMilliseconds` reserva os
últimos 2 ms do orçamento (ajustável de 0 a 8 ms) para a primeira tentativa da
fila promovida. A drenagem de `BaseTiles` já preparados para antes desse limite.
Assim, uma sequência de commits prontos não pode consumir integralmente os
12 ms usuais antes de consultar o pedido antigo. A tentativa continua limitada
pelo deadline original e uma etapa não interrompível ainda pode ultrapassá-lo;
o parâmetro não transforma microdungeons em trabalho paralelo. Com profiling,
`generation.fairness.reservePasses`, `.readyCommitDeferred`,
`.reservedAttempted` e `.reservedAdvanced` verificam se a reserva de fato
concedeu a oportunidade esperada.

Ao ser selecionado, o setor continua usando `generateSectorToLevel` com alvo
`Complete`. A rotina existente percorre os pré-requisitos de geração antes de
avançar a etapa dependente, portanto a oportunidade é herdada por eles dentro
da chamada autoritativa. Esta versão não insere vizinhos como pedidos de jogador
nem permite publicar estruturas fora da ordem já existente.

Com profiling ativo, `generation.fairness.promoted` registra promoções e
`generation.fairness.alreadySelected` registra quando o pedido antigo já era o
mais próximo. Com `regionalDetails`, o evento limitado de pedido usa
`fairnessPromoted`. Comparar essa opção desligada e ligada na mesma rota é
obrigatório antes de adotá-la como padrão.

`worldThreading.generationExpiryRiskSchedulerPreview` é um recorte separado,
também desligado por padrão. Antes da prioridade persistente e da amostragem de
fairness, ele examina o TTL restante das entradas já visíveis e promove somente
a que possui menor tempo restante dentro de
`generationExpiryRiskMilliseconds` (padrão 3.000 ms; limitado a 250–12.000).
Recebe uma tentativa reservada pela mesma janela final do orçamento de fairness,
mas o TTL não é renovado e o alvo continua `Complete`; logo não cria trabalho,
entidades ou persistência extras. `generation.expiryRisk.*` mede promoções,
TTL restante, reservas, tentativas e avanços. O objetivo é reduzir expirações,
não substituir a ordem normal por distância.

### Preview de continuidade da cadeia visível

`worldThreading.generationVisibleChainContinuationPreview` permanece desligado
por padrão. Quando uma tentativa `Complete` de um setor visível avança ao menos
uma etapa, mas ainda não conclui, a fila preserva `(setor, sequência)` para uma
única tentativa `Complete` na passagem seguinte. O setor fica na frente ao
final da passagem atual e a passagem termina, evitando que o tail
`BaseTiles`-only o consuma imediatamente ou que a mesma cadeia monopolize o
orçamento.

Uma continuidade válida recebe uma tentativa antes da próxima seleção de risco
de expiração; como é estritamente uma tentativa, o risco volta a prevalecer na
passagem seguinte. A identidade é revalidada antes de retomar; unload,
expiração, conclusão ou reentrada descartam o marcador. Não há renovação de
TTL, aumento de orçamento, mudança de prioridade de commit ou execução de
worker extra.
`generation.visibleChainContinuation.scheduled`, `.resumed`, `.attempted` e
`.dropped` medem o contrato.

Para análise por pedido, setores visíveis registram uma única vez os marcos
`tilesAvailable`, `baseTilesReady`, `microDungeonsReady`, `caveLiquidReady`,
`finalizeReady` e `terraformReady` em `generationRequests` quando
`regionalDetails` está ativo. Eles não representam pedidos para vizinhos
recursivos. O protocolo A/B e os critérios de avanço estão em
[regional-runtime-expiry-risk-validation-protocol.md](regional-runtime-expiry-risk-validation-protocol.md).

Além dos marcos, cada tentativa visível `Complete` registra o primeiro bloqueio
causal em `generation.visibleAttempt.blocked.*`: `baseTiles`,
`microDungeons`, `caveLiquid`, `finalize`, `terraform`, `neighbor`, `deadline`,
`levelLimit` ou `invalidSector`. Os eventos por pedido usam
`visibleCompleteAttempted`, `visibleCompleteFinished` e `visibleBlocked.*`.
O traço cobre somente raízes visíveis e preserva o diagnóstico pré-existente do
pedido mais antigo; recursão de vizinhos não vira falso pedido de jogador.

`worldThreading.generationDependencySchedulerPreview` usa o mesmo intervalo,
mas constrói a árvore limitada de pré-requisitos do pedido antigo. Se uma etapa
executável dessa árvore já tiver um pedido legítimo na fila, ela recebe a
promoção e é executada somente até o nível necessário. Assim, uma BaseTiles ou
etapa intermediária não é removida da fila como se estivesse completa. Se o
pré-requisito não estiver na fila, o scheduler registra
`generation.dependencyScheduler.frontierUnavailable` e promove o pedido antigo
pelo caminho normal, deixando a rotina autoritativa existente decidir o
carregamento recursivo. As métricas `generation.dependencyScheduler.promoted`
e o evento `dependencyPromoted` permitem comparar os dois casos. Um grafo
truncado nunca autoriza promoção de dependência; ele é contado em
`generation.dependencyScheduler.truncated` e recai no caminho normal.

Essa política nunca adiciona interesse, carrega um vizinho somente para o
agendador, ou altera a ordem de publicação de estruturas, entidades e tiles.

## Conclusão prioritária de setores visíveis

`worldThreading.generationVisibleCompletionPreview` permanece desligado por
padrão. Quando ativado, resultados `BaseTiles` prontos de setores com um pedido
de ativação são instalados antes de resultados de prefetch. Após a instalação,
o setor visível mais próximo recebe uma promoção única para a tentativa normal
de `Complete`; isto representa a notificação de que seu primeiro pré-requisito
foi satisfeito. Uma reserva de justiça tem precedência e impede essa promoção
na mesma passagem.

O preview não altera dependências, não inicia entidades distantes e não publica
microdungeons fora do caminho autoritativo. `generation.visibleCompletion.readyResults`
e `.promoted`, mais o evento `baseTilesSatisfied`, permitem verificar se a
prioridade reduziu a espera sem deslocar a fome para pedidos antigos.

Configuração de teste:

```json
"worldThreading" : {
  "generationVisibleCompletionPreview" : true,
  "generationPersistentPriorityPreview" : true,
  "generationExpiryRiskSchedulerPreview" : true,
  "generationExpiryRiskMilliseconds" : 3000,
  "generationVisibleChainContinuationPreview" : true
}
```

`worldThreading.generationReadyResultSchedulerPreview` é o recorte seguinte
da entrega. Quando ativo, resultados BaseTiles prontos de pedidos visíveis são
commitados pela posição atual da fila autoritativa, não pela ordem de término
do worker nem por nova ordenação geométrica. Portanto, uma promoção persistente
de justiça ou de dependência já aplicada à fila é preservada no commit. Resultados
de prefetch ficam depois dos visíveis e usam coordenadas como desempate estável.
Permanece desligado por padrão.

As métricas `generation.readyResults.selectedVisible`,
`.selectedPrefetch` e `.selectedByQueuePriority` verificam a seleção; os
contadores existentes de atraso até commit verificam se ela reduziu trabalho
pronto retido sem alterar equivalência de mundo/mods/saves.

```json
"worldThreading" : {
  "generationReadyResultSchedulerPreview" : true
}
```

`worldThreading.generationMicroDungeonReadyResultSchedulerPreview` estende o
mesmo contrato para o **plano privado já concluído** de MicroDungeons. Requer
`generationReadyResultSchedulerPreview`; indexa promessas concluídas, ordena
somente setores ainda visíveis pela fila autoritativa e chama o caminho normal
na thread dona para revalidar revisões e publicar tiles, entidades e promises.
O worker nunca recebe fachada do mundo nem publica efeitos. Setores auxiliares
não visíveis permanecem pendentes até que sua geração normal os possua.

Ele existe porque Tracy 19 mostrou BaseTiles com folga de capacidade (média de
3,72 pendências para teto 44), enquanto MicroDungeons foi o primeiro bloqueio
em 1.282 tentativas visíveis. Assim, a oportunidade de throughput é entregar o
resultado já calculado, não aumentar cegamente workers ou o orçamento serial.
Permanece desligado por padrão enquanto a equivalência é validada.

```json
"worldThreading" : {
  "generationReadyResultSchedulerPreview" : true,
  "generationMicroDungeonReadyResultSchedulerPreview" : true
}
```

Medir `generation.microDungeonPlan.readyResultsEnqueued`,
`.readyResultsPendingTotal`, `.readyResultsSelectedVisible`,
`.readyResultsCommitted` e `.readyResultDelay`, além de stale, expirações,
setores completos/s e equivalência de saves/mods. A aplicação ainda pode ser
uma etapa serial indivisível; esta mudança reduz espera ociosa, não autoriza
escrita concorrente.

`worldThreading.generationPersistentPriorityPreview` é o segundo recorte.
Quando `generationFairnessSchedulerPreview` está ativo, ele passa a acompanhar
a política de justiça por padrão; o campo explícito ainda pode ser definido
como `false` para uma comparação A/B. Quando um pedido promovido perde a
tentativa porque uma etapa anterior alcançou o deadline, ele preserva
`(setor, sequência)` até a próxima passagem. Reentrada do mesmo setor recebe
outra sequência e descarta o marcador antigo. A prioridade é removida logo
após a primeira tentativa real; ela não vira prioridade permanente.
`generation.persistentPriority.stored`,
`.resumed`, `.attempted` e `.dropped` verificam esse contrato.

## Primeiro recorte implementado

`WorldStorage` mantém uma sequência por entrada e um epoch por instância de
armazenamento. Renovação preserva sequência/idade; conclusão e expiração
removem a identidade; reentrada recebe outra sequência. A identidade existe
com HUD e profiling desligados. O epoch é único no processo, não persistido
em save nem comparável entre execuções distintas.

O unload atual pode preservar interesse na fila: por isso `unloaded` é uma
observação não terminal, e não força cancelamento ou reentrada artificial.
Encerramento do mundo ainda não emite evento terminal individual para todos
os pedidos restantes; ausência de conclusão nesse caso não prova perda.

Com `regionalDetails`, a comparação seleciona a cabeça atual e o pedido de
menor sequência, sem alterar ordem, orçamento ou efeitos. Esse candidato
antigo ainda não inclui elegibilidade por footprint nem prioridade herdada;
é apenas a primeira medição da diferença entre distância e antiguidade.
O registro da cabeça ocorre antes da drenagem de resultados prontos, portanto
não prova que houve orçamento para executar essa cabeça na mesma passagem.

Eventos de ciclo de vida têm limite de 64 por janela e observações de seleção
limite separado de 16; contadores de descarte tornam a perda explícita. O
detalhamento e a varredura adicional são desligados com `regionalDetails`.
Sem profiling, permanece somente o custo de manter identidade/data por entrada.
Essa etapa não cria novos workers nem transfere a simulação para as threads
das regiões lógicas.
