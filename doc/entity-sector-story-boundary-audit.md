# Auditoria de monstros, veículos, scripts de locais e bordas de setores

Estado: auditoria estática da engine na branch `Experimental`, em 29/10/2026.
Complementa o [modelo regional](world-region-rework-advanced-concept.md), a
[fronteira Lua dos NPCs](npc-lua-boundary-audit.md) e o
[diagnóstico de missões](story-script-profiling.md). Não altera execução ou
formato de save. Sem compilação ou reprodução em jogo nesta auditoria.

## 1. Tipos, autoridade e medição atual

`Npc`, `Monster` e `Vehicle` são tipos separados em
[`EntityType`](../source/game/interfaces/StarEntity.hpp). `Stagehand` também é
um tipo próprio. Todos entram no update de entidades de `WorldServer`, ordenado
por `EntityType`; `EntityMap::updateAllEntities` atualiza o índice espacial após
**cada** callback, de modo que a entidade seguinte pode observar a posição
nova. Mover só um tipo para outra thread mudaria essa visibilidade.

O `ExplorationProfile::EntityScope` já mede o update por tipo, incluindo Lua
atribuída ao update. Com `regionalDetails`, atribui custo a bins lógicos de
64 tiles. Isso não mede todos os callbacks externos ao update. O HUD agrupa
monstros, veículos e stagehands em `Other entities`; só NPC e objeto possuem
fatias próprias e contadores de update lento. A ponte Actor atual captura
somente NPCs, nunca esses outros tipos.

| Tipo | Autoridade e trabalho observado | Fronteiras que impedem migração automática |
| --- | --- | --- |
| Monstro | `clientEntityMode` vem da variante; `init` instala Lua e bindings apenas no `Master`. No update mestre, o script precede movimento, status e efeitos; no escravo há interpolação. | `applyDamage` invoca `damage`; `shouldDie`, `die`, `interact`, `receiveMessage`, `callScript` e `evalScript` entram em Lua fora do update. A morte pode criar drops no mundo. Script e movimento/status compartilham estado. |
| Veículo | `clientEntityMode` vem do config, com padrão `ClientSlaveOnly`; `Master` executa movimento e Lua, `Slave` interpola e envia controle/aim por `sendEntityMessage`. | `receiveMessage` altera controles e estado dos passageiros antes de encaminhar mensagens genéricas a Lua. Dano, interação, `callScript` e `evalScript` são síncronos. Âncoras e posições dos ocupantes dependem da posição do veículo. |
| Stagehand | Pode ser scriptado no `Master`, manter `scriptStorage` e ter `broadcastArea` própria. | Scripts de local podem receber mensagens, chamar métodos e executar Lua durante o update. A área de broadcast pode cobrir mais de um setor; sua posição não limita o footprint de efeitos. |

Evidência: [`StarMonster.cpp`](../source/game/StarMonster.cpp),
[`StarVehicle.cpp`](../source/game/StarVehicle.cpp),
[`StarStagehand.cpp`](../source/game/StarStagehand.cpp),
[`StarWorldServer.cpp`](../source/game/StarWorldServer.cpp) e
[`StarEntityMap.cpp`](../source/game/StarEntityMap.cpp).

**Decisão:** não admitir monstros, veículos ou stagehands na futura thread de
NPCs pelo nome ou pela presença de Lua. Uma capacidade Actor v1 pode fornecer
primitivas reutilizáveis de percepção, mensagens e snapshots, mas cada tipo
precisa de contrato próprio de autoridade, callbacks, física, save e fallback.
Uma entidade `ClientMasterAllowed` ou `ClientPresenceMaster` não pode ser
transferida implicitamente para um serviço do host.

## 2. Bordas físicas e ciclo de vida de setores

`WorldStorage` atribui uma entidade ao setor que contém sua **posição** para
descarregamento e persistência. Já `EntityMap` indexa seu `metaBoundBox`
espacial, que pode se sobrepor a vários setores e à emenda horizontal do
planeta. Ao descarregar, `WorldStorage` examina entidades sobrepostas e pode
reter o setor se o setor dono ainda tiver TTL; entidades `keepAlive` também
adiam unload. Entidades persistentes `Master` são serializadas; entidades
`Slave` mantêm o setor vivo. Um veículo comprido ou um monstro na borda não
pode ter sua posição, colisão, passageiros e save publicados separadamente.

O `WorldServer` atual percorre entidades, contextos Lua, dano, fios, líquidos e
armazenamento em fases ordenadas. `WireProcessor::process` varre entidades de
fio e `loadNetwork` pode carregar sincronamente setores conectados antes de
avaliar a rede; a rede, e não o setor inicial, é o footprint causal. O
`LiquidCellEngine::update` usa conjuntos globais de células ativas e fases de
pressão, movimento e interação; `WorldServer` chama o update e recebe os
efeitos no mundo. Logo, particionar água somente pelo índice do setor sem
definir fluxos e interações de borda mudaria a física. Dano, projéteis e
consultas de colisão também podem alcançar setores vizinhos. O
`WorldServer::signalRegion` ativa todos os setores da região solicitada; o
callback de dungeon segue regra especial para carga sem geração imediata.

Evidência: [`StarWorldStorage.cpp`](../source/game/StarWorldStorage.cpp),
[`StarWorldGeneration.cpp`](../source/game/StarWorldGeneration.cpp),
[`StarWireProcessor.cpp`](../source/game/StarWireProcessor.cpp) e
[`StarCellularLiquid.hpp`](../source/base/StarCellularLiquid.hpp).

**Contrato mínimo antes da simulação setorial:** definir dono por entidade e
geração de ID; capturar footprint de leitura/escrita; usar snapshots de
vizinhos com revisão; confirmar uma transferência exatamente uma vez; validar
efeitos multissetor antes de publicá-los; e impedir unload enquanto uma ação
confirmável ou persistência do dono estiver pendente. Preservar a ordem
observável do legado ou versionar explicitamente o novo contrato. Fios e
líquidos exigem protocolos de rede/borda próprios.

## 3. Scripts de missões e locais

`QuestManager` pertence ao jogador. `Quest::update` executa Lua; mensagens,
oferta, início e conclusão também podem entrar em Lua fora desse update; a
conclusão ainda entrega recompensas e pode oferecer a próxima quest.
`Quest::initScript` fornece callbacks para quest, player,
celestial, status e movimento. O perfil `quest.update` é **inclusivo** dos
updates de quests; `quest.offer`/`quest.message` medem apenas essas entradas,
não todo callback como `questComplete` chamado por outro caminho.

Cada mundo, inclusive uma instância de missão, tem um `WorldServerThread`
próprio. Seus contextos Lua, entidades scriptadas e mensagens compartilham o
estado daquele mundo. No update atual do mundo, entidades são atualizadas
antes dos contextos Lua globais do mundo; um contexto pode assim observar
efeitos de entidades do mesmo passo. `UniverseServer` cria instâncias via
`instance_worlds.config`, aplica ações de flags e encaminha mensagens/viagens;
seus próprios contextos Lua usam `m_luaLock`. `WorldServer::sendEntityMessage`
chama `receiveMessage` **imediatamente** para alvo `Master` local, mas
encaminha ao cliente e retorna uma promise para alvo não mestre. A mesma API
pode, portanto, observar tempos e donos diferentes. `Stagehand`, NPC, objeto,
monstro e veículo podem ser alvos scriptados de eventos de local.

Os assets disponíveis neste checkout são principalmente patches OpenStarbound;
o conjunto completo de scripts vanilla das missões e do Outpost não está
presente como fonte legível aqui. Esta auditoria classifica **caminhos da
engine**, não certifica a ordem ou o conteúdo de cada missão concreta ou mod.

Evidência: [`StarQuests.cpp`](../source/game/StarQuests.cpp),
[`StarQuestManager.cpp`](../source/game/StarQuestManager.cpp),
[`StarWorldServerThread.cpp`](../source/game/StarWorldServerThread.cpp),
[`StarWorldServer.cpp`](../source/game/StarWorldServer.cpp) e
[`StarUniverseServer.cpp`](../source/game/StarUniverseServer.cpp).

**Decisão:** manter o caminho serial para scripts legados de história e
instâncias. A API regional futura precisa distinguir leitura de snapshot,
chamada síncrona no mesmo dono, mensagem com resposta futura e transação
global; não pode converter silenciosamente uma chamada síncrona em promise.

## 4. Trabalho restante antes de habilitar o runtime paralelo

1. Medir separadamente monstro, veículo e stagehand em cenários com e sem
   mods, incluindo callbacks fora do update e autoridade `Master`/`Slave`.
   O perfil por tipo atual dá baseline de update, mas não fecha esse custo.
2. Inventariar assets oficiais das missões/Outpost e os mods alvo por
   origem, versão, callbacks, uso de `world.*`, mensagens, flags e save.
   Não inferir segurança de um script pelo nome do arquivo.
3. Reproduzir fronteiras: monstro e veículo cruzando setor; veículo longo com
   passageiros; stagehand com área multissetor; fio ligando setores distantes;
   líquido atravessando a emenda horizontal; unload/reload durante efeitos.
4. Comparar estado, ordem de eventos, inventários, rewards, interação, dano,
   posição, rede e save/reload entre legado e novo modelo em multiplayer.
5. Definir contratos separados para monster/vehicle/stagehand e para água/fios
   antes de transferir sua execução. Registrar fallback por capacidade, não
   pela identidade do mod.

Nenhuma das observações acima prova que o runtime novo preserve essas
mecânicas. O preview de thread por setor continua sem executar simulação.
