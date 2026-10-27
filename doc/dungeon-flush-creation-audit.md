# Auditoria de criação de objetos e NPCs no flush de dungeons

Estado: auditoria estática de `9cbdc328`; sem alteração de comportamento.
Motivação: no mundo `383738425:-48324202:-255713414:5`, sete flushes
somaram 3,70 s, dos quais 1,41 s em `dungeon.flush.objects` e 1,55 s em
`dungeon.flush.entities`. Os tempos são inclusivos; não representam apenas
consultas aos bancos. A coleta não mede custo por tipo nem por objeto.

## Caminho real

| Entrada | Consultas/construções | Dependências que impedem reuso ingênuo |
| --- | --- | --- |
| `DungeonGeneratorWriter::flush` objetos | Ordena posições; `DungeonGeneratorWorld::placeObject` chama `signalRegion`, `ObjectDatabase::createForPlacement`, `WorldServer::addEntity` | Orientação depende do terreno vivo; construtor incorpora parâmetros; `addEntity` chama `init`, atualiza `EntityMap` e tiles de entidade. Ordem espacial de colocação importa. |
| Bioma e veículos no mesmo bloco de perfil | Seleção por seed/posição; plantas, objetos de pool, baús ou veículos novos; `addEntity` | Mesmo rótulo `dungeon.flush.objects`; não atribuir seus 1,41 s só aos objetos explícitos. Seleção e posição podem alterar resultado. |
| `flush` NPCs | `NpcDatabase::generateNpcVariant` chama `buildConfig(typeName, overrides)`, gera humanoide/nome, avalia funções de nível; `createNpc` e `addEntity` | Seed, overrides, espécie, nível, identidade e scripts definem cada NPC. Seed ausente usa `Random::randu64`; pré-geração deve capturá-la na ordem correta. `init` pode executar efeitos dependentes do mundo. |
| `flush` monstros | `MonsterDatabase::monsterVariant(type, seed, params)` usa `m_monsterCache`; `createMonster`, `addEntity` | O cache já cobre chave completa de variante, mas entidade e inicialização são novas. |
| `flush` stagehands | `StagehandDatabase::createStagehand` faz `jsonMerge` da definição com parâmetros e constrói entidade; `addEntity` | Parâmetros podem ser únicos e scripts dependem do estado do mundo. |
| Fios após entidades | `connectWireGroup` consulta `WireEntity` no mapa por posição e liga nós | Exige todas as entidades já inseridas; ordem atual preserva essa dependência. |

`ObjectDatabase::getConfig` já usa `HashTtlCache` com mutex, chaveado por
nome. `canPlaceObject` e `createObject` consultam a mesma configuração; o
segundo acesso costuma atingir o cache, mas ainda há lookup e lock. Cachear
instâncias de `Object` ou `Npc` seria incorreto: posição, inventário, scripts
e ciclo de vida são próprios de cada entidade. `MonsterDatabase` também já
tem cache de variante. A definição do stagehand está carregada em mapa, mas
o merge com parâmetros ocorre por instância.

## Candidatos, em ordem de investigação

1. **Decompor os tempos inclusivos.** Usar os novos contadores de volume do
   commit `9cbdc328` e depois medir, com rótulos fixos e opção de profiling,
   tempo de `signalRegion`, validação/orientação, criação, `addEntity/init` e
   cada subtipo de bioma. Separar NPC: `buildConfig`, geração de identidade,
   construção e `addEntity/init`. Limitar cardinalidade; não gravar nomes ou
   parâmetros de mods no log. Sem isso, não atribuir 1,41/1,55 s a cache.
2. **Reusar configuração imutável de NPC quando a chave for idêntica.**
   `buildConfig` faz merges recursivos por tipo e overrides; considerar cache
   limitado/TTL de resultado imutável com chave completa. Confirmar que
   mudanças dinâmicas de assets/patches invalidam a entrada. Manter criação
   da variante, RNG e entidade por NPC. Medir taxa de repetição antes de
   implementar; overrides únicos podem eliminar o ganho.
3. **Reduzir lookup redundante de configuração de objeto.** Uma API interna
   pode obter o `ObjectConfig` uma vez e usá-lo para orientação e construtor,
   somente se `findValidOrientation` e `createObject` preservarem exatamente
   as mesmas regras e tempo de leitura de estado. Ganho provável pequeno;
   medir separadamente do custo de `init` antes de editar.
4. **Preparar apenas dados puros.** Variantes e configurações podem ser
   calculadas antes do commit quando toda seed/entrada estiver fixada e as
   dependências de assets forem versionadas. `addEntity`, validação de
   colocação, scripts, fios e publicação permanecem no dono do mundo até o
   [protocolo de retomada](dungeon-generation-resume-plan.md) estar pronto.

## Restrições de equivalência

Conservar ordem de colocação, RNG, chamadas de `init`, falhas de orientação,
IDs, inventários, nomes, parâmetros Lua, fios, tile entities, save e pacotes
de rede. Testar na mesma cópia de universo e stack de mods, inclusive
estruturas compostas, FU e mundo flutuante. Um cache não pode fazer um NPC
reutilizar identidade, script ou estado de outro. Nenhum ganho de desempenho
é afirmado por esta auditoria; os contadores de volume e a decomposição
subsequente decidirão qual candidato merece implementação.

## Primeira coleta de volume e decomposição seguinte

Na coleta `1704467a` do mundo `383738425:-48324202:-255713414:9`, uma
`apexresearchlab` colocou 228 objetos em 11 partes. Seu `dungeon.generate`
levou 316 ms e `dungeon.flush` 235 ms, dos quais 159 ms pertencem ao bloco
inclusivo `dungeon.flush.objects`. Não houve NPCs nem stagehands nessa
dungeon. A amostra não sustenta cache de NPCs como solução para esse caso.

Foram acrescentados temporizadores fixos por chamada de objeto:
`dungeon.object.signalRegion`, `dungeon.object.createForPlacement` e
`dungeon.object.addEntity`. `createForPlacement` ainda inclui validação de
orientação e construção, que poderão ser separadas caso dominem a próxima
coleta. Os tempos por objeto são subtotais de `dungeon.flush.objects`;
veículos e itens de bioma continuam dentro deste último. A coleta depende de
`explorationProfiling.enabled` e não registra nomes nem parâmetros do mod.
