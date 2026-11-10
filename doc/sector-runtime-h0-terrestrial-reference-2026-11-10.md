# H0: referência independente do builder terrestre

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`, base `ff1904fd94fc6b95a6b472d59efd1bc36a04eb38` de Experimental.

[Etapa anterior](sector-runtime-h0-terrestrial-builder-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

## Estado: em validação

O teste confronta o builder terrestre atual com os corpos herdados de `buildTerrestrialLayout`, `addLayer` e `buildRegion` da revisão `f22cf9c9`, anterior à integração dos snapshots. A referência de teste adapta o acesso ao receptor e qualifica tipos privados; o algoritmo herdado foi conservado. As duas funções externas adaptadas foram conferidas contra a base, ignorando apenas whitespace e adaptações de acesso/qualificação.

Para seeds de região 0, 42 e UINT64_MAX, a comparação cobre o Json do layout completo, o próximo valor do RNG global, 1.435 coordenadas e arrays BaseTiles de 1.024 tiles. Um segundo caso usa tamanho de sub-região inválido e compara o tipo de exceção e o próximo RNG. Root, TerrainDatabase, BiomeDatabase e MaterialDatabase são reais; a thread de manutenção de Root é bloqueada durante a comparação para não interferir no RNG. As duas rotas compartilham bibliotecas de seletores, assets e o kernel BaseTiles, logo a referência é independente do builder, não de toda a engine.

O cenário é um mundo 64×64 com cinco camadas, bioma e materiais mínimos, arquivos de fixture locais e seeds explícitas. Não foram recebidos saves, logs ou configuração do jogador. Ainda faltam catálogo de assets/mods representativo, mundo salvo e novo no jogo, multiplayer, validação de desempenho e identidade/admissão do contexto completo. Não se afirma ganho de setores/s ou que a arquitetura esteja pronta para adoção. A próxima etapa é integrar a captura profunda de layout/seletores com identidade e admissão de memória e comparar em sombra antes de conectar workers. Compatibilidade de mods e fallback CPU permanecem essenciais; NPC externo suspenso e Linux Clang adiado.

Validação local GCC 14/C++17, ASan/UBSan, GUI/Tracy OFF, Root opt-in: **764 testes padrão e 73 opcionais aprovados**; os dois novos casos passaram em três repetições com shuffle e seed inicial 1243. Três microexperimentos desabilitados. O jogo completo, outros sistemas e Actions não foram executados. O cache usa identifier `9c1ccbbd-selector-lifecycle-local-dirty`, anterior à base, e não identifica um pacote distribuível. CPU-01/H0/RAM-01 continuam em 25%, programa 7/196 =3,6%.

Fontes locais de 10/11/2026 UTC, bytes e SHA-256:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/test/world_generation_real_block_info_test.cpp` | 70072 | `aae195992d37bf08384a5ec8675d4f4ee86b5a811272c31f742d0468a450b3b3` |
| `source/game/StarWorldLayout.cpp` | 29595 | `5aaa321cbae6d0debc2daf7bc05bc53ebd870686827f08070164d2fb077acfa9` |
| `source/game/StarWorldLayoutRegion.cpp` | 3546 | `b5151299ee65cdad90f85cf416c0c6066aceed91f1612e2780ee9bd5942d196e` |
