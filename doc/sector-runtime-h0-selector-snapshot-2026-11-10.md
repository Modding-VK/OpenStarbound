# H0 / CPU-01: captura de parâmetros do seletor

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`, base `ea36a231c36ba4d9020f7d2026b32783ecf6dcb0` de Experimental.

[RNG e Root](sector-runtime-h0-rng-root-entropy-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

O novo `WorldGenerationSelectorSnapshot` captura por valor largura, altura, seed e commonality já inicializados. Seu materializador usa um construtor explícito dos parâmetros, sem consultar Root ou o RNG global. Os testes conferem bits dos floats, incluindo zero negativo, valores extremos de seed, origem destruída e três leitores com reconstruções privadas. O teste de sequência executa o construtor default legado no dono uma vez e verifica que 100 materializações não consomem novas seeds.

A captura exige que o dono inicialize todos os campos antes de chamá-la: o construtor default legado ainda não inicializa `baseHeight`. O snapshot não captura layout, seletores, assets, cache, identidade de mundo, reserva de RAM ou autoridade de publicação; não é usado pelos jobs nem integrado ao builder real. O construtor default e a ordem de RNG do caminho existente não foram mudados. Falta integrar captura no dono, comparar ordem/RNG e outputs com builder e assets reais, resolver ownership e admissão e validar mods/saves/multiplayer antes de usar workers.

Build local GCC 14/C++17, ASan/UBSan, cache `build/block-info-h0`, GUI/Tracy OFF. O identifier mantido pelo cache é `9c1ccbbd-selector-lifecycle-local-dirty`, anterior à base atual: trata-se de build local suja para testes, não de pacote distribuível identificado pelo commit atual. A bateria padrão passou **763 casos em 40 suites**; os três casos novos passaram também em dez repetições com shuffle e seed 1239. Três microexperimentos permanecem desabilitados. O jogo completo, Root opt-in com os novos objetos, outras plataformas e Actions não foram reexecutados.

Nenhum log/configuração do jogador foi recebido. Não há ganho de setores/s medido. CPU-01/H0/RAM-01 permanecem em 25%; programa 7/196 =3,6%. Fallback CPU, mods/saves/multiplayer e controle autoritativo continuam gates; NPC externo suspenso e Linux Clang adiado.

Fontes locais de 10/11/2026 UTC, bytes e SHA-256:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarTerrainDatabase.hpp` | 1724 | `18fd48a977cf2acef84524d1ae29ed82b713b74c3ee3d8e65f718f4ca1933137` |
| `source/game/StarTerrainSelector.cpp` | 1578 | `377ebfcf7e8f66553918d96ca139081d8a4a32df8f600f4f74fc538bc4054155` |
| `source/game/StarWorldGenerationSelectorSnapshot.hpp` | 666 | `59610377c5bc2fd8ef4d9f184a571bcc7aeb1d92047c0946eef007ca92c69cdc` |
| `source/test/world_generation_selector_snapshot_test.cpp` | 2161 | `3deb8e6626950aaf0bb62eb171fe1e2b5783abf4191e87e2b697dd7f57f847ea` |
