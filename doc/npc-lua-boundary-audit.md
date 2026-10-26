# Auditoria da fronteira Lua dos NPCs

Estado: inspeção estática da engine em 26/10/2026. Complementa o
[modelo conceitual](npc-externalization-concept.md) e o
[plano por fases](npc-ai-rework-action-plan.md). Não é uma certificação dos
scripts de todos os mods: o conteúdo vanilla completo e cada variante do
Frackin' Universe precisam de inventário de assets e coleta próprios antes
de migrar um tipo de NPC.

## Entradas de Lua fora do callback periódico

| Entrada | Contrato observado | Consequência para Actor v1 |
| --- | --- | --- |
| `Npc::init` / `uninit` | Inicializa/desinicializa VM, bindings de mundo, NPC, status, comportamento, songbook, animator e movimento ([StarNpc.cpp](../source/game/StarNpc.cpp)) | A VM isolada precisa ter ciclo de vida próprio; não pode compartilhar callbacks que guardam ponteiros mutáveis para esses objetos. |
| `Npc::update` | Executa `m_scriptComponent.update`, depois âncora, efeitos, movimento, status e estado compartilhado ([StarNpc.cpp](../source/game/StarNpc.cpp)) | Extrair IA não remove o tick autoritativo de status, ferramentas, efeitos, animação e publicação espacial. |
| Dano / morte | `applyDamage` invoca `damage`; `shouldDestroy` invoca `shouldDie`; `destroy` aciona ações de morte ([StarNpc.cpp](../source/game/StarNpc.cpp)) | Eventos de dano e morte devem ter ordem causal; a decisão síncrona de destruição não pode esperar worker. |
| Interação de jogador | `Npc::interact` devolve `InteractAction` imediatamente ([StarNpc.cpp](../source/game/StarNpc.cpp), [StarWorldServer.cpp](../source/game/StarWorldServer.cpp)) | Diálogo/quest Actor v1 precisa de descritor confirmado no proxy, ou o tipo permanece legado. |
| Mensagem / chamada de script | `receiveMessage` e `callScript` entram na VM de NPC fora do update ([StarNpc.cpp](../source/game/StarNpc.cpp)) | Mover somente `update` criaria acesso concorrente à VM. A fachada síncrona de leitura exige allowlist e dados confirmados. |

O asset `.npctype` fornece scripts, `scriptConfig` e `initialScriptDelta`
([StarNpcDatabase.cpp](../source/game/StarNpcDatabase.cpp)); o save contém
`scriptStorage`, movimento, status e itens. O modo novo deve ser opt-in e
versionado por tipo/instância. `clientEntityMode` configurável também exige
comprovar autoridade no servidor antes de admitir a instância.

## Bindings e chamadas cruzadas

| Interface existente | Semântica observada | Classificação inicial |
| --- | --- | --- |
| `world` no script NPC | `LuaWorldComponent::init` injeta os callbacks do mundo vivo e callbacks de thread separados ([StarLuaComponents.hpp](../source/game/scripting/StarLuaComponents.hpp)) | Mundo vivo apenas no legado; Actor v1 precisa de `NpcView` limitada. |
| `world.entityQuery` | Opção `callScript` executa método do alvo durante o filtro; `order=random` usa RNG ([StarWorldLuaBindings.cpp](../source/game/scripting/StarWorldLuaBindings.cpp)) | Query geométrica auditada pode usar snapshot; filtros scriptados e ordem aleatória requerem contrato novo. |
| `world.callScriptedEntity` | Chama método do alvo mestre e devolve resultado síncrono ([StarWorldLuaBindings.cpp](../source/game/scripting/StarWorldLuaBindings.cpp), [world.md](lua/world.md)) | Não transformar em promise implicitamente. Um NPC Actor v1 alvo só expõe fachada síncrona declarada e sem efeitos. |
| `world.sendEntityMessage` | API retorna promise, mas um alvo mestre pode processar a mensagem imediatamente no servidor ([StarWorldLuaBindings.cpp](../source/game/scripting/StarWorldLuaBindings.cpp), [StarWorldServer.cpp](../source/game/StarWorldServer.cpp)) | Actor v1 exige inbox e evento de resultado; scripts que dependem da resolução no mesmo stack ficam legados. |
| `mcontroller.controlPathMove` | Retorna `nil` enquanto busca/movimento está pendente e `bool` ao concluir ([StarLuaActorMovementComponent.hpp](../source/game/scripting/StarLuaActorMovementComponent.hpp)) | Adaptador pode preservar tipo de retorno, mas deve medir mudança de latência; busca pura pode usar snapshot. |
| `PathController::validateEdge` | Consulta Lua de porta (`hasCapability`) e envia `openDoor` ([StarActorMovementController.cpp](../source/game/StarActorMovementController.cpp)) | Nunca executar no worker sombra. No runtime novo, atravessar só após pós-condição observável de colisão livre. |
| `npc.setLounging` e ações de âncora | Consulta entidade/âncora e devolve resultado imediato ([StarNpc.cpp](../source/game/StarNpc.cpp)) | Exige adaptador síncrono mundo-side ou permanece legado. |
| Bindings de status, ferramentas, songbook, comportamento e animator | Instalados com referências a componentes do NPC na inicialização ([StarNpc.cpp](../source/game/StarNpc.cpp)) | Auditoria método por método antes de oferecer uma capacidade Actor v1; os objetos não são copiados para worker. |

O `ScriptableThread` existente expõe apenas um conjunto restrito de callbacks
seguros; não equivale ao `world.*` dos NPCs
([StarWorldLuaBindings.cpp](../source/game/scripting/StarWorldLuaBindings.cpp)).
Também há visibilidade dentro do tick: `EntityMap::updateAllEntities` atualiza
o índice espacial depois de cada entidade. O caminho legado preserva essa
ordem; Actor v1 precisa declarar sua fronteira causal.

## Inventário exigido antes de admitir um tipo Actor v1

1. Registrar scripts, hooks (`init`, `update`, `uninit`, `damage`, `shouldDie`,
   `onInteraction`, mensagens), funções exportadas e `clientEntityMode` do
   `.npctype` e seus overrides.
2. Classificar cada acesso `world.*`, `npc.*`, `status.*`, `mcontroller.*` e a
   cada componente adicional como leitura pura, leitura com callback, mutação,
   resultado síncrono ou promise. Uma função de aparência read-only pode
   executar Lua de outra entidade.
3. Registrar `world.callScriptedEntity`/filtros `callScript` **dirigidos ao NPC**
   por outros scripts; o contrato do alvo importa tanto quanto o do chamador.
4. Definir equivalência e versão de cada ação vanilla reutilizada: leitura,
   efeitos, retorno, falha, cancelamento e persistência. Callbacks sem
   equivalência continuam legados.
5. Documentar o asset e o save de origem, cenários de mod e multiplayer e
   custo medido. O custo de ITDs não substitui o baseline de NPCs.

O shadow mode deste recorte observa apenas **pathfinding puro**. Não carrega Lua
no worker nem ativa automaticamente um tipo de NPC para Actor v1.
