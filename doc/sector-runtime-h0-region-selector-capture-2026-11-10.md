# H0 / CPU-01: captura dos parâmetros de região no dono

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`, base `f22cf9c9c08c83dcc2952e75f395f42c098e3443` de Experimental.

[Etapa anterior](sector-runtime-h0-selector-snapshot-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

`WorldLayout::buildRegion` agora prepara os parâmetros de terreno, caverna de foreground e caverna de background por uma captura de valor no dono, e materializa os três parâmetros para o fluxo existente. A factory da captura ainda executa o construtor default legado uma vez, na mesma posição do fluxo, preservando seu consumo de seed global. As seeds derivadas mantêm as mesmas etiquetas e ordem. Nenhum worker ou nova publicação foi ativado.

O teste compara os três parâmetros e o próximo valor do RNG global com a construção anterior para quatro seeds de região, incluindo zero e UINT64_MAX. Também verifica materialização privada e os bits dos floats nos testes já existentes. Os testes exercitam a nova factory contra uma referência independente da preparação de parâmetros; não executam `buildRegion` inteiro com planeta, biome database ou mods. O objeto normal `StarWorldLayout.cpp` compilou, mas o jogo completo e Root opt-in não foram reexecutados nesta etapa.

O snapshot não é contexto profundo, versão de assets, admissão de RAM ou autorização de commit. Os parâmetros devem ser capturados no dono após inicialização; o construtor default ainda deixa `baseHeight` indefinido até o builder atribuí-lo. `addBiomeRegion` não foi alterado. Próximo passo: integrar teste de builder real com assets e comparar outputs/ordem de RNG, depois compor ownership, identidade e admissão antes de jobs em sombra. Não remover nem deslocar a chamada global presumindo que a seed descartada seja irrelevante.

Validação local com GCC 14/C++17, ASan/UBSan, build `build/block-info-h0`, GUI/Tracy OFF. A bateria padrão passou 764 casos em 40 suites; os quatro casos da suite de snapshot passaram dez vezes com shuffle e seed inicial 1240; o objeto normal compilou. Três microexperimentos permanecem desabilitados. Identifier do cache `9c1ccbbd-selector-lifecycle-local-dirty` é anterior à base atual: build local suja, não instalador identificável pela revisão atual. Nenhum log/config de jogador foi recebido. Sem benchmark de setores/s, testes de mods/saves/multiplayer, Actions, release ou merge em main. Fallback CPU preservado, NPC externo suspenso, Linux Clang adiado. Nenhum marco completo adicional: programa continua 3,6%.

Fontes de 10/11/2026 UTC, bytes e SHA-256:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldLayout.cpp` | 32946 | `79489d5c285d7432ff947cd5808bf7b3a1ad991578a0f4e2a195f5aa878d80c5` |
| `source/game/StarWorldGenerationSelectorSnapshot.hpp` | 1588 | `a81b73fd31f0ac2968ca17aa767b115b7dbaefd11777bd541ec07f78cf2da797` |
| `source/test/world_generation_selector_snapshot_test.cpp` | 3123 | `f4ad4286c1fd01ae5ede9298cc93aa1830b0a11a828958ca267b16e392c95ded` |
