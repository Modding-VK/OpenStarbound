# H5 — WorldThread scale adapter (pos H4 `76ab0fa2`)

Data: 28/11/2026 UTC. Branch `cursor/h5-hardware-scale-9f77`.

## Avaliacao do tip H4

H4 `76ab0fa2` entrega `WorldGenerationComputeRuntime` + `worldThreadPump`
(opt-in, default off, rehearsal cloud-safe). **Desbloqueia parcialmente** o
proximo passo H5 GPU-05:

| Sinal H4 | Suficiente para H5? |
| --- | --- |
| Pump no WorldServerThread (nao bloqueante) | Sim — observacao de tick |
| Rehearsal cloud-safe (CPU apos pump) | So prova path; **nao** e ganho |
| Desktop `gain_pipeline` gpu_faster=1 | Sim — custos calibrados externos |
| Publish live / dual-context GL / 2o vendor | **Nao** — ainda faltam p/ 50% |

## Entrega H5 neste tip

`WorldGenerationWorldThreadScaleRecorder`: observa `active` / `rehearsal` /
`lastPumpCompletions` + custos calibrados. Rehearsal **nunca** marca
`measuredGainFromLivePump`. Sem DenseUint32. H5 permanece **25%**.

Seguimento (wiring no tip INT): ver
[worldthread-runtime-wire](sector-runtime-h5-worldthread-runtime-wire-2026-11-28.md)
apos absorcao INT `191cb6e4`.

## Validacao

Filtro H5 (adapter original): **53/53** local. Apos wiring INT: **58/58**.

## O que ainda falta p/ 50%

1. ~~Wiring real H5<-H4 runtime~~ — feito sobre INT `191cb6e4`
2. Amostras **live** (`rehearsal=false`) no WorldThread com custos Desktop
3. Publish live / dual-context (H4) se adocao autoritativa
4. Benchmark setores/s (nao so harness)
