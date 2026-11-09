# H0: catálogo compartilhado e compacto de consultas BaseTiles

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `953a7644a8d51dac8ba7c9fecbc18fdca26beeed`.
Implementação: `4e5cc6c1852802ec4eba022de48afbcd9cfd171b`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Memória](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-base-tile-observations-2026-11-09.md)

## Objetivo e estado entregue

Substituir a retenção de uma fita por tile por um catálogo de respostas únicas,
com capacidade explícita e uso imutável compartilhado. WorldGenerationBaseTileLookupContext
e WorldGenerationBaseTileLookupScope são **protótipos H0 modelo 1**, header-only,
testados sem assets. O produtor normal não os chama: sem nova config, cache de
produtos, fila, replay de tarefa, alteração de orçamento ou escrita paralela viva.

Cada resposta ocupa um uint64_t: chave injetiva de tipo(2 bits), biome(8),
material(16), mod(16), mais valor(8), no máximo 50 bits. Não é hash. Entradas
ordenadas permitem busca binária, sem colisão ou fallback por igualdade de hash.
O catálogo não retém inputs de tiles, observações originais, Root, template,
ponteiros, callbacks, exceções ou heap próprio. Cópia do objeto é proibida;
compartilhamento por referência const exige lifetime e publicação sincronizada
pelo dono. Construção não é concorrente; depois de seal só há leitura.

Capacity é parâmetro explícito do tipo, não default ou limite novo do jogo. O
objeto contém todo o backing reservado, mesmo quando poucas entradas estão ativas.
Ele não cresce, aloca ou move arrays de tiles. Inserção ordenada pode deslocar
entradas: O(U) por chave nova, onde U é o número de entradas. Aplicação custa
até seis buscas O(log U); catálogos grandes/construção bulk ainda não foram medidos.

## Escopo, construção e aplicação

Scope exige worldEpoch, assetsGeneration e materialsGeneration não zero;
templateRevision zero é permitido. Também carrega versões de scope/input/líquido.
Admissão de amostra e aplicação exigem igualdade dos sete campos. Não inferir
gerações de endereços de ponteiros, igualdade de valores ou ausência de conflito.

**Essas gerações são declarações do chamador, não contadores reais conectados ao
motor nesta entrega.** WorldTemplate expõe generationRevision e tasks têm epoch;
Root oferece reload/listeners/hotReload e Assets::hotReload é const. Não foi ligado
um produtor de gerações de assets/material ao catálogo, nem feita auditoria completa
de mutações. IDs de testes não detectam reload real. Sem isso, o protótipo não é
cache seguro de produção ou fechamento completo de CPU-01.

add recebe observação completa/estruturalmente válida e escopo. Primeiro verifica
todas as entradas e duplicatas, depois insere as novas. Escopo/amostra inválidos
ou capacidade insuficiente não inserem parcialmente. Duplicata igual é reutilizada;
valor divergente para a mesma chave, inclusive dentro da mesma amostra, invalida
permanentemente o builder. Não é permitido selá-lo ou ignorar o conflito e usar
seu primeiro resultado. Depois de seal qualquer add é recusado sem mutação.

apply verifica scope/estado selado e resolve **todas as chaves necessárias uma só
vez**, antes de escrever biomas, camadas ou líquido. Só solicita hue/suporte/mod
hue das camadas inicialmente NullMaterialId, respeitando o resultado de suporte.
Até seis bytes locais alimentam o adaptador anterior na mesma ordem. Se faltar
a última chave do background, nada terá sido escrito; o chamador pode escolher
o caminho legado a partir do estado intacto. Não há consulta ao resolver vivo
ou fallback interno depois de escrever.

NoMod não recebe atalho implícito: suporte/hue precisam estar cobertos quando
seriam consultados. Mod rejeitado conserva mod/hue anteriores. Líquido usa os
materiais finais e conserva Keep/float bits. Os demais campos passam intactos.
O domínio aritmético do líquido continua o herdado: diferença cabe em int.

**Diferença da fita:** catálogo aplica aos dados atuais do tile privado exclusivo,
não confere os 14 campos de um tile original específico. Inputs, y e estado
prévio não ficam retidos nele. Cobertura não prova que um tile é snapshot fresco,
nem autoriza retry sobre estado parcial. Ticket/epoch/revisões, read set e guards
de commit continuam separados; catálogo não é hash de conteúdo ou autoridade.
Conflitos observados recusam casos impuros, mas amostras consistentes não provam
um resolver arbitrário puro ou que chamadas vivas atuais não lançariam exceção.

## Validação executada

Ambiente conferido com a skill cloud, executor pronto/política aplicada. Mesmas
ferramentas/dependências/cache da etapa anterior, sem instalação/download:
GCC 14.2.0/C++17, CMake 3.29.2/Ninja 1.13, Debug -Og, GUI/Tracy/allocators
alternativos OFF, BUILD_TESTING=ON, VCPKG_MANIFEST_INSTALL=OFF. Cache
`/tmp/sector-input-h0.RX3XLi`; identifier
`953a7644-base-tile-lookup-context-local-dirty`. Hook SDL3/sysroot externo ao Git
mantido conforme o relatório de inputs; sem dependência nova do projeto.

C++ com -fsanitize=address,undefined -fno-omit-frame-pointer e XXH_NO_INLINE_HINTS=1;
C/dependências pré-compiladas não têm a mesma instrumentação. Com
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e ASAN_OPTIONS=halt_on_error=1,
sem diagnóstico dos sanitizers:

1. **601/601 testes**, 29 suites; **26 novos** normativos. Dois experimentos
   DISABLED não fazem parte desses 601.
2. **117 casos afetados × 20**, shuffle/seed GTest 1229: **2.340 execuções
   adicionais**, não casos únicos.
3. **29/29 grupos CTest** `^world_generation_contract_`, NoAssets, repetindo os
   casos normativos do mesmo binário; experimentos fora do CTest.
4. Build/link NoAssets aprovado; StarVersion/teste atualizados. Sete objetos reais
   de geração/produto/TileDamage/TileDamageState/WorldTiles/WorldTemplate/WorldStorage
   conferidos sem trabalho pendente, não reconstrução forçada de todos nem link
   completo do jogo.
5. Sintaxe/tipos de geração, produto/comparador e teste BaseTiles aprovados com
   STAR_ENABLE_TRACY/TRACY_ENABLE e headers oficiais já existentes de Tracy
   0.11.2, commit b8c2e25c3d02009059e09f623072921677dd6bb4. Não linking Tracy;
   aviso herdado de fmt permanece. Sem código externo incorporado.
6. `git diff --check` aprovado; **602 links locais em 45 documentos**, 14 módulos
   alcançáveis, 49 contratos únicos cobertos no roadmap e ordem 1–49 conferidos.
   Oito totais de fase e 6/196 marcos = 3,1% recalculados; RAM-01/H0 mantidos em
   25%. Quatro fontes do manifesto conferidas em bytes/SHA-256.

Cobertura: matriz de 4.096 branches compara catálogo, fita e corpo anterior
independente do projeto, com campos completos e trace de captura/referência.
Casos de NoMod/ar/mod rejeitado, bit patterns de líquido, zero capacidade,
capacidade insuficiente sem inserção parcial, deduplicação, limites de chaves,
ordem de amostras, conflitos dos três tipos de consulta e por posição, sete
campos de escopo e amostras malformadas. Uma chave final ausente recusa sem
qualquer write; seguir pelo legado reproduz a mesma exceção/estado parcial.
Um catálogo selado atende 1.024 tiles iniciais distintos em quatro workers,
comparados à referência serial. Não são quatro templates reais em paralelo.

Reprodução após configurar build equivalente conforme os relatórios anteriores:

```bash
cmake --build <build-dir> --target world_generation_contract_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*:WorldGenerationBaseTileLookupContextTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1229 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Custo e memória: experimento local

Experimento novo executado separadamente com checksum aprovado. 8.192 operações,
nove rodadas por modo, ordem rotativa dos três modos de aplicação. Alocação/
inicialização de tiles fora da medição; resolver trivial, mesma fita compartilhada
e catálogo de seis chaves. Replay/catálogo acumulam booleano de sucesso. Construção
mede inicializar/add/seal de um catálogo por operação a partir da amostra **já
capturada**, não o custo de obtê-la. Debug/ASan/UBSan no executor x86_64.

| Modo | Mediana ns/operação | Mínimo | Máximo |
| --- | ---: | ---: | ---: |
| Adaptador direto | 204,92 | 123,23 | 323,44 |
| Replay da fita compartilhada | 779,81 | 543,50 | 1.122,96 |
| Catálogo: preflight e aplicação | 614,42 | 489,38 | 1.333,41 |
| Construir/selar catálogo de seis chaves | 828,86 | 631,79 | 1.558,16 |

Não comparar esses números a runs anteriores como série A/B. O catálogo fica
abaixo da fita nesta mediana, mas acima do adaptador com resolver trivial; há
variabilidade e não foi feito estudo estatístico. Não mede Release, WorldTemplate/
blockInfo, custo de cache miss/reload, hardware do usuário ou setores entregues/s.

Nesta ABI, Context<32> tem **320 bytes totais**, **256 bytes reservados** de entries
e **48 ativos** para seis chaves. Capacity=32 é escolha do experimento, não default.
Uma instância compartilhada substitui 1.024 fitas de 176 bytes (180.224 bytes/
176 KiB) **somente neste cenário de seis consultas únicas reutilizadas**. Escopos
com mais chaves precisam de outro dimensionamento; a origem/output dos tiles e
overhead dos donos não entram nessa comparação. O protótipo não retém nem cobra
memória no jogo. Uma integração deve reservar/contabilizar o objeto e donos antes
de alocar; não considerar só bytes ativos ou multiplicar o mesmo backing por leitor.

```bash
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_also_run_disabled_tests \
  --gtest_filter='WorldGenerationBaseTileLookupContextTest.DISABLED_SyntheticLookupContextCosts' \
  --gtest_brief=1
```

## Continuação, compatibilidade e progresso

Próximo recorte executável: conectar gerações reais e lifetime de assets/material
às fronteiras de reload/hotReload, auditando quando as respostas de consultas
mudam. Vincular epoch/revisão de template e rejeitar contexto obsoleto antes de
preparar/aplicar; manter origem viva ou identidade sem ABA. Construir/comparar o
catálogo a partir dos leitores reais, com cobertura/misses explícitos e exceções
originais no caminho legado. Só depois um protótipo em sombra por setor, com
admissão/bytes e comparação, sem publicar resultados divergentes.

Isso não exige contabilizar cada heap do motor antes de H1 delimitado. Contexto
profundo de blockInfo, demanda, mailbox/DAG, provider/fairness e streaming integrado
permanecem gates. Não ampliar filas/defaults, repetir callbacks ou converter uma
falha em fallback depois de escrever. Não ativar catálogo só por build verde.

RAM-01/H0 continuam **25%**, programa **3,1%**, 6/196 marcos dos 49 contratos.
CPU-01 ainda não tem contexto completo real ou novo marco reconhecido; quantidade
de testes e redução sintética de storage não sobem notas nem demonstram throughput.
Contexto/scope novos modelo 1; modelos anteriores, 14 classes de memória, estimativa
9, retry 2 e pressão padrão desligada inalterados. Nenhuma cópia de tile, RNG,
API Lua, save, nível, TTL, pré-requisito ou regra de commit/publicação mudou.

Sem novos config/log/Tracy do usuário, reanálise, jogo/assets/mods, save/reload ou
multiplayer executados. Sem TSan/MSan, OOM/pressão externa, Windows/macOS/ARM ou
allocators alternativos. Achados biomeTransition, seed automática/Perlin seguem
abertos, sem causa de crash atribuída. Sem Actions, release, main ou revisão
independente. Mods/saves/multiplayer e fallback CPU permanecem gates essenciais;
NPC externo suspenso, Linux Clang adiado, dados brutos fora do Git.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes/SHA-256. Quatro
fontes alteradas; nenhum artefato novo do usuário recebido nesta entrega.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16911 | `ce4ad133382fba1dd39bef2d5be7ccd11268bb506af7a50ed1ff4157e0999cb2` |
| `source/game/StarWorldGenerationBaseTileLookupContext.hpp` | 7324 | `4bb706f5936e20615c2d7ba5ad096b3f60edaa62eaf2f47a9b37da3b52bab9b3` |
| `source/test/CMakeLists.txt` | 6431 | `f5b20f6233064853ce34ec708e814ebdef14c2a7c8cf0c016ca15617ff825300` |
| `source/test/world_generation_base_tiles_test.cpp` | 79339 | `832bc20052571fc972d198cfd4629ead5fbae33741e2c81afa854be29c04a699` |
