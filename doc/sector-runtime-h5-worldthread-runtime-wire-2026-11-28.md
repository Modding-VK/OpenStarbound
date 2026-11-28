# H5 — WorldThread scale wired to INT ComputeRuntime (`191cb6e4`)

Data: 28/11/2026 UTC. Branch `cursor/h5-hardware-scale-9f77`.

## Objetivo

Usar o tip INT `191cb6e4` (H4 WorldThread runtime `76ab0fa2`) como base
absorvida e ligar o observador H5 GPU-05 ao `WorldGenerationComputeRuntime`
real no `WorldServerThread`.

## Branch / commits

- Merge INT → H5: `c250d69c` (tip INT `191cb6e4`)
- Wiring + rename de enum de roteamento H5: tip desta entrega (ver `git log`)

## Entrega

1. Merge de INT `191cb6e4` na frente H5 (ComputeRuntime + pump no WorldServerThread).
2. `observeWorldGenerationComputeRuntime(...)` mapeia `active` / `rehearsal` /
   `lastPumpCompletions` para o contrato H5.
3. `WorldServerThread::update` grava no `m_worldThreadScale` após `pumpUpdate`
   quando o runtime está ativo (custos de entrega **não** inventados).
4. Enum H5 de roteamento renomeado para `WorldGenerationHardwareRoutingBackend`
   (`Cpu`/`Gpu`) para não colidir com `WorldGenerationComputeBackend` do H4
   (`CpuReference` / `GpuOpenGlCompute`).
5. Teste `ObserveRealComputeRuntimeRehearsal` exercita o runtime real em
   rehearsal (TerrainFieldMax2; sem DenseUint32 nesta frente).

## Validação

- Filtro H5 local: **58/58 PASS** (inclui 5 WorldThreadScale).
- `StarWorldServerThread.cpp.o` recompilado com o wiring.
- Sem Actions (manual only). Sem merge de volta para INT. Sem claim de 50%.

## Limites

- Rehearsal continua ≠ ganho medido.
- Custos calibrados Desktop não são injetados no WorldServerThread neste tip.
- Publish live / dual-context / 2º vendor / benchmark setores/s ainda pendentes
  para GPU-05 50%.

## Próximo passo acionável

1. Injetar amostra Desktop `gain_pipeline` calibrada em pump **live**
   (`rehearsal=false`) e verificar `measuredGainFromLivePump`.
2. Só então reavaliar gates de adoção / percentagem GPU-05.
