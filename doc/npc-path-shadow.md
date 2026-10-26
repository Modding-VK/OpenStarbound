# Busca de rota de NPC em modo sombra

Estado em 26/10/2026: protótipo experimental das fases 1–2 do
[plano de NPCs](npc-ai-rework-action-plan.md), **desligado por padrão**.
O caminho serial existente ainda decide movimento, efeitos e multiplayer.
Nenhum resultado do worker é instalado no NPC ou enviado ao cliente. Este
recorte executa somente A* de plataforma, sem Lua de NPC ou objeto.

## Ativação para diagnóstico

Depois de coletar o baseline com a opção desligada, fechar o processo e usar:

```json
"explorationProfiling" : { "enabled" : true, "npcDetails" : true },
"worldThreading" : { "npcShadowWorkers" : 1 }
```

Mesclar as chaves com os objetos de configuração já existentes. Aceita 1 ou
2 workers; 0 ou ausência da chave desliga o recurso. Acima de 2 é limitado a
2. A opção é lida uma vez por processo, então requer reinício. O pool
`NpcPathShadow` é separado do pool de geração de setores. Há no máximo duas
tarefas admitidas por worker e uma tarefa pendente por NPC. Saturação descarta
somente o diagnóstico.

## Fronteira implementada

Quando o `PathController` inicia uma rota, o observador do NPC mestre no
`WorldServer` captura uma região em torno da origem e do destino (destino
ajustado ao wrap horizontal), com 16 tiles de margem. Largura e altura não
passam de 113 tiles; a captura aceita no máximo 16.384 tiles e 32.768 blocos
de colisão. Regiões tão largas quanto o mundo e tiles descarregados são
indisponíveis, nunca ar livre. Antes de consultar o cache de colisão, a
captura verifica se todos os tiles da região estão carregados.

O snapshot copia geometria, colisões, líquido e gravidade. Depois da captura,
as consultas do A* sobre ele não acessam `World*`, NPC, Lua ou entidades
mutáveis. Uma consulta fora da cobertura termina com `MissingCoverage`. A
referência serial e o worker calculam sobre **o mesmo snapshot**, com os
mesmos parâmetros e limite de 2.048 nós explorados por busca. Comparam
resultado e arestas da rota. Esse acordo mede determinismo entre os dois
cálculos; não prova equivalência com o mundo vivo, que pode mudar durante a
fila. `SearchLimit` também não é prova de ausência de rota.

Cada pedido leva época do `WorldServer`, ID e encarnação do NPC, sequência e
step de captura. A captura registra ainda a revisão e presença dos setores
cobertos. Ao remover o NPC, a sua token de vida é invalidada. Um worker pode
terminar fisicamente depois do cancelamento, mas não publica efeitos. Na
coleta, o NPC confere época, encarnação e sequência; revisões diferentes
geram `staleRevision`. Esse carimbo é **conservador**: consultas e atualizações
de cache de colisão podem avançar revisões sem mudar a topologia. Antes de
ativar rotas de workers, ainda será necessário definir revisões semânticas,
alvo/obstáculos dinâmicos, expiração e validação autoritativa.

## Métricas no `ExplorationProfile`

As contagens têm prefixo `npc.shadow.`:

| Contagem | Significado |
| --- | --- |
| `submitted`, `completed`, `npcBusy`, `saturated` | Admissão, conclusão, NPC já com tarefa e limite global atingido. |
| `missingCoverage`, `captureUnavailable` | Tile/região fora da cobertura; outra falha de captura ou enfileiramento. |
| `matches`, `mismatches`, `partialRoute`, `searchLimit`, `error`, `cancelled` | Classificação da comparação sobre o snapshot; uma falha do A* com `returnBest` pode entregar rota parcial. |
| `staleRevision`, `staleResult` | Setor alterado/descarregado; identidade do NPC/mundo divergente. |
| `capturedTiles`, `missingTiles`, `capturedBlocks` | Volumes acumulados das capturas concluídas. Captura com tile ausente é recusada antes de enfileirar, portanto `missingTiles` dos relatórios aceitos tende a zero. |

As distribuições de tempo são `npc.shadow.capture`, `reference`, `queue` e
`worker`. As três primeiras medem, respectivamente, cópia no mundo, busca de
referência no mundo e espera pelo worker. `worker` mede o cálculo na thread
auxiliar. A busca de referência não entra nos contadores de A* autoritativo.

## Limites e próximos critérios

O diagnóstico **aumenta** o trabalho na thread do mundo: copia tiles,
atualiza o cache de colisão e roda uma segunda busca serial antes de iniciar
o A* normal. A atualização do cache pode incrementar revisões dos setores e
interferir com a investigação de invalidações da geração. Não ligar esta
opção durante a medição baseline de setores ou ao comparar builds sem a
mesma configuração. Os limites atuais são de contagem de tiles/blocos e
tarefas; ainda falta um teto medido em bytes e em tempo de captura.

Antes de considerar a fase 2 validada, comparar snapshots e resultados com
cenários conhecidos de colisão dinâmica, plataformas, líquidos, gravidade,
wrap, áreas não carregadas e NPCs de mods; medir custo, descartes e
divergências em jogo. A [auditoria da fronteira Lua](npc-lua-boundary-audit.md)
descreve por que este protótipo não externaliza a IA completa.

Revisão local deste recorte: o CMake Linux normal configurou e as seis unidades
C++ alteradas do módulo `star_game` compilaram como objetos. Não houve linkagem
do jogo completo, build Windows, execução no jogo nem coleta de resultados.

## Correção de compilação Windows

A execução Actions `40398453882`, no commit `0f727ed8`, falhou ao compilar
`StarNpcPathShadow.cpp`: o Clang considerou ambígua a construção de
`PathReadUnavailable` com literal de texto e `false`. As seis ocorrências
no diagnóstico e no snapshot agora passam `std::string` explicitamente,
selecionando o construtor público `(std::string, bool)`. A opção de não
capturar stack trace e as condições de rejeição permanecem iguais.
A correção recebeu revisão do diff; a compilação Windows depende da próxima
execução manual do Actions.
