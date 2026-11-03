# Pré-planejamento de microdungeons

Estado: primeira fundação implementada na branch `Experimental`; não executa
microdungeons em paralelo e não altera a ordem de publicação.

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
