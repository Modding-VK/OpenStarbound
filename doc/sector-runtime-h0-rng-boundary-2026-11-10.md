# H0: investigação do RNG no contexto de geração

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`, base `88148dbe9f09421a99d87d48a12a6e5b6fcecb40` de Experimental.

[Etapa anterior](sector-runtime-h0-perlin-cache-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

Objetivo: localizar o consumo de RNG global observado ao construir a árvore de teste, antes de reconstruí-la em workers. O ensaio usa Root e TerrainDatabase reais, assets mínimos e seed global fixa 918; não recebeu configuração nem logs do jogador.

Com a sequência reiniciada antes de cada construção, a fixture `layout(true, true)` avançou o RNG. Os seguintes recortes isolados preservaram o próximo valor: WorldLayout, WorldRegion e Biome vazios; TerrainSelectorParameters com seed explícita; CacheSelector de constante; árvore Cache/Mix/Perlin/FlatSurface; Perlin de blend e BlockNoise com seeds explícitas. Isso restringe a investigação à montagem da fixture, mas ainda não identifica a operação específica ou prova que qualquer construção de layout real seja livre de RNG.

O ensaio não muda geração, ordem de RNG, configuração ou produtor do jogo. A árvore foi montada com metadata controlada e o teste não é benchmark nem prova de compatibilidade com mods, saves ou multiplayer. Não há evidência nova de ganho de setores/s. O próximo passo é dividir `layout(true, true)` em etapas, localizar a chamada concreta e comparar consumo/ordem com o caminho de produção antes de projetar captura e reconstrução privada. Preservar fallback CPU; NPC externo suspenso e Linux Clang adiado.

Fonte local, 10/11/2026 UTC: `source/test/world_generation_real_block_info_test.cpp`, 30988 bytes, SHA-256 `c3f06c2c02b1198e096d782427dd8d7da90b827d752c3aa3ca7521d84b7d7bae`. O build diagnóstico reaproveita GCC 14, ASan/UBSan e Root opt-in descritos na etapa anterior; Actions não foi executado.

Validação: 32/32 casos da suite opt-in WorldGenerationRealBlockInfoTest aprovados com ASan/UBSan. Os demais testes não foram reexecutados nesta etapa. Nenhum marco completo adicional foi atingido: CPU-01/H0/RAM-01 seguem em 25% e programa em 3,6%.
