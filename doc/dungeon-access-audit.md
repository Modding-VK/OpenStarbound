# Auditoria de acessos da geração de dungeons

Referência examinada: Experimental, `bb3628e9`. Auditoria estática do caminho
writer → facade → WorldServer/WorldStorage. Não constitui prova de segurança
de todos os callbacks de mods, nem medição de ganho de desempenho.

## Resultado

O writer atual não é um plano privado. `buildDungeon` remove entidades antes
de colocar cada peça; brushes podem definir player start imediatamente.
Adiar apenas `flush` não isola a geração. Também não basta reservar os setores
dos buffers de tiles: templates, índices de entidades e filas de rede/líquidos
são dependências adicionais.

## Matriz de operações

| Caminho | Estado acessado e efeitos | Destino proposto |
| --- | --- | --- |
| `buildDungeon`: conectores, preserveTiles, placementCounter, escolhas | Coleções locais e RNG do gerador; canPlace consulta facade | Worker com definições imutáveis, RNG próprio e visão de leitura |
| Writer: materiais, mods, líquidos, IDs | Buffers locais; markPosition atualiza bounds e IDs | Plano privado de valores, conservando substituições por coordenada |
| `flushLiquid` | Agrupa líquidos conectados, calcula pressão e transfere ao buffer; não aplica no mundo | Candidato a cálculo privado com inputs próprios |
| `checkSolid/checkOpen/getDungeonIdAt` | Tiles vivos; helpers podem sinalizar região conforme withSignal | Snapshot com presença explícita; ausência nunca vira tile vazio |
| `checkOceanLiquid` | `WorldTemplate::blockInfo`, incluindo caches do template | Template isolado ou dados previamente capturados |
| `clearTileEntities` | Consulta caixa expandida em 1 tile; verifica spaces/roots/anchors; remove entidades | Intenção serial com IDs/incarnações e revalidação de ocupação |
| `setPlayerStart` | Altera estado global do mundo imediatamente | Comando global ordenado |
| `markRegion` | signalRegion e ativação de líquidos quando markForActivation | Aplicação pelo dono do mundo |
| `markTerrain/markSpace` | Alteram WorldTemplate por polígonos; alcance excede posições dos buffers | Dependência versionada de template, aplicação serial |
| Materiais/mods/líquidos/IDs na facade | modifyServerTile; materiais também limpam líquidos | Comandos de aplicação com efeitos auxiliares preservados |
| Objetos/veículos | signalRegion, criação e addEntity; orientação/colocação depende do mundo | Preparação de descrição; criação/instalação serial inicialmente |
| Árvores/bioma | Template, posição calculada, spaces/roots, ajuste de tiles e criação de entidade | Expandir footprint pelo resultado; serial até isolar consultas |
| NPCs/monstros/stagehands/drops | Factories, RNG e addEntity | Descrições e sementes explícitas; init serial |
| Fios | Consulta WireEntity em ±16 tiles por endpoint; conecta todos os pares input/output encontrados | Capturar identidade/topologia e validar antes de aplicar |

Fontes: [writer e buildDungeon](../source/game/StarDungeonGenerator.cpp),
[facade DungeonGeneratorWorld](../source/game/StarWorldGeneration.cpp),
[WorldServer](../source/game/StarWorldServer.cpp).

## Efeitos que precisam sobreviver à separação

`WorldServer::modifyServerTile` não é somente obter um ponteiro: invalida colisão,
visita a posição no motor de líquidos e enfileira updates de tiles. Aplicar um
array preparado diretamente sem reproduzir esses efeitos altera o contrato.

`signalRegion`, durante m_generatingDungeon, carrega setores imediatamente via
WorldStorage::loadSector; fora desse modo, enfileira ativação. Carregamento pode
inicializar entidades. Um worker não pode chamar esse helper para completar
um snapshot enquanto calcula.

`addEntity` reserva ID, executa init, adiciona ao índice e atualiza tiles de
TileEntity. `removeEntity(false)` ainda atualiza tiles, produz pacotes de
destruição para clientes, remove do índice e executa uninit. O argumento false
evita destroy, mas não torna a remoção livre de callbacks ou efeitos globais.

`spawnNpc` avalia Random::randu64() como argumento de fallback de getUInt.
Em C++, esse argumento é avaliado mesmo quando a seed está no JSON. Preservar
equivalência exige auditar o consumo de RNG, além da seed efetivamente usada.
O RNG local de seleção de peças e esse consumo não devem ser confundidos.

## Separação proposta e ordem de implementação

1. Definir DungeonPlan com operações tipadas e valores próprios, identidade,
   seed, revisões e cobertura de leitura/escrita; nenhuma entidade viva.
2. Criar facade de planejamento que rejeite acessos indisponíveis e registre
   efeitos imediatos como intenções. Para remoções, modelar a ocupação prevista
   das entidades removidas nas consultas seguintes. Apenas omitir a remoção
   pode mudar decisões posteriores de colocação.
3. Executar esse planejador primeiro na thread do mundo contra snapshots,
   comparando ordem, escolhas e efeitos com o legado em mundos descartáveis.
   Não executar duas vezes callbacks no mundo usado como referência.
4. Separar reserva espacial de dependências globais: tiles por setor e revisão,
   entidades por identidade/ocupação, fios por topologia, template por revisão.
   Footprint incompleto recusa admissão; uma caixa geométrica não prova isolamento.
5. Publicar comandos em ordem legada pelo dono do mundo. NPC init, Lua,
   remoções, índices, rede e persistência permanecem nesse estágio inicial.
6. Mover somente planejamento auditado ao pool. Equivalência em jogo, falhas,
   unload e multiplayer precedem qualquer aplicação concorrente entre regiões.

## Limites e pendências específicas

Ainda falta auditar transitivamente factories, callbacks init/uninit, criação
de tesouros, todos os brushes e alterações do template; demonstrar cobertura
completa para mods arbitrários não decorre desta matriz. Persistência de plano
incompleto exige o protocolo de [retomada](dungeon-generation-resume-plan.md).
As métricas atuais são amostras parciais e não implementam reservas.

Não é necessário aguardar logs para implementar os contratos de plano e
snapshot. Logs e testes em jogo serão necessários para medir overhead e
comprovar que o novo planejamento conserva as decisões e os efeitos observáveis.

## Segunda passagem: factories, callbacks e template

Auditoria estática adicional sobre a mesma implementação. Os pontos abaixo
restringem a separação proposta; não dependem de nova coleta de logs.

| Caminho confirmado | Consequência para o plano |
| --- | --- |
| ObjectDatabase::createForPlacement → canPlaceObject → findValidOrientation | Valida placementValid e anchorsValid das orientações contra World antes de criar o objeto. A factory não é uma função apenas de JSON. |
| Object::init | Pode escolher orientação novamente, registra callbacks object/config/animator e inicializa o componente de script no Master. Preservar o estado observado nessa segunda validação. |
| LuaBaseComponent init/uninit | Invoca os entrypoints Lua init/uninit; LuaWorldComponent instala a API world. Um callback legado tem acesso além dos setores da estrutura. |
| Object::uninit | Desinicializa o script antes de Entity::uninit. A remoção feita durante escolha das peças tem efeitos observáveis por scripts. |
| Npc::init | Inicializa movimento/status/tools, equipa itens e inicializa script com callbacks de NPC, status, comportamento e movimento. Não transferir init inteiro a um worker. |
| Stagehand::init | No Master com m_scripted, inicializa script e callbacks de comportamento. Ausência de corpo físico não implica ausência de dependências. |
| TreasureDatabase::createTreasureChest | Lê threatLevel, escolhe definição e container por seed e cria objeto com treasurePools/treasureSeed. A geração do conteúdo ainda ocorre depois. |
| ContainerObject::init | Primeiro chama Object::init (incluindo script), depois, se ainda não inicializado, cria itens e preenche tesouro, finalizando com itemsUpdated. Pré-preencher baú antes do init pode mudar o que o script observa. |
| WorldTemplate::addCustomTerrainRegion/addCustomSpaceRegion | Acrescenta polígono, limpa m_blockCache inteiro e incrementa m_generationRevision. Dependência atual é global ao template, mesmo com polígono local. |

Fontes adicionais: [ObjectDatabase](../source/game/StarObjectDatabase.cpp),
[Object](../source/game/StarObject.cpp), [Npc](../source/game/StarNpc.cpp),
[Stagehand](../source/game/StarStagehand.cpp),
[componentes Lua](../source/game/scripting/StarLuaComponents.cpp),
[callbacks do mundo](../source/game/scripting/StarLuaComponents.hpp),
[Treasure](../source/game/StarTreasure.cpp),
[ContainerObject](../source/game/objects/StarContainerObject.cpp),
[WorldTemplate](../source/game/StarWorldTemplate.cpp).

### RNG e ordem observável dos baús

ContainerObject::init também avalia Random::randu64() no argumento de fallback,
mesmo com treasureSeed explícita. A seleção entre treasurePools usa
Random::randValueFrom; só depois fillWithTreasure recebe a seed incrementada.
Portanto, copiar apenas treasureSeed para um worker não estabelece equivalência
de seleção nem do consumo de RNG. O primeiro plano deve guardar a intenção de
criar o baú e deixar essa sequência no aplicador serial. Uma separação futura
de conteúdo exige capturar escolhas e preservar a ordem relativa ao init Lua.

### Fronteira recomendada após esta passagem

O piloto deve preparar geometria, buffers e descrições de entidades. O aplicador
continua responsável por orientação final, init/uninit, tesouros, alterações do
template e efeitos de rede. Para planejar após uma remoção com uninit arbitrário,
uma projeção de ocupação não basta: o script pode alterar outras dependências.
Esse caminho precisa declarar incompatibilidade com planejamento isolado ou
usar conteúdo cujo contrato de callbacks tenha sido auditado. Não repetir o
callback durante retry, nem executá-lo no mundo para descobrir seu alcance.

As passagens seguintes detalham placementValid/anchorsValid, os brushes
reconhecidos e o caminho inicial dos veículos. Continuam pendentes subclasses
de objetos, itens gerados por factories e a segurança concorrente dos caches
internos das databases. A existência de um lock em um cache não demonstra
segurança do restante da operação em um worker.

### Ancoragem e ocupação: dependências confirmadas

`ObjectOrientation::placementValid` examina cada `space` transladado para a
posição do objeto. Rejeita ocupação em foreground e proteção do tile.
`anchorsValid` examina cada âncora transladada: conectividade e, conforme a
definição, mod de solo arado, material de solo ou material específico. Quando
`anchorAny` está ativo, a escolha aceita ao menos uma âncora válida; caso
contrário, exige todas. `findValidOrientation` tenta primeiro orientações com
afinidade de direção e depois faz uma segunda passagem geral. A primeira
orientação válida determina o resultado.

Logo, o footprint de leitura de um objeto inclui a união dos `spaces` e das
âncoras de **todas as orientações que podem ser tentadas**, inclusive as
rejeitadas antes da escolhida. A revisão precisa cobrir proteção, ocupação de
entidades e tiles/mods nesses pontos. Uma revisão somente dos tiles escritos
pelo writer pode aprovar um plano cuja orientação já deixou de ser válida.
Fonte: [ObjectOrientation e ObjectConfig](../source/game/StarObjectDatabase.cpp).

## Terceira passagem: brushes, regras e veículos

`Part::canPlace` percorre tiles e regras até a primeira falha; `Tile::canPlace`
consulta o ID de dungeon antes das demais regras. `Part::place` percorre a
mesma peça **dez vezes**, na ordem Clear, Wall, Mods, Object, BiomeTrees,
BiomeItems, Wire, Item, Npc e DungeonId. Dentro de cada fase, os brushes do
tile rodam na ordem em que aparecem no asset. Um plano de operações não pode
simplesmente agrupar todos os brushes de um tile: isso mudaria a precedência
entre tiles de fases diferentes. A seleção dos conectores ainda usa o RNG
`m_rand` do gerador e as contagens de peças já colocadas.

| Brush ou entrada | Quando decide e o que alcança | Implicação |
| --- | --- | --- |
| Clear, Front, Back, Surface, SurfaceBackground | Preparam materiais/mods e, conforme colisão, limpeza de líquido; Surface consulta buffers vizinhos em `y+1` na fase Mods | Preservar ordem das fases e a visão dos buffers, inclusive na borda entre setores. |
| Liquid | Na fase Wall resolve nome do líquido, acumula posição; `flushLiquid` agrupa posições contíguas e calcula pressão | O conjunto de posições é local, mas a ordem de escrita no buffer final precisa de comparação. |
| Object, Vehicle, BiomeTree, BiomeItems, Item, Stagehand, DungeonId, Wire | Acumulam descrições ou posições; seus efeitos de mundo aparecem no `flush` | Não usar somente posições âncora como footprint completo de leitura/escrita. |
| PlayerStart | Chama `setPlayerStart` já na fase Npc, antes do `flush` | Efeito global imediato; substituir por intenção exige validar decisões subsequentes. |
| RandomBrush | `Random::randi64()` ao construir a definição; escolha por posição usa `staticRandomI32` dessa seed | Uma reconstrução privada da definição pode escolher brushes diferentes sem capturar a seed escolhida. |
| NpcBrush | Seed `stable` vira `Random::randu64()` na construção; espécies separadas por vírgula são escolhidas com `Random::randFrom` durante a fase Npc | O RNG global participa tanto da definição quanto da colocação. A seed efetiva do NPC não representa todo o consumo. |
| TMX Rectangle/Polyline | Rectangle expande uma área em tiles; Polyline emite cada vértice, usada também para fios | O footprint deve usar tiles emitidos pelo reader, não apenas a âncora ou o retângulo do objeto TMX. |

Os brushes reconhecidos pelo parser são explícitos em
[StarDungeonGenerator.cpp](../source/game/StarDungeonGenerator.cpp). O TMX
permite atribuir brushes e regras a Rectangle e Polyline; isso não cria uma
API C++ arbitrária de brush para mods, mas os assets podem combinar essas
operações e scripts das entidades resultantes continuam arbitrários. A análise
de concorrência deve considerar também o carregamento das definições: o cache
de `DungeonDefinitions` usa mutex, mas um brush de RNG global pode ser criado
quando a definição é reconstruída. A ordem e o instante dessa reconstrução
podem influenciar escolhas futuras.

`DungeonGeneratorWorld::placeVehicle` sinaliza o setor, cria um Vehicle com
configuração e o adiciona ao mundo. `VehicleDatabase::create` apenas busca a
configuração e constrói o objeto; não valida footprint nem segurança para
concorrência. `WorldServer::addEntity` executa `Vehicle::init`, que inicializa
movimento e Lua com callbacks de veículo, configuração, movimento e animação.
`Vehicle::uninit` executa o caminho correspondente. O campo `m_luaMutex` em
VehicleDatabase não demonstra que essa sequência seja segura fora do dono do
mundo; a criação e o init são operações distintas. Fontes:
[StarWorldGeneration.cpp](../source/game/StarWorldGeneration.cpp),
[StarVehicleDatabase.cpp](../source/game/StarVehicleDatabase.cpp) e
[StarVehicle.cpp](../source/game/StarVehicle.cpp).

### Critério para o primeiro DungeonPlan

O plano precisa registrar a **ordem de emissão por fase e peça**, a versão ou
identidade das definições, a seed local de conectores e as escolhas aleatórias
efetivas dos brushes. Para provar equivalência, comparar a sequência de
operações, não apenas o estado final dos tiles: callbacks e RNG podem observar
estados intermediários. Um worker só deve receber definições e escolhas já
congeladas, além de snapshots completos das consultas. Vehicle e outros
spawns permanecem intenções até a aplicação autoritativa.

## Quarta passagem: subclasses de objetos e itens

`ObjectDatabase::createObject` escolhe a subclasse pelo `type` da configuração:
`object`, `loungeable`, `container`, `farmable`, `teleporter` ou `physics`.
`getConfig` protege a busca e a carga do cache com `m_cacheMutex`, mas esse lock
não cobre o construtor, a escolha de orientação, `init` nem os scripts.
Consequentemente, preparar um `ObjectPtr` no worker não é uma fronteira segura
para o primeiro plano. Guardar nome, parâmetros, posição e direção; executar
`createForPlacement` e `addEntity` no aplicador.

| Subclasse | Dependência confirmada | Consequência para o plano |
| --- | --- | --- |
| `container` | `init` chama primeiro `Object::init`; depois lê `threatLevel`, cria `initialItems` e preenche `treasurePools`. `update` pode envelhecer itens por Lua e invocar `containerCallback`. | A ordem script → conteúdo e a autoridade sobre o inventário pertencem ao mundo. A descrição de um baú não deve conter um inventário pré-computado no piloto. |
| `farmable` | O construtor lê configuração de assets; `update` e `enterStage` consultam tempo, líquidos, mods e ocupação, podem modificar solo, criar planta e agendar `world()->timer`. | O objeto instalado tem dependências dinâmicas além do footprint de colocação. |
| `teleporter` | O construtor usa `configValue("uniqueId", Uuid().hex())`; `Uuid()` usa `Random::randBytes`. O argumento padrão é avaliado mesmo com `uniqueId` configurado. | Criar a entidade antecipadamente desloca o consumo do RNG global. O ID efetivo e a sequência observável devem ser preservados. |
| `physics` | `init` registra callbacks Lua de força/colisão, chama `Object::init` e calcula a caixa que inclui regiões de força; `uninit` remove callbacks. | A caixa relevante pode exceder os tiles da orientação; registro e remoção de callbacks ficam no aplicador. |
| `loungeable` | `setOrientationIndex` carrega posições de assento e estado de orientação; a interação pode devolver `SitDown` para uma entidade. | A orientação final e o ID da entidade definem âncoras/interação; não antecipar essa decisão. |

O `ItemBrush` grava apenas um `ItemDescriptor` no writer. Durante `flush`, os
drops vêm **depois** de NPCs, stagehands e fios, e antes de líquidos e IDs de
dungeon. A facade chama `ItemDrop::createRandomizedDrop(descriptor, position)`:
isso cria o item via `ItemDatabase::item`, lê `/itemdrop.config`, usa
`Random::randf()` para deslocamento/velocidade e adiciona a entidade ao mundo.
`ItemDatabase::item` chama `tryCreateItem`; `itemConfig` pode executar um script
Lua `builder` do asset. Em caso de erro, `tryCreateItem` pode passar pelos
handlers Lua do `Rebuilder` e produzir o item genérico de recuperação. Assim,
mesmo a factory de um drop pode executar código de mod e produzir efeitos
observáveis. O plano deve carregar o descriptor, sem materializar o item ou
sortear seu movimento no worker inicial.

O cache de protótipos de `ItemDatabase::itemShared` tem mutex, mas `item()`
chama diretamente `tryCreateItem`, sem usar esse cache. O builder Lua usa
`m_luaMutex` e o `Rebuilder` usa outro mutex próprio. Esses locks serializam
trechos específicos; não demonstram isolamento do RNG, dos assets, dos
callbacks ou da ordem de criação. Também não convém interpretar uma eventual
contenda nesses locks como ganho garantido ao adicionar workers.

### Próxima fronteira de auditoria

Antes de mover factories ao pool, analisar cada tipo de item e seus scripts de
construção, os handlers de recuperação de mods e a propriedade do RNG. Para o
primeiro `DungeonPlan`, basta serializar descrições imutáveis e aplicar
factories na sequência legada. Perfis de custo de `createForPlacement`,
`addEntity` e `ItemDatabase::item` são necessários para avaliar quanto tempo
restará na thread do mundo após paralelizar apenas a preparação.

Fontes: [ObjectDatabase](../source/game/StarObjectDatabase.cpp),
[subclasses](../source/game/objects), [ItemDatabase](../source/game/StarItemDatabase.cpp),
[ItemDrop](../source/game/StarItemDrop.cpp),
[Rebuilder](../source/game/StarRebuilder.cpp),
[Uuid](../source/core/StarUuid.cpp),
[writer](../source/game/StarDungeonGenerator.cpp) e
[facade](../source/game/StarWorldGeneration.cpp).

## Quinta passagem: microdungeons por setor

Esta passagem examina o caminho de microdungeons, separado do piloto de
dungeons compostas iniciado em `WorldServer::init`. Em
`WorldStorage::generateSectorToLevel`, o estágio `MicroDungeons` só começa
depois de levar os setores adjacentes ao estágio anterior. O prefetch em
segundo plano prepara apenas `BaseTiles`; não executa microdungeons. No
`WorldGenerator`, apenas `BaseTiles` usa `prepareTilesAsync`; o ramo
`MicroDungeons` chama `generateMicroDungeons` diretamente.

`generateMicroDungeons` percorre cada tile do setor 32×32, consulta itens
potenciais/válidos no `WorldTemplate` e inclui distribuições de placements
enfileirados. Depois ordena a fila e tenta cada microdungeon. A posição é
semeada por `worldTemplate()->seedFor`; o nome é escolhido por
`staticRandomFrom`. Para uma tentativa bem-sucedida, o gerador altera tiles
retornados, substitui blocos de bioma e pode satisfazer uma promise de
placement enfileirado. O estágio todo deve preservar essa sequência e o
estado `fulfilled` das solicitações; paralelizar apenas a chamada ao factory
não isola esses efeitos.

O `MicroDungeonFactory` tem `m_generating` e rejeita reentrada. Tenta o anchor
na altura original e em três tiles para cima e para baixo. O `bounds` passado
pela geração do setor é `sectorRegion.padded(WorldSectorSize - 1)`; no
factory, ele limita **somente o retângulo do anchor candidato**. Após escolher
o anchor, `DungeonGenerator::buildDungeon` expande conectores e peças sem
receber esse `bounds`. Logo, o retângulo acolchoado não é prova de footprint
máximo nem reserva suficiente para execução simultânea de setores vizinhos.

A fila de placements usa `sort` sobre pares `(BiomeItemPlacement,
QueuedPlacement*)`; `BiomeItemPlacement::operator<` compara apenas prioridade.
Quando prioridades empatam, o desempate do par alcança o ponteiro da entrada
enfileirada. Não tratar essa ordem como chave espacial estável em um novo
agendador. Primeiro registrar a ordem efetiva e definir desempate explícito
sob uma regra de compatibilidade, sem alterar silenciosamente a geração
legada.

Para um futuro piloto de microdungeons, a unidade de reserva precisa ser a
**estrutura completa produzida**, incluindo leituras de candidatos rejeitados,
peças expandidas, efeitos do writer/facade e alterações de bioma após retorno.
Uma primeira versão pode usar planejamento serial contra snapshot e recusar
planos cujo alcance exceda a cobertura; o factory atual não deve ser invocado
em paralelo como objeto compartilhado.

Fontes: [WorldStorage](../source/game/StarWorldStorage.cpp),
[WorldGeneration](../source/game/StarWorldGeneration.cpp),
[MicroDungeonFactory](../source/game/StarMicroDungeon.cpp),
[BiomePlacement](../source/game/StarBiomePlacement.cpp),
[StarAlgorithm](../source/core/StarAlgorithm.hpp).

## Sexta passagem: líquidos, finalização e revisões de setor

Depois de `MicroDungeons`, `WorldStorage::generateSectorToLevel` exige os
vizinhos no estágio anterior antes de avançar para `CaveLiquid` e `Finalize`.
Essa dependência é recursiva e conserva uma borda preparada; não equivale a
permitir escrita concorrente nesses vizinhos.

`generateCaveLiquid` calcula a partir de seeds do setor e percorre tiles numa
caixa acolchoada por `WorldSectorSize - 1` em cada direção. Lê foreground,
background, líquidos e IDs de dungeon, com wrap horizontal, e pode gravar
líquido e background **fora do setor solicitado**. A geometria dos candidatos
depende dos tiles vivos dentro da caixa; reservar só o setor de origem é
insuficiente. O estágio seguinte ativa a região no motor de líquidos, outro
efeito que precisa continuar com a autoridade do mundo.

`prepareSector` atualiza os tiles do setor e faz consultas de vizinhos ao
colocar grass. Em seguida, seleciona itens de bioma e chama `placePlant` ou
`createForPlacement`/`addEntity`. `placePlant` consulta tiles, plantas já
instaladas, spaces e roots da planta; pode **modificar tiles vizinhos** antes
de instalar a entidade. A borda de leitura/escrita depende da variante da
planta, não apenas da dimensão fixa do setor. O caminho de floating dungeon
usa `prepareSectorBiomeBlocks`, com menos efeitos, mas ainda consulta tiles
vizinhos no cálculo de grass.

`TileSectorArray::modifyTile` incrementa a revisão do setor físico quando
retorna um ponteiro válido, **antes de saber se o chamador mudou algum campo**.
Assim, `sectorRevision` é um indicador conservador de acesso mutável, não uma
prova de alteração semântica. `prepareTilesAsync` exige igualdade dessa
revisão ao publicar um resultado: acessos mutáveis sem alteração real podem
descartar preparação válida. Uma revisão semântica mais precisa poderia
reduzir rejeições, mas só depois de auditar todos os escritores e sem perder
invalidação por mudanças de tiles, template, entidades ou estado de geração.

Fontes: [WorldGeneration](../source/game/StarWorldGeneration.cpp),
[WorldStorage](../source/game/StarWorldStorage.cpp) e
[TileSectorArray](../source/game/StarTileSectorArray.hpp).

## Sétima passagem: snapshot de terreno base e RNG compartilhado

O caminho assíncrono de `BaseTiles` copia o array do setor e serializa o
`WorldTemplate` quando a identidade ou `generationRevision` muda. O worker
reconstrói um template por thread, guardado em cache `thread_local` com chave
de identidade/revisão, e calcula os tiles nessa cópia. Na publicação, o dono
do mundo verifica tarefa/época, ponteiro e revisão dos tiles de origem,
identidade e revisão do template. Esse é um padrão útil para o plano regional:
snapshot isolado, validação antes de publicar e retenção da reserva até o
producer terminar.

Há uma dependência de RNG ainda não isolada. `WorldTemplate(Json const& store)`
chama o construtor padrão antes de substituir a seed pelo valor armazenado.
O construtor padrão executa `Random::randu64()`. `Random` usa uma fonte global
protegida por mutex: isso evita acesso simultâneo sem lock, mas cada
reconstrução do template no worker **consome uma amostra da sequência global**.
O número e a ordem dessas reconstruções dependem de worker, cache e revisões;
logo, a preparação de terreno pode alterar escolhas aleatórias posteriores
mesmo quando o resultado dos tiles é determinístico. Uma correção específica
deve construir o template desserializado sem sortear seed transitória,
preservando a seed efetiva do store; antes de aplicá-la, avaliar se alguma
sequência legada depende inadvertidamente desse consumo.

O `WorldLayout(Json)` e `TerrainDatabase::loadSelector` recebem seeds
serializadas no trecho examinado; esta passagem **não prova** ausência de
outros efeitos durante a reconstrução de todo o grafo de assets. Para o
`DungeonPlan`, não compartilhar o `WorldTemplate` vivo com workers e não
assumir que desserialização é pura sem essa auditoria transitiva.

Fontes: [WorldGeneration](../source/game/StarWorldGeneration.cpp),
[WorldTemplate](../source/game/StarWorldTemplate.cpp),
[WorldLayout](../source/game/StarWorldLayout.cpp),
[TerrainDatabase](../source/game/StarTerrainDatabase.cpp) e
[Random](../source/core/StarRandom.cpp).

## Oitava passagem: alcance real das reservas existentes

`WorldGenerationTasks::reserve` usa a chave `(sector, stage)`, recusa tarefa
duplicada e mantém a reserva de bytes até `retire` após conclusão do producer.
O enum de estágios e a própria admissão aceitam **somente `BaseTiles`**. A
identidade inclui época do mundo, sequência, setor, região e estágio, mas
`region` serve como metadado: o registro não bloqueia outra tarefa em setor
vizinho nem cobre o footprint de uma dungeon ou do líquido de caverna.

`WorldRegionCoordinator` modela turnos exclusivos por região com snapshots de
metadados e lanes lógicas. Seu comentário e API delimitam um scheduler
**shadow**, sem execução de callbacks ou publicação de tiles. Aceitar um turno
após comparar o snapshot não constitui reserva física de tiles, entidades,
template, fios ou RNG. O próximo mecanismo de admissão deve manter o contrato
de não duplicar tarefas por identidade e acrescentar conflitos pelo conjunto
completo de leitura/escrita, inclusive cruzamentos de regiões e wrap.

Fontes: [WorldGenerationTasks](../source/game/StarWorldGenerationTasks.cpp),
[WorldRegionCoordinator](../source/game/StarWorldRegionCoordinator.cpp) e
[WorldRegionPartition](../source/game/StarWorldRegionPartition.cpp).

## Nona passagem: persistência, reload e estágios parciais

`WorldStorage::generateSectorToLevel` atualiza `metadata.generationLevel`
depois de cada estágio concluído e marca o setor sujo. `syncSector` grava
tiles com esse nível quando a revisão mudou; entidades persistentes são
serializadas por setor quando o nível de carga inclui entidades. `sync` passa
pelos setores carregados e só então faz `m_db.commit`. No reload, o tile
sector restaura o nível de geração e os dados de entidade são criados,
inicializados e indexados em uma etapa de carga separada.

Esse formato permite retomar **estágios inteiros** pelo nível salvo, mas não
codifica uma dungeon em planejamento, parcialmente aplicada ou aguardando
commit. O futuro plano não pode publicar metade das operações e marcar o
estágio como completo. Também não deve inferir atomicidade de uma estrutura
composta apenas porque o banco comita setores no final: callbacks, entidades
ativas e pacotes já podem observar efeitos em memória antes do sync. A
proposta de intenção durável e recuperação de
[dungeon-generation-resume-plan.md](dungeon-generation-resume-plan.md)
continua necessária antes de aplicação interrompível.

No unload, entidades persistentes são serializadas pelo setor de sua
**posição**, enquanto uma entidade que só o sobrepõe pertence a outro setor.
Um footprint regional deve distinguir ocupação, setor dono e identidade;
reservar todos os setores tocados não muda automaticamente a propriedade da
entidade nem seu índice de IDs únicos.

Fontes: [WorldStorage](../source/game/StarWorldStorage.cpp) e
[WorldServer](../source/game/StarWorldServer.cpp).

## Décima passagem: publicação observável pelo cliente

`WorldStorage::sectorActive` exige carga `Loaded` e geração `Complete`.
`WorldServer` mantém setores pendentes por cliente e só envia o pacote completo
de tiles quando o setor está ativo. Depois envia atualizações pendentes de
tile, dano e líquido, e consulta entidades monitoradas para produzir os
pacotes correspondentes. Portanto, a fronteira de ativação já impede que o
cliente receba um setor base incompleto como pacote inicial.

Isso **não** cria uma publicação atômica para uma dungeon que cruza setores:
os setores podem completar em passos distintos, e `addEntity`/`modifyTile`
atuam no mundo vivo antes da formação dos pacotes. Um commit regional precisa
definir o ponto em que todas as partes e entidades passam a ser observáveis,
além de preservar a ordem de IDs, `init`, índices e atualizações de rede.
Conferir apenas o pacote inicial de tiles seria insuficiente para provar
integridade visual ou mecânica em multiplayer.

Fontes: [WorldServer](../source/game/StarWorldServer.cpp) e
[WorldStorage](../source/game/StarWorldStorage.cpp).

## Décima primeira passagem: fila visível e prioridade espacial

`WorldServer::update` entrega à fila um orçamento de tempo e limite opcional
de estágios por passo. `WorldStorage::generateQueue` ordena os setores
solicitados, publica resultados `BaseTiles` prontos, tenta completar o
primeiro setor e, quando ele bloqueia, só agenda/prepara `BaseTiles` dos
demais nessa passagem. Os estágios de microdungeon, líquido e finalização
continuam serializados nessa ordem. Prefetch usa apenas o tempo que sobra
**depois** de esvaziar a fila visível; a quantidade de workers por si só não
garante avanço de dungeons quando a fila visível permanece cheia.

A ordenação calcula `vmag(sectorCenter - player->position())` para o jogador
mais próximo. Essa subtração é plana; não usa `WorldGeometry::diff`, embora o
mundo faça wrap horizontal. Junto à emenda do planeta, um setor fisicamente
próximo pode ser ordenado como distante. Isso é uma inconsistência concreta
da prioridade, não prova de que seja a causa dos atrasos observados nos logs.
Uma correção isolada deve usar distância com wrap e comparar a ordem em
cenários próximos à emenda antes de integrar prioridades por velocidade.

O prefetch usa `worldStoragePrefetchPad` limitado a 32 tiles e só é ativado
quando `sectorWorkers > 0`; ele prepara `BaseTiles`, sem entidades nem
microdungeons. Um plano de pré-geração direcional precisa manter essa
distinção e evitar anunciar um setor como pronto antes de `Complete`.

Fontes: [WorldServer](../source/game/StarWorldServer.cpp),
[WorldStorage](../source/game/StarWorldStorage.cpp) e
[WorldGeometry](../source/base/StarWorldGeometry.hpp).

## Décima segunda passagem: escrita transfronteira e avisos de rede

O cálculo de `CaveLiquid` grava pelo `tileArray->modifyTile` em posições da
caixa acolchoada, inclusive em setores vizinhos já carregados. Esse helper
marca a revisão do setor físico atingido, mas não executa os efeitos de
`WorldServer::modifyServerTile`: sujar colisão, visitar o motor de líquidos e
enfileirar atualização de tile para clientes. `generateSectorLevel(Finalize)`
ativa a região **do setor solicitado**, não toda a caixa escrita por
`CaveLiquid`.

No `WorldServer::update`, `generateQueue` roda antes de
`queueUpdatePackets`. A ausência de enfileiramento no write direto não é
compensada automaticamente pela montagem posterior dos pacotes: para um
setor que já recebeu o pacote inicial, o código de rede envia apenas as
posições presentes nas filas de atualização, salvo outro evento que as inclua.

Isso cria uma condição que deve ser investigada com rastreamento de sequência:
se um setor vizinho já estiver `Complete` e monitorado quando outro setor lhe
alterar líquido/background, o caminho de escrita examinado não agenda por si
um pacote de tile para esse vizinho. O código não prova que o cliente ficará
permanentemente divergente, pois outros sistemas podem visitar ou atualizar
o mesmo tile depois. Um modelo regional deve declarar todas as posições
escritas e publicar os avisos/ativações para **cada setor realmente alterado**.

`WorldServer::placeDungeon` também altera `m_generatingDungeon` e
`m_tileProtectionEnabled` antes de chamar o gerador, mas os restaura somente
no retorno normal. Uma exceção do gerador pode deixar esses flags alterados
durante o unwinding. Esse caminho é acessível por Lua e comando; qualquer
refatoração de execução ou retry precisa restaurar estado com escopo de
exceção, sem presumir que falhas sempre encerram o mundo imediatamente.

Fontes: [WorldGeneration](../source/game/StarWorldGeneration.cpp),
[WorldServer](../source/game/StarWorldServer.cpp),
[TileSectorArray](../source/game/StarTileSectorArray.hpp) e
[WorldLuaBindings](../source/game/scripting/StarWorldLuaBindings.cpp).

## Décima terceira passagem: profundidade dos pré-requisitos

`SectorGenerationLevel::Complete` é `Finalize` (nível 4). Para executar um
estágio acima de `BaseTiles`, `generateSectorToLevel` exige o nível anterior
do próprio setor e de `adjacentSectors`, que cobre os setores da região
acolchoada em 32 tiles. Assim, completar um setor central pode depender de
`CaveLiquid` numa vizinhança 3×3, `MicroDungeons` numa 5×5 e `BaseTiles`
numa 7×7, até **49 setores base** longe das bordas do mundo. Não são 49
jobs necessariamente novos: níveis já satisfeitos retornam imediatamente.

Essa expansão explica por que a solicitação visível de um único setor pode
alcançar muito mais trabalho e por que o limite de estágios por passo pode
interromper a cadeia antes de tornar o setor ativo. O grafo observacional
atual capta até oito raízes e 256 nós; quando trunca, sua fronteira não
representa todos os pré-requisitos. Para o novo scheduler, a admissão deve
calcular a cadeia completa ou tratar explicitamente a parte ainda não
resolvida, sem usar uma fronteira truncada como autorização de execução.

Fontes: [WorldStorage](../source/game/StarWorldStorage.cpp),
[WorldGenerationDependency](../source/game/StarWorldGenerationDependency.cpp)
e [WorldStorage.hpp](../source/game/StarWorldStorage.hpp).

## Décima quarta passagem: limite local versus pool global

`worldSectorWorkerPool()` é um singleton do processo, configurado com até
oito workers. Já `m_pendingBaseTiles` e `WorldGenerationTasks` pertencem a
cada `WorldGenerator`/mundo. Cada mundo admite até `2 × workers` preparações
pendentes e reserva, contabilmente, um `sizeof(SectorArray)` por tarefa. O
`WorkerPool::queueWork` acrescenta trabalhos à fila compartilhada sem um
limite global próprio. Em servidores com vários mundos ativos, a soma das
admissões pode exceder o teto de 16 resultados citado para **um** mundo e
elevar o tempo de espera dos jobs, embora só existam até oito threads desse
pool.

O cálculo de bytes reservado cobre o array de tiles do resultado; não inclui
o JSON de `WorldTemplate::store`, o template reconstruído por worker, caches
do template, promise ou overhead da fila. Não tratar `reservedBytes` como
medida exata da memória da geração. Para expandir o pool a microdungeons ou
planos regionais, é preciso orçamento global de jobs/bytes e prioridade entre
mundos, além do limite local e da métrica de tempo na fila.

Fontes: [WorldGeneration](../source/game/StarWorldGeneration.cpp),
[WorldGenerationTasks](../source/game/StarWorldGenerationTasks.cpp) e
[WorkerPool](../source/core/StarWorkerPool.cpp).

## Décima quinta passagem: coordenadas de líquido na emenda do planeta

`generateCaveLiquid` mantém `candidateNodes` e `solidSurroundings` nas
coordenadas da caixa acolchoada, que podem ter `x < 0` ou `x >= width` perto
da emenda horizontal. Apenas o acesso ao array de tiles usa `wrapCoords`.
Depois, `determineLiquidLevel` copia esses candidatos para `openSet`, mas
normaliza **cada vizinho** com `geometry.xwrap` antes de procurá-lo no set.
Exemplo: com um candidato em `x = -1` e outro em `x = 0`, o vizinho esquerdo
de `0` é procurado como `width - 1`, enquanto o set contém `-1`. Os dois
tiles fisicamente adjacentes podem ser separados em clusters diferentes.
`levelCluster` ainda consulta `filled` usando coordenadas cruas.

O efeito visual e a frequência desse caso dependem de haver seeds, cavidades
e líquido elegíveis perto da emenda; esta auditoria estática não os mede.
Antes de paralelizar o estágio, escolher uma representação única para as
coordenadas dos sets e preservar os limites da caixa de busca em espaço
desenrolado. Comparar pressão/volume resultantes nas duas bordas do planeta
com e sem a correção.

Fontes: [WorldGeneration](../source/game/StarWorldGeneration.cpp) e
[WorldGeometry](../source/base/StarWorldGeometry.hpp).

## Décima sexta passagem: colocação de plantas e falha parcial

`WorldGenerator::placePlant` lê spaces, roots, ocupação de plantas, materiais
e IDs de dungeon antes de instalar a planta. Depois altera diretamente
tiles dos spaces próximos e dos roots; só no fim chama
`WorldServer::addEntity`. Se `modifyTile` devolver `nullptr` após alguma
alteração anterior, a função retorna `false` **sem restaurar os tiles já
alterados**. A condição depende de footprint da variante além dos setores
carregados ou dos limites do mundo; não foi demonstrada sua frequência em
assets reais.

`addEntity` chama `updateTileEntityTiles`, que pode enfileirar avisos para
material spaces quando modifica tile ou colisão. Isso não equivale a publicar
automaticamente todas as alterações anteriores de `placePlant`, como
background ou cópia de tile em spaces, e não resolve o retorno parcial antes
de instalar a entidade. Um plano regional precisa preparar e validar o
footprint **inteiro** antes da primeira escrita, inclusive resultado da
factory da planta; a aplicação deve ter caminho definido para falha de
instalação sem deixar tiles órfãos.

Há uma segunda implementação, `DungeonGeneratorWorld::placePlant`, usada por
árvores/itens de bioma da facade de dungeon. Ela chama
`WorldServer::modifyServerTile` em cada space/root, o que enfileira avisos e
suja colisão, mas também pode retornar depois de escritas parciais. As duas
implementações **não são equivalentes**: no ajuste de space, a de setor fixa
`CollisionKind::None`, enquanto a de dungeon termina com a colisão do
material foreground. O rework não deve fundi-las antes de decidir qual
comportamento é esperado e comparar estruturas/plantas geradas nos dois
caminhos.

Fontes: [WorldGeneration](../source/game/StarWorldGeneration.cpp),
[WorldServer](../source/game/StarWorldServer.cpp) e
[TileSectorArray](../source/game/StarTileSectorArray.hpp).

## Encaminhamento após as passagens estáticas

1. Investigar primeiro a escrita de `CaveLiquid` em vizinho já ativo, a
   separação de clusters junto à emenda horizontal e falhas parciais de
   `placePlant`. Registrar sequência de geração, revisões, visitas do motor
   de líquidos e pacotes de rede; são riscos de consistência observável.
2. Isolar o sorteio transitório na desserialização de `WorldTemplate`, sob
   revisão de equivalência da sequência de RNG.
3. Corrigir a prioridade espacial na emenda horizontal com `WorldGeometry`,
   mantendo os estágios e o orçamento atuais como controle da comparação.
4. Definir orçamento global para o pool antes de admitir planos de dungeons
   ou microdungeons de múltiplos mundos.
5. Só então pilotar `DungeonPlan` com footprint completo, snapshot, checagem
   de conflitos, aplicação ordenada e protocolo de retomada. A coordenação
   regional observacional e os locks de caches não satisfazem esses contratos.
