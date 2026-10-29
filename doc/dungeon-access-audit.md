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
