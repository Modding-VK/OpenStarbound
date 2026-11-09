# H0 / RAM-01: Json dos variants nas distribuições enfileiradas

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `ad6c5693de49295ab36846b53eac611f499fbdc3`.
Implementação: `49a12fbf4e0e47aa82ad60c6d10a2d98513ed5ca`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-json-storage-2026-11-09.md)

## Objetivo e escopo

Cobrir o Json imutável retido nos variants das distribuições enfileiradas,
sem cobrar o mesmo storage novamente a cada cópia, captura ou consumidor.
O acompanhamento começa na adoção da entrada original e segue o bundle
compartilhado até seu último consumidor físico, inclusive workers após retirada
da fila ou fechamento do facade chamador.

É um recorte de RAM-01, não nova arquitetura de execução ou ganho de setores/s.
Não muda configuração/defaults, admissão, workers, TTL/níveis, RNG, predicados,
sort, IDs/promises, geração de mundo ou publicação serial. Não altera campos/
representação de BiomeItem, Json, Perlin ou TileDamage e seus formatos de save.
Os wrappers diagnósticos ganham um handle; o sizeof real do bundle continua
coberto pelo allocator rastreado que o constrói.

## Raízes observadas sem copiar os itens

| Variant | Raízes Json |
| --- | --- |
| GrassVariant | descriptions |
| BushVariant | descriptions |
| TreePair | stemSettings, foliageSettings, descriptions, stemDropConfig e foliageDropConfig de cada árvore: até dez raízes |
| ObjectPool | parâmetros Json de cada item ponderado, sem selecionar ou alterar pesos |
| MicroDungeonNames / TreasureBoxSet | nenhuma raiz Json |

`collectGenerationJson` enumera as duas listas físicas, aleatória e ponderada,
mesmo se o sampler usar apenas uma delas. Não chama allItems/toJson, não copia
BiomeItem/Perlin e não acessa Root/assets, Lua ou mundo vivo. Os novos métodos
são leitores dos valores; os sizers noexcept anteriores continuam separados.

Cada raiz retida é apenas um handle Json compartilhado. Primitivas não têm
backing próprio e são ignoradas nessa coleta de raízes. Não é criado JsonArray
artificial nem cobrado um container sintético como dado do jogo. Distribuições
sem raízes de heap não alocam o tracker de floresta; medição desligada retorna
antes de enumerar. Uma coleção vazia truncada ainda produz diagnóstico explícito.

## Floresta limitada e identidade única

O mesmo walker da captura de template mede a floresta com um único visited e
um único limite de 32.768 valores, incluindo raízes/referências repetidas e
descendentes, não um limite novo por campo ou por distribuição. A coleta armazena
no máximo esse número de raízes de heap. Ao encontrar outra raiz além do limite,
interrompe a enumeração e marca rootsComplete/complete falsos.

As raízes admitidas ocupam o allowance antes de expandir descendentes. Se um
objeto não puder ter todas as chaves medidas, sua cobrança inteira é omitida
conforme Json v2, sem registrar bytes parciais conflitantes. Filhos que couberem
continuam medidos; inputs de geração não são filtrados. A captura de um único
Json conserva a API, fórmula e semântica anteriores.

Registro processual/classe 2 não mudam: controle de ownership deduplica entre
campos, distribuições, bundles e o template quando compartilham realmente o
mesmo backing. Conteúdo igual em owners independentes continua distinto. Json
não é somado novamente às classes 9/12/13 dos heaps conhecidos.

O limite restringe raízes e valores da floresta, não tempo de CPU ou número
total de itens sem Json examinados ao enumerar a entrada. Iteradores hash
podem percorrer slots vazios; alocações/locks também custam. Os vetores de
handles, pending/visited, tokens e control blocks diagnósticos não recebem
contabilidade profunda própria nesta etapa. Não é orçamento hard de toda RAM.

## Transferência de ownership e falhas

1. Com profiling regional ativo no enqueue, coletar/medir uma vez antes de
   mover os containers originais para o cache. É observação após construção,
   não reserva antes da criação do Json pelo chamador.
2. O cache mantém o handle junto da entrada original. Na primeira captura,
   executar a mesma única cópia snapshot-equivalente das distribuições.
3. Json dessas cópias conserva os owners. O bundle recebe **o mesmo handle**,
   sem segunda travessia, serialização ou cópia de itens para medir.
4. Somente após construir/publicar o bundle, destruir o source original e
   liberar suas parcelas. Capturas seguintes compartilham o bundle e o handle.
5. Retirada da fila/cancelamento lógico não libera a cobrança enquanto houver
   snapshot/worker com referência ao bundle. Valores são destruídos antes do
   acompanhamento; raízes da floresta são destruídas antes dos tokens.

Falha ao copiar/construir o bundle preserva o original e seu handle para retry.
Recusa do ledger ou truncamento marca a captura incompleta sem recusar um
input por esse diagnóstico; bad_alloc real da instrumentação pode propagar,
como nos outros trackers. O ledger runtime continua sem novo teto finito.
O mecanismo genérico pressupõe que o handle corresponda aos valores imutáveis
observados; não autoriza anexá-lo a dados mutados ou independentes.

Não há medição retroativa de pedidos já enfileirados sem acompanhamento nem
varredura por tile/candidato. A cobrança Json continua condicionada ao profiling
regional, ao contrário das classes básicas que já eram independentes dele.

## Versões e telemetria

Modelo geral **9**, Json **2** mantido e novo submodelo de Json das distribuições
**1**. Modelos próprios de distribuições **3**, candidatos/temporários **2** e
nomes/buffers/cache de worker **1** mantêm suas fórmulas. A cobertura adicional
aparece na classe processual de Json compartilhado, sem criar classe nova.

Prefixo `generation.memory.distributions.`:

- jsonSizing: duração da coleta/medição no enqueue com medição ativa.
- jsonModelVersion: máximo 1; jsonCaptures: florestas anexadas a entradas aceitas.
- jsonRoots: raízes retidas, não owners/bytes únicos; jsonVisitedValues e
  jsonObjectKeys: esforço acumulado das capturas.
- jsonOmittedObjects: objetos alcançados mas omitidos pelo allowance de chaves.
- jsonRootCollectionIncomplete: capturas com enumeração de raízes truncada;
  não é quantidade exata de raízes omitidas.
- jsonIncomplete: capturas incompletas por limite, overflow ou recusa.

Inputs sem heap Json não incrementam jsonCaptures. O timing cobre a tentativa
diagnóstica, inclusive a enumeração vazia. Contadores são agregados, sem nomes
ou conteúdo de mods. Capturas de uma entrada não significam setores entregues.

## Validação

Ambiente cloud conferido pela skill operacional, sem instalar/baixar dependências.
GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/allocators alternativos OFF e
`VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`ad6c5693-variant-json-local-dirty`; fontes do commit/manifesto abaixo.

Versão final com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **320/320 testes**, dezenove suites, **26 novos** de floresta/variants/lifetime.
2. Os novos casos repetidos vinte vezes, embaralhados com seed GTest 1220:
   **520 execuções adicionais**, não 520 casos únicos.
3. **19/19 grupos CTest** `^world_generation_contract_`, NoAssets; repetem casos
   do binário, não são outros 320 testes distintos.
4. Build dos contratos e quatro objetos reais: geração, template, sampling e
   estimador Json. Sem linking completo do jogo.
5. Sintaxe/tipos de geração, estimador, sampling e dois testes modificados
   aprovados com `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2
   existentes. Não é build/linking Tracy completa. Aviso herdado de fmt e nota
   GCC de ABI no fixture de alinhamento 64 permanecem.
6. `git diff --check` aprovado; **420 links locais em 32 documentos** sem
   destino ausente, 14 módulos alcançáveis e 49 contratos únicos cobertos pelo
   roadmap. Ordem 1–49, oito totais de fase e 6/196 marcos = 3,1% conferidos;
   RAM-01/H0 mantidos em 25%. Dez fontes do manifesto conferidas em bytes e
   SHA-256. Nenhum teste manual do jogo foi presumido.

Cobertos: floresta vazia/primitivas, roots repetidos/independentes, sharing com
template, limite global de raízes/descendentes, retenção física, recusa finita,
26 casos novos incluindo todos os seis variants, ambas as listas físicas,
medição desligada/caminho sem heap, transferência sem rescan, retirada da fila,
falha de cópia/retry, move de cache, WorkerPool após fechar facade e oito bundles
privados concorrentes. São testes de ASan/UBSan, não de TSan/OOM real.

Sampler nativo de entrada mista comparado à cópia de referência em 561 posições,
preservando coordenada/prioridade, ordem de nomes e escolhas em 64 seeds nos
casos micro. Seleção ponderada do ObjectPool também confrontada em 64 seeds.
Suites anteriores, inclusive bytes serializados, replays de 96/432 candidatos
e controles negativos de hash/wrap, passaram. Não é streaming com assets/mods
nem prova integrada de save/multiplayer.

Reprodução, após configurar build equivalente, a partir da raiz:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o \
  game/CMakeFiles/star_game.dir/StarBiomePlacementSampling.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldGenerationMemoryEstimate.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationVariantJsonTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1220 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites, progresso e continuação

Nenhum config/log/Tracy novo fornecido/coletado ou captura antiga reanalisada;
nenhum arquivo do usuário alterado ou dado bruto adicionado ao Git. Não é
execução de jogo/assets/mods, save/reload, multiplayer, TSan, OOM real, benchmark
de throughput/overhead ou matriz Windows/macOS/ARM/allocators alternativos.
Sem Actions, release, merge em main ou revisão independente.

O escopo implementado é **distribuições enfileiradas e consumidores do seu
bundle**. Não é acompanhamento geral de Json de candidatos ambientais, layouts,
biomas ou todo o template privado do worker. Variants permanecem opacos no
modelo raso; nós TileDamage/malloc, decode e picos pré-adoção seguem parciais.
Referências comuns/assets podem reter Json após o último tracker: saldo zero
não prova desalocação física global. SSO pode superestimar.

RAM-01/H0 continuam em **25%**, programa **3,1%**, sem outro marco cumulativo
integral. Próximo: consolidar envelope coberto/exclusões e política de pressão
para o escopo antes de limites por hardware/configuração. Classificar Json
ambiental/privado e nós/allocator como cobertura ou exclusão explícita; não
exigir todo heap do motor para iniciar H1 em sombra nem ampliar filas com
estimativas parciais. VAL-02/MOD-01 e baseline integrado continuam gates para
publicação/ganho, priorizando setores comuns e hardware sem modelo de CPU fixo.

Mods/saves/multiplayer e fallback CPU continuam essenciais. NPC externo
suspenso e Linux Clang adiado; pendências CPU-01 de seed automática/Perlin
Uninitialized não foram corrigidas pelo acompanhamento de Json.

## Manifesto das fontes

Fontes locais coletadas/conferidas em 09/11/2026, nomes relativos ao checkout,
tamanho/SHA-256. Sem novos artefatos do usuário.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarBiomePlacementSampling.cpp` | 7274 | `85174f1c7077cfd179c09a17960465a95e5fe806fdcbf2c64b26d50f56aa1d17` |
| `source/game/StarBiomePlacementTypes.hpp` | 3620 | `f8cdb9d472bf60755a0620fbbb2059155738c77f8beb2a701cb9e67944fae334` |
| `source/game/StarWorldGeneration.cpp` | 127379 | `3846581b90a2cc2df0ca39a86f42353fcfb104e8740d0d22df871876a2895ce4` |
| `source/game/StarWorldGenerationImmutableArray.hpp` | 5294 | `2f91e0874ccd149bc0d884fb751f798d999445d6ed80c24ac5482babdff02412` |
| `source/game/StarWorldGenerationMemoryEstimate.cpp` | 4468 | `752b59ce83e1992230990797a10a27ba4c9957ff52f80d084a5f1d51814da90e` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 2827 | `697a65cd0fd7abe9d9487ee94f8bc3c917b305fe0698cc4e42f0338eb7cb8368` |
| `source/test/CMakeLists.txt` | 5566 | `76b3559a48b526b6a353756b10f37282dc5cdec15b859aa8ec14e700c6405a68` |
| `source/test/world_generation_distribution_test.cpp` | 29300 | `f501d3d6158a08fb711540932128e698ee26b2e834d6483e0927e5fc7232db12` |
| `source/test/world_generation_estimate_test.cpp` | 11802 | `f968b157acc2e3847f72bdedeb555ede1e0203c795a5f34e65b9d48c4af9c6a4` |
| `source/test/world_generation_json_storage_test.cpp` | 19223 | `f0ba6cccf4f0c2c1e735c0365903744cab123246dc6da000cc57f18ffbb11dd6` |
