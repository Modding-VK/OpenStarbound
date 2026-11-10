# H0: separação entre RNG do layout e manutenção de Root

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`, base `155323a16b67564bf609d635b98ccb38e297ea0a` de Experimental.

[Investigação anterior](sector-runtime-h0-rng-boundary-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

## Resultado

A conclusão anterior de que a fixture de layout consumia RNG não estava estabelecida: seu teste mantinha o Root ativo, cuja thread de manutenção chama `Random::addEntropy()` independentemente da montagem do layout. A mesma asserção produziu resultados opostos em execuções isoladas. O teste agora encerra o Root e aguarda sua thread terminar antes de reiniciar o RNG; retém uma referência ao TerrainDatabase já criado para construir seletores.

Com seed fixa 918, a fixture de layout, suas partes de seletores/biomas/camadas, caches e noises preservam o próximo valor do RNG nesse cenário isolado. Isso não garante pureza de todos os construtores, de assets arbitrários ou do builder de planeta.

O código real de `WorldLayout::buildRegion`, por outro lado, constrói `TerrainSelectorParameters` por default antes de chamar `withSeed` com uma seed derivada. O construtor default chama `Random::randu64()`. Um teste independente verifica que essa construção consome uma seed global e que `withSeed` não consome outra; `baseHeight` é inicializado na fixture antes da cópia. `addBiomeRegion` também contém construção default. Preservar a ordem e o número dessas chamadas até existir uma decisão de compatibilidade validada com assets e mods.

Esta entrega muda apenas testes e documentação; não elimina chamadas de RNG nem altera geração, publicação, configurações ou saves. A próxima etapa é desenhar uma captura de parâmetros no dono que preserve o consumo legado e entregue seeds explícitas ao futuro contexto privado; comparar outputs e RNG com o builder real antes de ativar workers. A inicialização default de `baseHeight`, `regionLiquids`, `biomeTransition` e `horizonClouds` continua sob auditoria.

Não foram recebidos logs ou configuração de jogador. Não há benchmark de streaming, validação com mods/saves/multiplayer ou ganho de setores/s demonstrado; o alvo regional por região 2×2 e fallback CPU permanecem inalterados. NPC externo suspenso e Linux Clang adiado. Nenhum Actions foi iniciado.

Fonte de teste de 10/11/2026 UTC: `source/test/world_generation_real_block_info_test.cpp`, 33280 bytes, SHA-256 `9cef792225c542adaa4cde67d021adaba10068b84655430dc1017c83f71a10ba`. Diagnóstico local usa GCC 14, ASan/UBSan e Root opt-in, conforme o relatório anterior.

Validação: 33/33 casos da suite de amostragem passaram; os três casos de RNG passaram em dez repetições adicionais com shuffle e seed inicial 1238. ASan/UBSan não relataram falhas. As demais suites e o jogo completo não foram reexecutados nesta etapa. Nenhum marco completo adicional foi atingido: CPU-01/H0/RAM-01 seguem em 25% e o programa em 3,6%.
