# Instrumentação de entidades, setores e eventos de história

Esta instrumentação é observacional e usa `explorationProfiling.enabled`.
Sem esse perfil, não instala o observador de transferências de líquido, não
coleta contadores de setores/fios e não guarda hops de mensagens. Não muda
Lua, save, ordem de update, protocolo de rede ou autoridade de entidades.
Os relatórios `[ExplorationProfile]` continuam separados por componente e
janela (`server`, `client`, `playerQuests`, `universeScripts`).

## Entidades e bordas

Os estágios `entity.monster.*`, `entity.vehicle.*` e `entity.stagehand.*`
medem callbacks identificados por `master` ou `slave` no mundo que os executa.
Incluem `init`, `uninit`, mensagens, `callScript`, `evalScript` e, conforme o
tipo, dano, interação, destruição ou controles de veículo. São tempos de
parede **inclusivos**; callbacks invocados durante o update também constam no
tempo `entityTypes` do mesmo tipo. Não somar esses tempos como trabalho
independente. `vehicle.control` compreende `control_on`, `control_off`,
`control_all` e `aim` no receptor. O perfil `client` registra o trabalho de
entidades executado no cliente; `server` registra o trabalho no host.

No relatório `server`, `entity.sectorTransfers` conta mudança do setor da
**posição** antes/depois do update, incluindo subtotais para monstro, veículo
e stagehand. `entity.footprintSamples` conta entidades observadas a cada 60
passos; `entity.multiSectorFootprints` e subtotais contam caixas espaciais
que tocam mais de um setor físico de 32 tiles. Isto é amostragem de footprint,
não número de entidades simultâneas em todos os instantes. Posições/caixas
inválidas são ignoradas para que o diagnóstico não altere a decisão do jogo.

`storage.unloadDeferred.keepAlive` e `storage.unloadDeferred.overlap` contam
**tentativas** adiadas, não setores únicos. O primeiro inclui TTL vencido
renovado por entidade `keepAlive` e uma tentativa direta de unload recusada;
o segundo indica sobreposição com entidade pertencente a outro setor ainda
vivo. Um trabalho BaseTiles pendente não adia o unload hoje: os contadores já
existentes `generation.baseTiles.pendingInvalidatedOnUnload` e
`generation.tasks.cancelled` mostram sua invalidação e saída.

## Fios e líquidos

`wiring.process`, `wiring.loadNetwork` e `wiring.evaluate` medem tempo de
parede. `wiring.networks`, `networkSectorsTotal`, `newlyLoadedSectors` e
`entitiesEvaluatedTotal` são somas por janela. `maxima` publica os maiores
`wiring.networkSectorsMax` e `wiring.entitiesEvaluatedMax`. A rede inclui os
setores alcançados por conexões, inclusive setores carregados durante a busca;
o número não representa apenas tiles próximos do jogador.

`liquid.updateSamples`, `activeCellsBeforeTotal` e `activeCellsAfterTotal`
permitem calcular a média de células ativas por update;
`maxima.liquid.activeCellsBeforeMax` guarda o pico da janela. Os contadores
`liquid.crossSectorLevelTransfers` e `crossSectorPressureTransfers` vêm de
transferências efetivas acima do limiar de ativação do motor, com coordenadas
normalizadas no eixo X. `crossSectorInteractionEffects` conta transformações
entre líquidos vizinhos e `crossSectorCollisionEffects` conta alterações de
tile em colisões, sempre após detectar que os dois lados pertencem a setores
distintos. São **operações**, não volume de líquido nem número de fronteiras
únicas. O observador é instalado somente no perfil opcional; exceções da
coleta não interrompem o motor depois de uma mutação.

## Mensagens de missão

`storyHops` contém até 64 eventos por janela, com ID numérico temporário,
rótulo fixo e tempo na fila. `story.hopsDropped` conta o excedente. Nenhum
evento inclui nome/conteúdo da mensagem, identificador de jogador ou payload.
Um ID é herdado apenas quando uma chamada síncrona em processo enfileira a
próxima mensagem. `universe.messageDequeued` mede a espera desde a entrada
na fila local do universo; `world.messageDequeued` mede a espera desde
`sendWorldMessage`, incluindo eventual criação do mundo; `world.entityMessage`
marca um encaminhamento para entidade dentro do contexto atual.
`player.questMessage` marca a entrada no gerenciador do jogador.

O ID **não atravessa pacotes cliente/servidor**, não identifica uma missão e
não persiste no save. Assim, hops do cliente e do servidor não formam uma
cadeia causal completa; correlação entre máquinas exigiria extensão de
protocolo versionada e consentida, com limites e compatibilidade com clientes
antigos. Ausência de hop pode significar perfil desligado, limite de 64,
entrada sem mensagem ou chamada por outro caminho; não prova ausência de
evento. Comparar com os estágios agregados de
[scripts de história](story-script-profiling.md).

## Uso e limites

Ativar `explorationProfiling.enabled: true` apenas nas sessões diagnósticas.
`regionalDetails` continua opcional para o restante das métricas regionais.
O profiling acrescenta relógio, contadores, algumas consultas espaciais e,
para líquido, uma chamada por transferência significativa. Comparar o mesmo
save e percurso com perfil ligado/desligado antes de usar os tempos para
decidir arquitetura. A coleta não comprova segurança para mover entidades,
fios, água ou scripts para outras threads.
