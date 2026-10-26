# Medição detalhada de NPCs

Instrumentação da fase 0 do [plano de rework](npc-ai-rework-action-plan.md).
Acrescenta dados ao `ExplorationProfile` e zonas Tracy. Não move NPCs para
workers, altera callbacks, APIs de mods, saves ou protocolo de rede.

## Ativar

Com o processo fechado, incluir no objeto principal de `starbound.config`
(ou `starbound_server.config` no servidor dedicado):

```json
"explorationProfiling" : {
  "enabled" : true,
  "npcDetails" : true,
  "intervalSeconds" : 5,
  "slowOperationMilliseconds" : 50,
  "entityDetailLimit" : 12
}
```

Reiniciar o jogo/servidor. As opções são lidas na criação dos coletores.
`npcDetails` é falso por padrão e depende de `enabled`. Esta coleta em log é
independente do HUD: ocultar `/debug` não a desliga. Para comparar desempenho
sem instrumentação, usar `enabled: false` e uma build normal sem Tracy.

## O que foi acrescentado

Relatórios do servidor passam a incluir `npcStages`, com nomes fixos:

| Campo | Trecho medido |
| --- | --- |
| `update` | Corpo de `Npc::update`; não inclui toda a manutenção externa da entidade no mundo |
| `scriptDispatch` | Despacho do componente periódico, incluindo mensagens de threads e controle de cadência |
| `movement` | `ActorMovementController::tickMaster` do NPC |
| `status` | `StatusController::tickMaster` do NPC |
| `shared` | `Npc::tickShared`, incluindo ferramentas e apresentação compartilhada |
| `init`, `uninit` | Entrada e saída do NPC do mundo |
| `damage`, `damagedOther` | Processamento de dano recebido e notificação de dano causado |
| `shouldDestroy`, `destroy` | Decisão de destruição e processamento da destruição |
| `interact`, `receiveMessage`, `callScript` | Entradas síncronas do NPC, inclusive fora de seu update periódico |
| `lua.update` | Invocação de função chamada `update` por `LuaBaseComponent::invoke` |
| `lua.invoke` | Outras invocações por esse componente; nomes arbitrários de mods não viram chaves do relatório |
| `lua.init`, `lua.uninit` | Callbacks de inicialização e finalização dos componentes Lua |
| `lua.message`, `lua.threadMessage`, `lua.eval` | Handlers de mensagem, mensagens de threads e avaliação de código pelos componentes instrumentados |
| `pathfinding.init` | Inicialização/reinicialização do A* de plataforma, incluindo cópias que reiniciam a busca |
| `pathfinding.explore` | Cada chamada de exploração do A* de plataforma |
| `outgoing.callScriptedEntity` | Chamada Lua a outra entidade, incluindo validação e execução síncrona |
| `outgoing.sendEntityMessage` | Envio via binding Lua; pode incluir atendimento local síncrono |
| `outgoing.interact` | Chamada a `WorldServer::interact` durante uma cadeia de NPC |

As subetapas herdam um contexto na mesma thread. Não consultam um NPC vivo
depois do retorno nem propagam o contexto para workers. Entradas usam o
coletor do mundo servidor; NPCs de apresentação do cliente não são incluídos.
As etapas `movement`, `status` e `shared` detalhadas cobrem o ramo mestre.

`samples` informa frequência de chamadas medidas, não número de NPCs distintos.
`scriptDispatch` pode ocorrer sem um callback periódico, dependendo do
`script.updateDelta`. `lua.update` conta invocações efetivas com esse nome,
incluindo componentes de status e chamadas explícitas, não apenas o script
principal do NPC. Uma falha durante a execução também produz uma amostra.

### Tempos inclusivos e exclusivos

Cada etapa fornece total, média, máximo, amostras lentas e percentis, além de:

- `exclusiveMilliseconds`: total menos os escopos instrumentados filhos no
  mesmo coletor. Não é tempo exclusivamente de CPU; pode incluir bloqueios e
  trabalho não detalhado, além do custo da instrumentação.
- `loggingTotalMilliseconds`: tempo síncrono em `Logger::log`, incluindo seus
  filhos instrumentados. A formatação anterior à chamada não entra nesse campo.

Exemplo: `update → scriptDispatch → lua.update → pathfinding.explore` é uma
hierarquia. Os totais inclusivos **não devem ser somados**. A soma dos tempos
exclusivos evita essa duplicação dentro da cadeia instrumentada, mas não deve
ser somada novamente aos totais de `entityTypes` ou `stages`.

Se um NPC chama um objeto e esse objeto executa Lua ou busca um caminho de
forma síncrona, esse custo faz parte da cadeia iniciada pelo NPC. O relatório
não atribui propriedade de cada callback ao objeto destinatário. Chamadas
recursivas entre NPCs também podem sobrepor totais inclusivos.

O subtotal legado `entityTypes.Npc.luaTotalMilliseconds` continua medindo apenas
o callback periódico observado por `EntityLuaScope`. Ele não foi redefinido
como soma de todos os novos campos Lua. Logging legado é preservado.

### População e resultados de pathfinding

Em `counts`:

- `npc.populationTickSamples`: percursos de atualização de entidades concluídos.
- `npc.updatedEntitiesTotal`: soma dos NPCs visitados nesses percursos.
- `npc.maximumUpdatedEntitiesPerTick`: maior quantidade visitada em um percurso.
- `npc.pathfinding.pendingExploreCalls`: chamadas que retornaram busca incompleta.
- `npc.pathfinding.successfulExploreCalls`: chamadas que retornaram sucesso.
- `npc.pathfinding.failedExploreCalls`: chamadas que retornaram falha.

Dividir `updatedEntitiesTotal` por `populationTickSamples` fornece a média de
NPCs visitados por tick da janela. Não é a população total salva no planeta.
Uma atualização abortada por exceção pode ter tempos parciais sem uma amostra
de população concluída. Os resultados do A* são contados por chamada, não por
busca única; consultar uma busca já finalizada pode contar seu resultado de novo.
Uma rota parcial aceita por `returnBest` segue a semântica de sucesso do A*
existente. Não há inferência de que o NPC chegou ao destino.

### Percentis com memória limitada

`stages`, `entityTypes` e `npcStages` incluem `p50UpperMilliseconds`,
`p95UpperMilliseconds` e `p99UpperMilliseconds`. São limites superiores
aproximados do percentil pela ordem das amostras da janela, não percentis exatos.

Cada distribuição usa 256 contadores fixos: primeiro intervalo abaixo de 1 µs,
seguido por oito subdivisões lineares de cada potência de dois. Fora do primeiro
intervalo e do último intervalo de saturação, a largura é no máximo 12,5% do
limite inferior. O resultado é limitado ao máximo observado; saturação usa esse
máximo. O campo `percentileMethod` identifica o método. Não se armazenam amostras
individuais nem nomes, IDs ou argumentos arbitrários para criar categorias.

Os histogramas reiniciam a cada relatório. Não tirar média de p95/p99 entre
janelas para representar uma sessão. Usar janelas equivalentes e informar a
contagem de amostras; capturas Tracy são uma alternativa para análise detalhada.
`stages.serverUpdate` mede `WorldServer::update`, não o tick completo da thread
com sleep, espera anterior pelo mutex, pacotes e sync externo.

## Custo e limites da cobertura

Com diagnóstico desligado, os novos escopos não leem relógios, registram tempos
ou alocam dados; permanecem verificações de contexto e de tipo do mundo.
Com `npcDetails` ligado, cada escopo usa duas leituras de relógio, um acumulador
de logging por thread e contadores fixos. Não acrescenta mutex nem escreve uma
linha por callback. A serialização acontece no relatório periódico já existente.
Os histogramas acrescentam cerca de 2 KiB por distribuição; as etapas NPC têm
quantidade fixa. O overhead real precisa ser medido nas coletas pareadas.

Tracy ganha zonas para movimento/status/shared, A*, invocações Lua e handlers;
essas zonas só existem em builds com `STAR_ENABLE_TRACY`. As zonas genéricas
de Lua e A* no Tracy também cobrem consumidores que não são NPCs.

A [busca de rota em modo sombra](npc-path-shadow.md) é um diagnóstico **separado**,
desligado por padrão, que acrescenta cópia de tiles, busca serial de referência
e busca em worker. Suas chaves `npc.shadow.*` só aparecem quando
`worldThreading.npcShadowWorkers` é positivo e `npcDetails` está ativo. A busca
de referência e a do worker não entram nas contagens de A* autoritativo acima;
elas não devem ser habilitadas para obter o baseline de CPU dos NPCs.

Esta entrega não mede automaticamente:

- Reação entre um estímulo percebido e uma decisão observável. É necessário
  definir o estímulo e a resposta para cada cenário, com marcadores correlatos.
- Espera de uma promessa até resolução, tempo total de uma busca em vários
  ticks ou disputas reais de dois NPCs pela mesma porta. Duração de envio ou
  frequência de chamadas não são essas métricas.
- Todo acesso a tile/objeto nem toda execução Lua possível. Closures invocadas
  fora dos componentes e entradas instrumentados não têm cobertura garantida.
- Filas, snapshots ou reconciliação de futuros workers NPC, ainda inexistentes.

O relatório final do mundo ocorre antes da limpeza de shutdown, como antes;
desinicializações durante essa limpeza não entram nesse último relatório.

## Próxima coleta e estado

Guardar commit/build, configuração, mods/assets, hardware e cópia do universo.
Executar pelo menos cinco pares por cenário: área com NPCs vanilla e área com
NPCs do Frackin' Universe, mantendo população, rota e condições equivalentes.
Separar coleta diagnóstica de comparação com diagnóstico desligado. Usar as
contagens de chamadas para localizar a etapa que merece marcadores de reação
e conflitos. Não atribuir o gargalo dos roteadores a NPCs sem essa evidência.

As sete unidades C++ afetadas foram compiladas no ambiente Linux:
`StarExplorationProfile`, `StarNpc`, `StarPlatformerAStar`, `StarRootLoader`,
`StarWorldServer`, `StarLuaComponents` e `StarWorldLuaBindings`. Isso não equivale
a linkagem do jogo completo, compilação Windows ou execução de gameplay.

Implementação entregue para revisão; validação de gameplay, overhead, Windows
e cenários de NPCs continuam dependentes de build e coleta. Nenhuma medição
real de ganho foi produzida por esta alteração.
