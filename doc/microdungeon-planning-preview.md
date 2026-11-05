# Pré-planejamento de microdungeons

Estado: primeira fundação implementada na branch `Experimental`; não executa
microdungeons em paralelo e não altera a ordem de publicação.

## Próximo recorte multithread

`WorldGenerationTasks` já reserva uma identidade por mundo, região, setor e
etapa para `MicroDungeonPlan` e `CaveLiquidPlan`. A reserva impede que dois
workers recebam a mesma etapa do mesmo setor e permite revogar o resultado no
unload. Ela ainda não autoriza acesso ao mundo vivo: o próximo produtor deverá
receber somente um snapshot de template, tiles acolchoados e colocações
pendentes copiadas. A aplicação de tiles, entidades, fios, líquidos e promises
continuará na região dona após validar revisão e footprint.

O código agora declara `MicroDungeonPlanSnapshot` e
`PreparedMicroDungeonPlan`. O primeiro não aceita referências ao `WorldServer`,
entidades, Lua ou promises; o segundo somente devolve descritores ordenados e
tempo de execução. O produtor e a fila assíncrona serão ligados no recorte
seguinte, após o snapshot incluir o halo de tiles e sua revisão.

O contrato `WorldMutationBatch` também foi introduzido para a publicação
futura. Ele guarda a identidade da tarefa, revisões esperadas por setor e uma
lista ordenada de alterações de tile. Nenhum worker pode aplicar esse batch
diretamente: a região dona deverá validar revisões, reservar o footprint e
aplicar as operações em ordem antes de confirmar a tarefa.

`WorldMutationBatch::matches` já realiza a parte pura dessa validação: exige
identidade idêntica, revisão atual para cada setor do footprint e sequência
estritamente crescente. O adaptador da região fornecerá as revisões atuais e
somente então executará as mutações no mundo vivo.

## O que muda

`WorldGenerator::generateMicroDungeons` agora transforma a fila ordenada de
colocações válidas em uma lista local de `MicroDungeonPlan`. Cada descritor tem
apenas `bounds`, posição, seed, nome de dungeon e, quando existe, o índice da
solicitação de colocação. Ele não conserva ponteiro para o mundo, entidade,
Lua ou facade. A aplicação continua imediatamente depois, na mesma ordem, pelo
mesmo `DungeonGeneratorWorld` e na thread do mundo.

Escolher o nome pelo seed antes de chamar o gerador é equivalente ao fluxo
anterior: a escolha já era determinística por posição e ocorria antes de cada
chamada de `generate`. Promises de colocações enfileiradas continuam sendo
cumpridas somente após o `generate` obter sucesso. Planos posteriores da mesma
solicitação são ignorados quando uma colocação anterior a cumpre.

## Diagnóstico opcional

Com as duas opções abaixo, o relatório de exploração registra o volume do
plano e do footprint realmente retornado pelo gerador:

```json
"worldThreading" : {
  "microDungeonPlanPreview" : true
}
```

`explorationProfiling.enabled` também precisa estar ativo para gravar as
métricas. Os contadores são `generation.microDungeonPlan.candidates`,
`queuedCandidates`, `accepted`, `footprintTiles` e
`crossSectorAccepted`; `footprintSectors` é uma amostra por dungeon aceita.
Não são registrados nomes de dungeons, parâmetros de mods ou payloads.

Com `regionalDetails: true`, a facade viva também conta acessos e efeitos de
microdungeons sob `generation.microDungeonFacade.*`: leituras de material,
líquido e ID de dungeon; escritas de materiais, mods, líquidos, proteção e IDs;
criação de objetos, veículos, plantas, drops, NPCs, monstros e stagehands;
fios, limpeza de entidades e marcações. Os rótulos são fixos e não incluem
identificadores de mods. A coleta ocorre somente para a facade de
microdungeons, que é criada sem `markForActivation`; dungeons compostas e
comandos continuam em suas métricas próprias.

As fases autoritativas do gerador são separadas em `logs/exploration/dungeons/`
quando a gravação de relatórios está ativa. O arquivo reúne rótulos
`dungeon.*`, incluindo `definitionLoad`, `generate`, `canPlace`, `placePart`,
`flush` e a publicação de objetos e entidades. Ele ajuda a identificar o
recorte que poderá virar um comando privado, sem registrar nome de dungeon ou
dados de mods.

No relatório `generation/`, as etapas fixas
`generation.microDungeonPlan.collect`, `.sort`, `.describe`, `.apply` e
`.cleanup` delimitam o custo da varredura dos tiles, ordenação, construção dos
descritores, chamada ao mundo vivo e remoção de pedidos cumpridos. `.apply`
continua serial e contém as métricas `dungeon.*`; as demais etapas não
publicam alterações no mundo.

## Limites

O descritor ainda não é um plano privado completo. `MicroDungeonFactory::generate`
usa a facade viva e pode escrever tiles, objetos, entidades, fios e outros
efeitos antes de retornar o conjunto de posições. Portanto, ele não pode ser
enviado a worker nem pausado entre escolha e aplicação. O próximo recorte deve
adaptar o writer/facade para comandos privados, capturar o footprint completo
e validar revisões/reservas antes de qualquer publicação.

Esta fundação preserva o caminho legado, saves, rede e API de mods. A comparação
necessária usa a mesma seed, assets, mods e rota, verificando tiles, estruturas,
entidades, promises, reload e multiplayer antes de habilitar qualquer execução
externa.
