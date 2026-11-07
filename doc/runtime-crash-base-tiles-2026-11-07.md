# Crash de BaseTiles em worker — 07/11/2026

**Branch afetada:** `Experimental`  
**Build observada:** `eb07add8` (`Decode saved tile snapshots on sector workers`)  
**Objetivo:** registrar a causa, o limite de compatibilidade e a correção do
crash Windows ocorrido ao exercitar decode privado de tiles e preparação de
BaseTiles.

## Artefatos recebidos

Os artefatos brutos permanecem fora do Git. Identificação SHA-256:

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `tracy10.tracy` | 7.503.737 bytes | `e5e77b89575bc97c5d21a5476d914d6d33d77c57c080c9362c11bb357a6f36a8` |
| `exploration.zip` | 15.808.604 bytes | `3beb6212e8c23e115e8dd888f120adfce1dab7a64da8ca21eee396e98b3f5ea0` |
| `starbound.log` | 70.307 bytes | `7fb014ae46174ca7616295b3230075d1c90899748b311f578053f01a61e05ca7` |

O Tracy foi preservado como evidência, mas não foi interpretado por um parser
nesta análise. A conclusão abaixo vem da pilha simbólica e do log.

## Resultado observado

Às 18:01:23, a build `eb07add8` encerrou com `Access violation` de escrita no
worker `WorldSectorGeneration`. A pilha é consistente entre as repetições:

```text
StarWorldGeneration.cpp:74 prepareBaseTileArray
StarWorldGeneration.cpp:1261 lambda de prepareTilesAsync
StarWorkerPool.hpp:244 addProducer
StarWorkerPool.cpp:300 WorkerThread::run
```

O endereço de escrita `0x100` corresponde ao acesso a um `ServerTile` de um
array nulo. Não há evidência de script/mod na pilha do crash. O log também
registra assets ausentes de mods durante o carregamento, mas esses avisos são
anteriores e não aparecem no caminho fatal.

## Causa confirmada

O preview `savedTileDecodeAsyncPreview` permite que `loadSectorToLevel(...,
Tiles)` retorne enquanto os bytes estão em decode privado. Nesse intervalo, o
metadado do setor existe, mas seu nível ainda é `None` e não há array de tiles
instalado.

O alimentador de preparação independente chamava
`admitBaseTilesPreparation` logo após essa carga sem exigir `loadLevel >=
Tiles`. `prepareTilesAsync` então recebia `copySector(sector)` nulo. As
asserções não protegem a build Windows de produção, e o worker posteriormente
desreferenciava esse resultado em `prepareBaseTileArray`.

## Correção e contrato

- O alimentador e o prefetch não admitem/geram BaseTiles enquanto o nível de
  carga for inferior a `Tiles`.
- `admitBaseTilesPreparation` exige tanto `loadLevel >= Tiles` quanto array de
  setor presente.
- `prepareTilesAsync` repete a verificação e retorna incompleto com o contador
  `generation.baseTiles.tilesUnavailable`, protegendo outros chamadores.
- Não houve mudança no contrato de workers: eles continuam recebendo somente
  snapshots privados e não acessam banco, mundo vivo, entidades, scripts ou
  callbacks.

## Verificação executada

Foi executada compilação sintática C++17 de `StarWorldStorage.cpp`,
`StarWorldGeneration.cpp` e `StarWorldServer.cpp`, além de `git diff --check`.
Não houve build completo nem regressão Windows nesta máquina.

## Próximo teste obrigatório

No Windows, repetir a rota com `savedTileDecodeAsyncPreview` e
`generationPreparationFeederPreview` ativos. Confirmar:

1. ausência de `Access violation` e de `prepareBaseTileArray` sem tiles;
2. presença de `storage.tiles.async.scheduled/resultsApplied` quando houver
   setores salvos no prefetch;
3. eventuais `generation.baseTiles.tilesUnavailable` apenas durante a espera
   legítima do decode, sem crash;
4. equivalência em mundo salvo, modificado, unload/reload e dois jogadores.

Complementa [sector-io-ready-queue-audit-2026-11-07.md](sector-io-ready-queue-audit-2026-11-07.md) e o
[checklist operacional](regional-runtime-execution-checklist.md).
