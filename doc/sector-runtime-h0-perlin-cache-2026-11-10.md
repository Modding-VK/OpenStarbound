# H0: Perlin e seletores com cache

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`; base `9c1ccbbd9a9176c1b2144ab3f5c30bdc1900124b` da Experimental.

[Relatório anterior](sector-runtime-h0-real-sector-sampling-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

O construtor padrão de Perlin já inicializava os campos. Na cópia de um Perlin não inicializado, a atribuição deixava metadata anterior no destino e o construtor de cópia deixava campos indefinidos. A cópia agora transfere a metadata da origem antes de limpar os buffers. O teste negativo reproduziu a divergência para float e double.

A factory de CacheSelector usava o tipo do wrapper em vez do tipo declarado para sua fonte. Um cache de constante falhava com JsonException ao tentar criar outro cache. A correção usa o tipo declarado; o teste negativo reproduziu a falha. Configurações afetadas passam a construir a fonte solicitada, mas a compatibilidade com mods completos ainda precisa ser validada.

O teste opt-in usa Root e TerrainDatabase reais com assets mínimos e uma árvore privada de CacheSelector, Mix, FlatSurface e Perlin, além de blend noise e block noise. Compara BlockInfo e arrays BaseTiles 32×32, exercita reconstrução, eviction e caches privados, inclusive IslandSurface. As rotas compartilham kernel e aplicação BaseTiles; a comparação isola bindings e estado dos caches. O layout usa metadata controlada.

Foi observado que construir a árvore de teste avança o RNG global, embora Perlin isolado com seed explícita não o faça. A origem desse consumo ainda precisa ser isolada. Não reconstruir essa árvore em workers como contexto puro antes de preservar ou eliminar compativelmente esse consumo e validar a ordem de RNG. Defaults de metadata, caches Worm/Karst, ownership profundo, admissão e publicação também permanecem pendentes.

O build local usa GCC 14, C++17, ASan/UBSan e Root opt-in, com arquivos mínimos de fixture. Nenhum log ou configuração do jogador foi recebido nesta etapa. Não há medição de ganho de setores por segundo, validação de jogo completo, saves, multiplayer, mods completos ou execução de Actions. Compatibilidade e fallback CPU permanecem requisitos; externalização de NPCs está suspensa e Linux Clang adiado.

Validação: 760 casos padrão e 44 opcionais, 41 grupos CTest aprovados com ASan/UBSan. Três microexperimentos permanecem desabilitados. O primeiro ensaio após a correção detectou consumo de RNG global na árvore, e o teste foi atualizado para registrar esse comportamento; a bateria CTest passou depois. Quatro corpos de BlockNoise foram relocados sem mudança para permitir a ligação do teste real.

Próximo passo: identificar o consumo de RNG na construção da árvore, estabelecer identidade, ownership e admissão seguros e comparar os resultados antes da execução em sombra por setor.

Fontes locais, 10/11/2026 UTC, bytes e SHA-256:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/core/StarPerlin.hpp` | 17458 | `ce4517ca9e36c69d05cbc5d13b24c0c5e067d634f13af90042fd5ead7e61a37d` |
| `source/game/terrain/StarCacheSelector.cpp` | 852 | `80918a0d2d7875153a175666f5be55c5af59c08d1134ba9bd704978a4dbf44d8` |
| `source/test/perlin_lifecycle_test.cpp` | 5186 | `dc5ce35d166afc0ed844da7bc02352330cd84b36603562358c63b59970978e32` |
| `source/test/world_generation_real_block_info_test.cpp` | 29609 | `a31945375c86eb043571305299858612b39e1d0b1973aa3b614e29487ef362ca` |
