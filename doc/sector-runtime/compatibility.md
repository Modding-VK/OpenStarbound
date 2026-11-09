# Compatibilidade, invariantes e fronteiras de adoção

[Índice](README.md) · [Validação](validation-and-telemetry.md) · [Roadmap](roadmap.md)

## Propósito

Permitir ganhos de hardware sem alterar silenciosamente mundos, mods,
scripts, rede ou persistência. Compatibilidade é uma condição de publicação
de cada entrega. Um caminho novo pode ser rápido e continuar inelegível por
alterar resultados ou tornar estado parcial observável.

## Invariantes de bloqueio

| Domínio | Invariante |
| --- | --- |
| Estado vivo | Uma dona autoritativa por unidade/instante; worker não publica por conta própria |
| Identidade | Epoch, encarnação e sequência impedem aceitar outputs de outro ocupante |
| Tiles | Edição real após snapshot não pode ser sobrescrita por output antigo |
| RNG | Fluxos e ordem de escolhas preservados no perfil de equivalência |
| Estruturas | Mesma seleção, tentativas, conflitos e integridade de footprint |
| Promises | Cumprimento único após efeito confirmado; cancelamento/erro explícitos |
| Entidades | Uma autoridade e no máximo um update aplicável por passo |
| Lua | Callback síncrono não vira operação atrasada sem contrato explícito |
| Persistência | Save descreve estado confirmado e recuperável |
| Rede | Primeiro pacote e deltas respeitam a ordem/formato suportados |
| Hardware | CPU completa disponível; GPU diferente não muda o planeta |
| Geometria | Setor físico 32×32, wrap e bordas mantidos |

## MOD-01 — Classificar operações por efeito

Inventariar cada leitor/escritor e inicializador em terreno, assets, objetos,
Lua, líquidos, fios, índices, RNG e comandos externos. Classificar como kernel
puro, preparação que exige dependências resolvidas, callback autoritativo ou
efeito multirregião. Consulta que pode invocar script não é pura por nome.

Workers de cálculo e kernels GPU recebem valores imutáveis. Não lhes entregar
`WorldServer`, VM Lua, banco mutável ou facade viva. O sistema de assets tem
suas próprias threads e scripts de patch; reutilizar sua API auditada não
autoriza executar esses scripts num worker de geração arbitrário.

Recorte auditado em H0: o
[bundle de distribuições enfileiradas](../sector-runtime-h0-distribution-bundles-2026-11-09.md)
compartilha valores somente const; o sampler/Perlin não mantém cache mutável.
O template continua privado por worker e a fila/promises permanecem na dona.
Preserva uma cópia inicial porque a normalização do hash set pode afetar a
escolha estática. Sampler real e ordem foram comparados sem assets; isso não
aprova todos os variants, callbacks ou equivalência com mods/multiplayer.

O [recorte seguinte](../sector-runtime-h0-planning-storage-2026-11-09.md) demonstra
que normalização de hash com wrap pode não ser idempotente: suprimir cópias
durante realocação muda escolhas. A reutilização de scratch mantém storage
fresco quando há MicroDungeonNames; compartilhar o nome escolhido acontece
somente depois da seleção. Não remover cópias ou filtrar candidatos antes do
sort sem comparador explícito de ordem/IDs/seleção. A auditoria segue parcial.

O [recorte de payloads](../sector-runtime-h0-candidate-payloads-2026-11-09.md)
mantém factories prvalue, crescimento e comparator legados; a contagem nativa
de cópias/moves e a seleção com colisões foram confrontadas com o vetor anterior.
Sizing lê campos públicos sem Root/Json traversal; opacidade não filtra itens.
Essa prova local não substitui comparação de mundo/mods, nem autoriza escrita
concorrente, eliminação de cópias ou ampliação de admissão.

O [recorte dos temporários](../sector-runtime-h0-temporary-storage-2026-11-09.md)
preserva a cópia por valor dos cinco containers de entrada. A cópia privada é
movida para o parâmetro do mesmo validador sem nova cópia de elementos; APIs,
predicados e consultas de WorldTemplate não mudaram. O tracker deve sobreviver
a seus escopos locais; operações não podem reter referências transferidas.
Replay nativo e testes de falha/lifetime não aprovam todo o streaming com mods.

O [recorte dos containers](../sector-runtime-h0-container-storage-2026-11-09.md)
adiciona apenas consultas const de capacidade/quantidade. Não troca campos,
allocators, representação std::map, construtores/cópias, ordem de hash,
seleção ou serialização de produção. Os pares dos fatores não são tratados
como o tamanho total dos nós. Testes verificam consulta sem hash/alocação/
mudança de ordem e bytes do mapa de fatores preservados; não são validação
completa de save, mods ou multiplayer. Json/overhead de nós permanecem excluídos,
sem autorizar eliminação de cópias ou maior admissão.

O [recorte do Json](../sector-runtime-h0-json-storage-2026-11-09.md) usa o
mesmo objeto imutável, sem reserializar, copiar chaves ou trocar representação/
allocator/ordem. Medição parcial não registra bytes dependentes do limite para
um owner compartilhado; recusa/omissão não filtra o input. Identidade, ordem
e bytes DataStream foram confrontados em fixtures nativos, incluindo capturas
concorrentes; não substituem save/reload, mods ou multiplayer reais. Json dos
variants e overhead dos nós permanecem fora, sem aprovar concorrência adicional.

O [recorte de Json enfileirado](../sector-runtime-h0-variant-json-2026-11-09.md)
enumera referências dos campos, sem usar allItems/toJson, copiar BiomeItem/
Perlin ou alterar seleção. A cópia existente conserva owners Json; o mesmo
acompanhamento segue o bundle, sem rescan/serialização no freeze. Retirada da
fila não encerra lifetime de worker. Sampler nativo/colisões e seleção ponderada
foram confrontados; isso não aprova mundo/mods/save/multiplayer completos.
Só cobre os variants enfileirados; Json ambiental/privado e nós permanecem
exclusões explícitas. Não há nova concorrência autoritativa.

O [recorte do envelope](../sector-runtime-h0-memory-envelope-2026-11-09.md)
somente classifica o snapshot do ledger e acrescenta métricas. A política de
pressão é owner-local e isolada, sem consumidor de admissão ou callbacks de
mundo. Não cancela, evicta, bloqueia drenagem ou libera buffers em uso. Conselho
não é reserva atômica nem permissão para alterar o mundo; sua integração futura
exige escopo, progresso e os gates de equivalência. O
[contrato](../sector-runtime-memory-envelope.md) mantém explícitas essas exclusões.

O [backend atômico opt-in](../sector-runtime-h0-atomic-memory-2026-11-09.md)
conserva o construtor anterior do singleton desligado. Sob configuração tipada,
uma única trava protege checagem/cobrança e fachadas não duplicam a cota.
Demanda acompanha tokens/allocators; não é promoção automática de prioridade.
Runtime legado usa a demanda padrão, mantendo cópias/crescimento e formatos
persistidos. Recusa de allocator é exceção, não resultado vazio; integração
futura precisa de contrato de jobs, retry/progresso e gates de equivalência.

A [reserva composta de BaseTiles](../sector-runtime-h0-composite-memory-2026-11-09.md)
antecipa cobrança do array e parcela inicial do template, sem novo limite ou
classificação dinâmica. Mesmo input/output in place, mesma cópia de tiles e
publicação serial; sem cópias extras de elementos para diagnóstico. O cache
consome tokens e isola ledger/demanda. O planejador mantém o caminho anterior.
Previsões enfileiradas mudam os saldos observados, não o formato persistido.
Progresso em hard foi testado com produtor limitado, não com todo heap real;
assets/mods/saves/multiplayer e retry de crescimento variável seguem gates abertos.

O [protocolo de saída variável](../sector-runtime-h0-variable-memory-2026-11-09.md)
é restrito a produtores puros controlados, sem consumidor real. Não reexecutar
Lua, callbacks ou mutações do mundo por capturar bad_alloc: erro físico segue
erro, previsão insuficiente precisa de descritor explícito, unwind e fence física.
A API anterior de batch mantém o optional; só a resposta detalhada é nova.
MOD-01/VAL-02 precisam delimitar produtor versionado e comparar outputs antes
de replay real. Sem novas configs/defaults, alteração de formatos ou ganho alegado.

O [recorte de capacidade/subkernel](../sector-runtime-h0-capacity-kernel-2026-11-09.md)
mantém pressão desligada e retry sem consumidor real. Modelo 2 elimina a revisão
manual interna; não é chave Lua/config nem mudança persistida. O kernel escalar
de líquido é usado em BaseTiles na mesma posição, com materiais já resolvidos;
Keep não atribui e preserva o líquido anterior. Compara todos os campos de
LiquidStore à lógica anterior; não altera simulação de líquidos ou oceanos.
Isso não prova equivalência de todo tile/setor, assets/mods/saves/multiplayer
ou segurança de replay do produtor completo; esses gates continuam abertos.

## MOD-02 — Equivalência e versionamento

Definir inputs que produzem um output: seed, template/layout, geração de
assets, distribuição, versão do algoritmo, ordem de comandos e estado vivo
relevante. Uma cache é válida apenas sob essa identidade.

Comparar tiles, líquidos, proteção, colisão, entidades, IDs, inventários,
promises e metadata. Para entidades/IDs com diferenças permitidas, documentar
normalização exata do comparador; não mascarar divergência de conteúdo como
ruído. Alteração de matemática GPU ou seed que mude mundo exige decisão de
versão de geração separada, fora do perfil equivalente.

## MOD-03 — Perfis de execução e fallback

| Perfil conceitual | Uso |
| --- | --- |
| Legado com preparação acelerada | Mods existentes mantêm efeitos síncronos; kernels privados podem ganhar velocidade após prova de equivalência |
| Regional experimental | Somente subsistemas e conteúdo com capacidades auditadas podem ter donos regionais |
| Regional ampliado | Mais APIs/subsistemas após handoff, transações, save e multiplayer aprovados |

O ganho de CPU/RAM/GPU na preparação não exige reescrever todos os mods.
Já a simulação regional concorrente de callbacks antigos não tem equivalência
automática. Conteúdo desconhecido precisa de fallback por contrato antes da
ativação do modo regional; nunca por uma lista de nomes de mods supostamente
seguros. FU e roteadores síncronos continuam no caminho compatível até auditoria
das operações que compartilham estado.

Não decidir fallback depois de publicar metade de uma operação. Retorno de
modo requer fronteira de checkpoint/reload. Fallback de um kernel numérico
antes do commit é diferente de migrar um mundo regional parcialmente ativo.

## MOD-04 — Integração de subsistemas e plataformas

Transferência de entidades, fios, líquidos, clima, quests e scripts globais
tem protocolos próprios. Handoff de NPCs existentes não retoma o plano
suspenso de externalização de IA. No modo central inicial, esses efeitos
continuam sob o contrato atual.

Manter builds normais e diagnósticas distinguíveis. GPU compute é opcional;
driver/contexto ausente preserva CPU. Windows, Linux GCC/ARM e macOS seguem
no escopo; Linux Clang permanece adiado. Uma dependência futura exige origem,
versão, licença e integridade conforme a
[política do projeto](../dependency-sources.md).

## Melhoria esperada e aceitação

O objetivo é permitir adoção incremental com regressões localizáveis e caminho
compatível preservado. Uma entrega termina apenas quando seu teste de efeitos,
erro e recuperação estiver documentado, além do benchmark de velocidade.

Revisão independente é necessária para mudanças de alto impacto antes de
integração a `main`. O resultado deve informar limites conhecidos: aprovação
de um kernel não aprova todo o runtime regional. Ver a matriz de
[validação](validation-and-telemetry.md).
