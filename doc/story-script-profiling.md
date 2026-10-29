# Diagnóstico de scripts de missões e locais

O fluxo de missões atravessa três donos: o `QuestManager` do jogador, os
contextos Lua de cada `WorldServer` (incluindo mundos instanciados) e os
contextos Lua do `UniverseServer`. NPCs e objetos do local continuam no update
de entidades do mundo. O preview de threads por setor não executa esses
scripts.

Com `explorationProfiling.enabled: true`, os relatórios existentes
`[ExplorationProfile]` passam a incluir componentes `playerQuests` e
`universeScripts`, além de `server`. Os estágios de nome fixo são:

| Componente | Estágio | Inclui |
| --- | --- | --- |
| `playerQuests` | `quest.update` | Seleção, expiração e update das quests do jogador. |
| `playerQuests` | `quest.message` | Entrega de mensagem ao gerenciador de quests. |
| `playerQuests` | `quest.offer` | Inicialização e oferta de uma quest. |
| `playerQuests` | `quest.interestingObjects` | Consulta de objetos relevantes às quests. |
| `server` | `story.world.contextUpdate` | Update de um contexto Lua do mundo. A soma pode ser comparada ao estágio já existente `scriptContexts`; não somar ambos como trabalho distinto. |
| `server` | `story.world.message` | Entrega de mensagem aos contextos Lua do mundo. |
| `universeScripts` | `story.universe.contextUpdate` | Update de um contexto Lua do universo. |
| `universeScripts` | `story.universe.message` | Entrega de mensagem aos contextos Lua do universo. |
| `universeScripts` | `story.universe.warpOverride` | Consulta síncrona aos scripts que podem alterar uma viagem. |

As zonas Tracy marcam também o encaminhamento de mensagens para mundos e o
tratamento de mensagens do universo. Nenhuma métrica armazena nome de missão,
conteúdo de mensagem, identificador do jogador ou payload. Os componentes do
jogador e do universo só alocam coletores quando o perfil opcional está ligado;
as zonas Tracy só existem na build diagnóstica. Não há alteração da ordem de
execução, da API Lua, do save ou do protocolo multiplayer.

Os relatórios mostram tempos e quantidades **por ponto de execução**, não uma
trilha causal de ponta a ponta. Um mesmo evento pode aparecer em mais de um
componente e em máquinas distintas. Antes de mover scripts do Outpost ou das
missões da história para threads setoriais, inventariar callbacks concretos,
leituras/escritas globais, ordem observável, passagem entre mundos e efeitos
de save/reload; a instrumentação não demonstra que um callback seja seguro
para paralelismo.

A [auditoria das fronteiras de entidades e setores](entity-sector-story-boundary-audit.md)
inclui `Stagehand`, monstros e veículos entre os possíveis scripts de um local.
O estágio `quest.update` inclui os updates de quests; callbacks de conclusão
ou recompensa disparados fora dele ainda não têm medição própria.
