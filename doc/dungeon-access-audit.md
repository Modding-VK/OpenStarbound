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

Pendências mais específicas: expandir placementValid/anchorsValid, subclasses
de objetos e veículos, itens gerados por factories e chamadas indiretas dos
brushes. Caches internos e locks das databases ainda precisam de auditoria de
concorrência; a existência de cache não prova segurança de acesso em worker.
