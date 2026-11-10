# H0: builder terrestre e dez verificações adicionais

Data: 10/11/2026 UTC. Branch `codex/experimental-regional-runtime`; base `fe51d2513de39733dae0ff5fc99d029921cb61b1` de Experimental.

[Região real](sector-runtime-h0-real-region-builder-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

## Estado: em validação

O alvo opt-in executa `WorldLayout::buildTerrestrialLayout` real, com cinco camadas, Root, bancos e arquivos mínimos de fixture. A geração no jogo, os saves e a matriz de mods/multiplayer ainda não foram validados; estes resultados não aprovam a nova arquitetura ou demonstram ganho de setores/s. São dez verificações técnicas consecutivas, não dez contratos completos do roadmap.

1. O builder de região com Root ativo consome uma seed global no sucesso.
2. A falha de seletor também preserva o consumo de uma seed.
3. Três tarefas constroem seletores privados reais a partir dos parâmetros capturados.
4. O builder terrestre produz dois layouts equivalentes e BaseTiles 32×32.
5. A serialização/reconstrução Json do layout preserva amostras e BaseTiles.
6. O metadado de oceano produz os líquidos esperados nos tiles privados.
7. Um patch real de asset altera o seletor cacheado como esperado.
8. A falha de seletor ausente deixa o banco utilizável em consulta posterior.
9. Três layouts privados continuam consultáveis após destruir template e Root.
10. O bioma ambiental de região secundária segue a configuração do asset após hotReload.

O teste de RNG bloqueia temporariamente a manutenção do Root por acesso de teste, somente após carregar os bancos, pois essa thread adiciona entropia em paralelo. Um friend no header permite esse controle; não modifica o runtime. Os layouts de teste são privados e usam seeds explícitas. A comparação de BaseTiles usa os mesmos kernels nas duas rotas: não é referência independente da geração inteira.

Duas verificações adicionais comparam `buildRegion` atual contra o corpo herdado da revisão `f22cf9c9`, anterior à integração do snapshot. O teste usa bancos e fixtures reais, compara Json de região/bioma, parâmetros e amostras de seletores, além do próximo RNG após sucesso e após falha com registro parcial. A referência preserva o algoritmo herdado, com adaptações para acesso por friend de teste. Ela cobre a região, não compara independentemente o builder terrestre inteiro.

O builder terrestre usa mundo 64×64, seed 42, materiais e biomas mínimos, além de configuração e patch locais. Não há logs, saves ou configuração do jogador nesta coleta. Não é um planeta com catálogo/mods reais, teste de save binário, rede ou benchmark. Próximo passo: ampliar a comparação independente da região para o builder terrestre completo e RNG/ordem sob assets e mods representativos; depois compor ownership/identidade/admissão e testar execução em sombra e streaming. Preservar fallback CPU; NPC externo suspenso e Linux Clang adiado.

Validação: 764 casos padrão e 71 opcionais passaram com GCC 14/C++17 e ASan/UBSan; os dois testes de referência passaram em cinco repetições, com shuffle e seed inicial 1242. A bateria anterior com dez verificações passou 42 grupos CTest; três microexperimentos permanecem desabilitados. GUI e Tracy estavam OFF. O identificador do cache é `9c1ccbbd-selector-lifecycle-local-dirty`, anterior à base: não representa pacote distribuível desta revisão. O jogo completo, outras plataformas e Actions não foram executados. Duas compilações foram iniciadas simultaneamente por engano; a duplicada foi interrompida, o registro do Ninja recomposto e o alvo reconstruído em sequência antes da bateria aprovada.

CPU-01/H0/RAM-01 permanecem em 25%; programa 7/196 =3,6%, pois ownership, admissão, integração e aceitação completa seguem abertos. Nenhum ganho de throughput é alegado.

Fontes locais de 10/11/2026 UTC, bytes e SHA-256:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarRoot.hpp` | 10689 | `6bde93ac67d2abbf6df1e2dff046fed52c1c625973f32cc4011e770b35df8c3a` |
| `source/test/CMakeLists.txt` | 11818 | `ea478864a25f116948575368cea7e8bc38e0a1f40b162c28b1686f0b024b30a5` |
| `source/test/world_generation_real_block_info_test.cpp` | 58812 | `029e016b00ff492b2fa30e764d2832abdc41a6e99488366a9bf3b92cee429a2d` |
