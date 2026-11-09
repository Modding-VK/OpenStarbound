# H0 / RAM-01: templates compartilhados e estimativas de retenção

Data: 09/11/2026. Branch de entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `64daf72571ea84725f86d2e84a19dae12d653fd1`.
Implementação: `803d4cf7683d210f2df9df41feba17dd3e3a7c43`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Recorte anterior](sector-runtime-h0-process-memory-2026-11-09.md)

## Objetivo e resultado

Continuar RAM-01 identificando storage compartilhado de templates serializados,
metadados de jobs e resultados variáveis. Separar estimativas de containers das
reservas de arrays, antes de definir orçamento global ou ampliar admissão.

Implementado o modelo de tamanho **v1**, com deduplicação por proprietário do
storage imutável de Json e cobrança retida por handles de inputs, caches e
resultados. São estimativas parciais, não medição de RSS, allocator ou limite
superior de heap. O ledger processual permanece observacional; nenhum novo
teto, preview, worker ou padrão de configuração foi ativado.

Este recorte prepara uma política de RAM mais verificável. **Não é uma
aceleração medida da entrega de setores**, nem conclui RAM-01/H0.

## Implementação e identidade

`WorldGenerationSizeEstimate` faz soma/produto verificados e declara a versão
do modelo. `captureWorldGenerationJson` percorre iterativamente os nós
imutáveis já expostos pela API pública de Json, sem alterar seu layout, formato
serializado ou conteúdo. O limite diagnóstico é 32.768 valores por captura,
incluindo referências repetidas; o trabalho e a fila temporária são limitados.
Uma subárvore já vista não é expandida novamente.

`reserveSharedJson` usa `owner_less` sobre o control block de `shared_ptr`:
alias pointers do mesmo proprietário contam uma vez; conteúdo igual em
storages independentes continua distinto. A identidade não é apenas o endereço
nem o hash do conteúdo. Capturas de raízes diferentes compartilham a cobrança
dos mesmos nós; storages realmente distintos de duas serializações continuam
separados. O registro do ledger mantém referências fracas, não prolonga a
vida de payloads e remove a entrada no último token rastreado.

Aquisição concorrente e última liberação têm tratamento explícito: uma
entrada cujo token expirou pode ser substituída antes de seu destrutor obter o
mutex; o token anterior só libera se ainda for a identidade registrada. Isso
evita dupla cobrança/liberação e recusa falsa no teto finito do teste.
Referências temporárias fortes sobrevivem ao escopo do lock, inclusive na
recusa por modelos conflitantes; seu último destrutor não roda sob o mesmo
mutex. Inserções que podem alocar antecedem a cobrança.

O tracker possui o Json e seus tokens, destruindo o Json primeiro. O cache
de `WorldGenerator` publica fonte/revisão somente após a captura bem-sucedida:
uma falha de construção não rotula um store antigo como uma revisão nova.
O cache privado do worker continua reconstruído a partir do mesmo Json e não
recebe artificialmente um handle rastreado para prolongar o input serializado.

## Modelo v1 e fronteiras de cobertura

| Grupo | Fórmula declarada e lifetime |
| --- | --- |
| Json string | `sizeof(String) + utf8.capacity() + 1`, uma cobrança por storage rastreado |
| Json array | `sizeof(JsonArray) + capacity × sizeof(Json)`; primitivas inline não criam outro nó de heap |
| Json object | `sizeof(JsonObject) + size × sizeof(value_type)`; apenas representações ocupadas |
| Pendência BaseTiles | `sizeof(PendingBaseTiles)`, até retirada do registro |
| Snapshot assíncrono do planejador | `sizeof(snapshot)`, revisões ocupadas, capacidade de referências ao halo, colocações e suas listas de distribuições; até último dono do snapshot |
| Pendência do planejador | `sizeof(pending)` e entradas ocupadas de sua cópia do mapa de revisões; até retirada do registro |
| Resultado assíncrono do planejador | `sizeof(result)`, capacidade da lista de planos e capacidade UTF-8 + terminador de cada nome; até último dono do token/resultado |

São grupos distintos das reservas já existentes de arrays BaseTiles/halo.
A estimativa da lista de referências ao halo não cobra novamente seus arrays.
As duas cópias físicas do mapa de revisões são estimadas separadamente.

Limitações importantes:

- `String` pode usar SSO: contar o objeto e capacidade pode superestimar.
  Buckets não ocupados de mapas, buffers heap das chaves Json, control blocks,
  alinhamento, allocator, tokens e índices do próprio tracker não são medidos.
  Portanto o modelo não é nem medição exata nem limite superior conservador.
- Heaps internos de `BiomeItemDistribution`, cache privado de `WorldTemplate`,
  assets, caches de pré-aquecimento e decode de tiles salvos continuam fora.
  Metadados/resultados do caminho microdungeon síncrono também não recebem
  essas reservas adicionais; seu template serializado pode ser rastreado.
- Deduplicação cobre os **handles rastreados** do input serializado. Um Json
  comum, um asset ou uma referência interna pode manter backing após o último
  tracker sair. Saldo zero não prova liberação física de todo esse storage.
- Outputs variáveis são estimados depois de construídos. A atualização do
  token acompanha a retenção posterior, não reserva antes do crescimento nem
  mede o pico transitório de construção ou uma saída parcial que lançou exceção.
- Limite de travessia, overflow ou recusa da estimativa marcam cobertura
  incompleta; o Json inteiro e o resultado de geração permanecem intactos.
  `complete` significa cobertura de travessia/aritmética dentro do modelo v1,
  não cobertura de todo o heap. `bad_alloc` não é convertido em sucesso.

## Ativação e custo diagnóstico

As novas estimativas requerem `explorationProfiling.enabled: true` e
`regionalDetails: true` no perfil de servidor. Não há nova chave de config.
Sem isso, o template mantém seu backing normal, mas não há travessia profunda
nem tokens das novas classes. As reservas anteriores de arrays permanecem
ativas independentemente do profiling.

O cache serializado é medido uma vez por fonte/revisão em cada cache. Se um
handle antes não medido for solicitado com medição ativa, o Json é reaproveitado
sem nova serialização. Jobs antigos não são retroativamente medidos; retirar
a solicitação de medição não elimina tokens já retidos. Não foi acrescentada
uma interface de alteração de profiling durante o jogo.

A captura diagnóstica aloca estruturas limitadas e faz operações de ledger por
nó; não é gratuita. Seu custo exige A/B separado. O worker atualiza somente
seu token de resultado; não chama mundo, coletor ou singleton ao terminar,
mesmo que o facade original tenha sido destruído.

## Telemetria implementada

Os campos anteriores de arrays preservam sua semântica, inclusive
`sampledResourcesTotal`, que continua somando grupos de arrays. Internamente,
`reservedBytes` agora soma arrays e estimativas declaradas; os novos campos
`reservedArrayBytes` e `estimatedBytes` separam as parcelas. Não interpretar
esse total misto como RAM física nem somá-lo novamente às suas parcelas.

Campos novos sob `generation.memory.process.`:

| Campo | Interpretação |
| --- | --- |
| `sampledEstimatedBytesTotal` | soma amostrada das três classes estimadas |
| `sampledSharedJsonNodesTotal` | soma amostrada de nós Json compartilhados rastreados |
| `sampledTemplateJsonEstimateTotal` | parcela estimada de Json |
| `sampledMetadataEstimateTotal` | parcela estimada de metadados |
| `sampledPlanEstimateTotal` | parcela estimada de planos variáveis |
| `lifetimePeakEstimatedBytes` | pico estimado processual desde a criação do ledger |

Permanecem amostras no máximo uma vez por segundo por coletor; dividir somas
por `generation.memory.process.samples` dá média amostrada, não bytes alocados.
Coletores diferentes observam o mesmo processo: não somar suas séries como
memórias independentes. Ausência de amostra/estimativa não prova ausência de
uso. Pico vitalício não reseta por janela ou mudança de mundo.

Sob `generation.memory.estimates.`: `modelVersion` (máximo, atualmente 1),
`enabledSamples`, `templateMeasurements`, `templateIncomplete`,
`reservationIncomplete`, `planIncomplete` e tempos `templateSizing`,
`snapshotSizing`, `planSizing`. As chaves têm cardinalidade fixa, sem nomes,
endereços ou conteúdo dos templates. `templateSizing` exclui a serialização;
`planSizing` é devolvido pelo worker e registrado pela dona ao consumir o
resultado. O tempo total do worker inclui sizing. Resultados descartados antes
da aplicação não produzem essa amostra de tempo na dona; os tempos não medem
todo o overhead do ledger e não devem ser subtraídos como um custo completo.

## Validação executada

Ambiente cloud conferido pela skill operacional; reutilizadas ferramentas e
dependências locais, sem instalação. Build headless GCC 14.2.0/C++17 Debug
(`-Og`), ASan/UBSan, `STAR_BUILD_GUI=OFF`, `STAR_ENABLE_TRACY=OFF`, allocators
alternativos desligados. C++: `-fsanitize=address,undefined`,
`-fno-omit-frame-pointer`, `-DXXH_NO_INLINE_HINTS=1`; C mantém o workaround
`XXH_NO_INLINE_HINTS=1` já documentado. Dependências pré-compiladas/C não têm
a mesma cobertura dos sanitizers C++.

Identifier local: `64daf725-ram-estimates-local-dirty`; os hashes do manifesto
identificam o código validado e commitado em `803d4cf7`.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. Build CMake de `world_generation_contract_tests` concluída; **115/115 testes**
   em nove suites aprovados, incluindo 20 novos `WorldGenerationEstimateTest`.
2. Os 20 novos repetidos 20 vezes, embaralhados, seed GTest `1210`:
   **400 execuções adicionais** aprovadas, não 400 casos únicos.
3. CTest filtrado por `^world_generation_contract_`: **9/9 grupos NoAssets**;
   repetem casos do binário, não constituem outros 115 casos distintos.
4. Objeto CMake real `game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o`
   compilado; não houve linkedição completa do cliente/servidor.
5. Sintaxe/tipos de geração, ledger e estimator também aprovados com
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2 existentes;
   isso não é build/linking completa da variante Tracy.
6. `git diff --check` aprovado; 212 links locais em 19 documentos sem destino
   ausente, 13 módulos alcançáveis e 49 IDs únicos presentes no roadmap.
   Os sete arquivos do manifesto coincidem em tamanho e SHA-256.

Casos novos cobrem: parcelas/picos, crescimento/redução, recusa sem alterar
cobrança, arrays imutáveis, aliases, endereços iguais com donos diferentes,
modelos conflitantes, árvore compartilhada entre capturas/raízes, conteúdos
iguais independentes, primitivas, captura desligada, limite diagnóstico,
overflow, UTF-8, profundidade 512 sem recursão, WorkerPool real, fechamento do
facade, resultado pronto retido e saldo/registro final zero.
Há testes concorrentes de 8.000 ciclos por repetição para última liberação e
4.000 para modelos conflitantes, incluindo proteção contra deadlock. TSan não
foi executado; ASan/UBSan e stress não substituem sua cobertura.

Reprodução em build equivalente:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o -j2
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationEstimateTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1210 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Configuração observada, compatibilidade e limites

Não houve novo config, log ou Tracy fornecido/coletado nesta atividade; nenhum
arquivo do usuário foi alterado ou captura anterior reanalisada. O cenário
validado foi NoAssets/controlado, não um planeta real. Nenhum dado bruto foi
adicionado ao Git.

Seeds, ordem de RNG, TTL, níveis, pré-requisitos, admissão e commit serial
continuam iguais por construção; não foi alterado o contrato público de Json,
Lua, save ou rede. Equivalência de outputs com assets/mods, streaming real e
multiplayer ainda precisa ser demonstrada, não apenas inferida do diff.

Sem execução do jogo, Windows/macOS/ARM, benchmark A/B, TSan, build completa
ou Actions. Não há ganho de setores/s nem overhead de profiling medido.
O overflow da seed automática em `StarRandom.cpp:197` continua aberto em
CPU-01, sem alteração do RNG. NPC externo suspenso e Linux Clang adiado.
Não houve release, merge em `main` ou revisão independente nesta atividade.

## Manifesto de fontes e continuação

Fontes locais conferidas em 09/11/2026, SHA-256; binários e caches fora do Git:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldGenerationMemory.hpp` | 4.017 | `de86da99860ef9501d319ac582e90f10a4f0547027e7d45ec92f67f91de5ed4f` |
| `source/game/StarWorldGenerationMemory.cpp` | 6.999 | `4ffd21c1244c1ec5dce2da2e043f35b5b5fabebb8e022c8a5488d87be594f9a7` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 1.036 | `bf8628c348526b502e277bcdf142a85ed3f97145f45bcd6eb22eea4831702140` |
| `source/game/StarWorldGenerationMemoryEstimate.cpp` | 2.846 | `8df8bcf5c786157ded631a6ba33a1dc3c32ec45afe7fb0eb132f0008e271fa40` |
| `source/game/StarWorldGeneration.hpp` | 12.847 | `1a837dff0dbe82d7642c1613dd13dc2caa2a2341edeba50c95900d6d812f1024` |
| `source/game/StarWorldGeneration.cpp` | 119.432 | `d968c06bfab21ded26bab1d814026d4a06f70bd31dab7fb6d98b97a6b9afde77` |
| `source/test/world_generation_estimate_test.cpp` | 11.802 | `b94a1e9976faada12c9edf776f5ef08368c312360281b6679ba8adc0e0a2b876` |

Próxima sequência executável, ainda em H0:

1. Auditar retenção do `WorldTemplate` privado/cache por worker e containers
   ainda excluídos; definir medição de capacidade/allocator e cobertura sem
   depender do profiling antes de usar números para admissão obrigatória.
2. Cobrir construção/crescimento de outputs variáveis e os caminhos síncronos;
   distinguir estimativa de reserva pré-alocação. Só então definir cotas
   processuais por hardware/configuração, pressão e reação verificável.
3. Manter MOD-01 explícito e ampliar VAL-01/02 para streaming real e outputs
   com assets/mods; medir overhead e throughput em variantes equivalentes.

Não ampliar admissão com a estimativa parcial. O inventário e o roadmap
governam a retomada; executor H1, nova WorldThread autoritativa e GPU continuam
entregas futuras, não consequências desta instrumentação.

### Continuação posterior — `739067b1`

O [relatório seguinte](sector-runtime-h0-worker-cache-2026-11-09.md) registra
o cache privado/LRU por worker com atualização independente do profiling e
limites do grafo ainda excluído. São 136 testes e dez grupos CTest no alvo
atual. Este relatório preserva a evidência histórica de `803d4cf7`; a coleta
profunda descrita aqui continua vinculada ao profiling regional.
