# H0: captura de parâmetros com factory real

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`; base `f28a8578b7c3777732be53b0202f380795999c1a` de Experimental.

[Etapa anterior](sector-runtime-h0-region-selector-capture-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

Três verificações complementares exercitam os parâmetros capturados com Root e TerrainDatabase reais e arquivos mínimos de fixture:

1. Comparar a captura da seed de terreno com a preparação anterior na factory de Cache/Mix/Perlin/FlatSurface, incluindo configuração armazenada e bits de amostras.
2. Reconstruir seletores de caverna a partir da configuração armazenada e comparar amostras após aquecimento e eviction da LRU.
3. Verificar que uma falha de tipo de seletor não adiciona consumo de RNG além da seed global legada e que uma criação válida posterior funciona.

Os testes encerram Root antes de medir RNG, aguardando a thread de manutenção, e retêm o banco já criado. Usam seeds explícitas e grafos privados. Não executam `WorldLayout::buildRegion` completo nem o builder procedural de planeta; comparam fronteiras de parâmetros e seletores reais. As duas rotas usam as mesmas implementações de seletores, portanto isso não é referência independente do algoritmo inteiro.

Validação local GCC 14/C++17, ASan/UBSan e Root opt-in, com assets mínimos gerados pela fixture: **764 testes padrão e 49 opcionais aprovados**, 42 grupos CTest aprovados. Os três novos casos foram repetidos cinco vezes com shuffle e seed inicial 1241. Há três microexperimentos desabilitados. Root e a factory foram recompilados; o jogo completo e outras plataformas não foram executados. O identifier do cache continua `9c1ccbbd-selector-lifecycle-local-dirty`, anterior à base, e não identifica uma build distribuível desta revisão. Nenhum Actions foi disparado.

Sem mudança de runtime, orçamento, jobs, formatos ou configuração. O teste não estabelece equivalência com mods/saves/multiplayer nem comprova maior entrega de setores. Próximo passo: executar o builder completo com bancos e assets reais, comparar outputs e ordem observável; compor identidade e admissão do contexto antes de jobs em sombra. CPU-01/H0/RAM-01 seguem em 25%, programa 3,6%; fallback CPU e compatibilidade seguem gates, NPC externo suspenso e Linux Clang adiado.

Nenhuma captura ou configuração de jogador recebida. Fontes locais em 10/11/2026 UTC:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/test/world_generation_real_block_info_test.cpp` | 35740 | `e12eb1b03fc97c941b73b22d974010ae3ad0cc78ebc7b8ec2d3c54b347102f6b` |
| `source/game/StarWorldGenerationSelectorSnapshot.hpp` | 1588 | `a81b73fd31f0ac2968ca17aa767b115b7dbaefd11777bd541ec07f78cf2da797` |
| `source/game/StarTerrainDatabase.cpp` | 4603 | `bb7f1087599534561eb6276244685b04d051dc6e7f5f5810526acc61f944f4d7` |
