# H0: builder real de região e dez cenários

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`; base `1db27f5b9dd8def73cf152edd3dc82940adb5003` de Experimental.

[Etapa anterior](sector-runtime-h0-region-selector-factory-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

O alvo opt-in chama `WorldLayout::buildRegion` real com Root, TerrainDatabase, BiomeDatabase, MaterialDatabase e arquivos mínimos locais. Dez cenários cobrem seletores nomeados/seeds, bioma/subbloco, BaseTiles 32×32, falha com escrita parcial, oceano, recuperação após erro, patch de asset, CacheSelector, seeds extremas e leitura após destruição de Root/template. O patch usa o mecanismo real de assets, mas não equivale à matriz completa de mods.

Para permitir a ligação com ASan/UBSan, as três funções reais de registro e construção de região foram relocadas para `StarWorldLayoutRegion.cpp`; as três factories reais de entidades de planta foram relocadas para `StarPlantDatabaseEntities.cpp`. As seis definições coincidem byte a byte com a base, e o conteúdo restante dos arquivos originais difere apenas por linhas vazias. SHA-256 das seis definições concatenadas com newline: `f866ef048cd8fce4f4c6a9da16b9da6aad9b1d6b65c9591a14bf0080487c366a`. Ambas as novas unidades entram na build normal. Os objetos normais envolvidos compilaram; o jogo completo não foi ligado ou executado.

Os cenários usam região simples 64×64, altura 16, dados e seletores mínimos. A comparação de BaseTiles usa o mesmo kernel nas duas rotas, portanto não é uma implementação independente do gerador inteiro. O construtor de Root mantém sua thread de manutenção: não se afirma equivalência da sequência global de RNG durante esse ensaio. O teste inicial de oceano esperava hue 9 da fixture sintética anterior; o bioma real mínimo tem hue 0. A expectativa foi corrigida e a bateria aprovada novamente.

Os dez cenários são **um recorte de validação**, não dez contratos do roadmap concluídos. Ainda faltam builder procedural completo de planeta, mundo novo/salvo, matriz representativa de mods e multiplayer, comparação integrada de RNG, contexto proprietário e sua admissão, workers e benchmark. Não há ganho de setores/s demonstrado. Próxima etapa: exercitar o builder procedural completo e comparar sua saída, preservando RNG, efeitos e ordem; então compor identidade/ownership/admissão antes de sombra por setor. Compatibilidade e fallback CPU seguem essenciais; NPC externo suspenso e Linux Clang adiado.

Build diagnóstico local GCC 14/C++17, ASan/UBSan, GUI/Tracy OFF, Root opt-in. O identifier do cache `9c1ccbbd-selector-lifecycle-local-dirty` é anterior à base, portanto não identifica pacote distribuível desta revisão. A bateria padrão passou **764 casos**, a opt-in **59 casos**, e **42 grupos CTest** passaram. Três microexperimentos permanecem desabilitados. O primeiro link expôs dependências reais dos bancos e RTTI de plantas; as unidades de produção necessárias foram incluídas sem stubs ou desativação de sanitizers. Nenhuma captura/configuração do jogador foi recebida; Actions não foi executado.

Fontes locais, 10/11/2026 UTC, bytes e SHA-256:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldLayout.cpp` | 29595 | `5aaa321cbae6d0debc2daf7bc05bc53ebd870686827f08070164d2fb077acfa9` |
| `source/game/StarWorldLayoutRegion.cpp` | 3546 | `b5151299ee65cdad90f85cf416c0c6066aceed91f1612e2780ee9bd5942d196e` |
| `source/game/StarPlantDatabase.cpp` | 12714 | `5c730bc7833f95b690405bbed2c371867b07e0960d2005da4fb4355e2036f827` |
| `source/game/StarPlantDatabaseEntities.cpp` | 1105 | `4fa33c5cf81f0bc573353266b4e5076ad9fadc643bfcd4565a56c455ddfdba15` |
| `source/test/world_generation_real_block_info_test.cpp` | 43501 | `5040cfdfe654ec392a2a6afb766d828865c69ae7c70ad24fa0558aa99b039d30` |
| `source/test/CMakeLists.txt` | 11721 | `7a65a9bbb7ad6f818b36316a24486fc0b2b6ea86af1e859d15bc8b0e8e7ea3b4` |
| `source/game/CMakeLists.txt` | 17538 | `ac952f9e9a19529826b177828ae47bdbf825f128e4e6d43390b8e6bc123d7da0` |
