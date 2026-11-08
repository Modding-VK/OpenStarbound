# Auditoria da arquitetura de geração e entrega de setores

Data: 08/11/2026. Branch remota de trabalho: `Experimental`; checkout
`codex/experimental-regional-runtime`. Código examinado:
`7ca7ed2af9b5f08bc080f07e8e51c66cdbdb3428`.

Estado: **análise e proposta; nenhuma alteração de runtime nesta auditoria**.
Objetivo: aumentar substancialmente a entrega de setores padrão usando CPU
disponível, preservando mods, saves e o contrato autoritativo do mundo.

Revisão posterior da proposta, por solicitação do usuário: a
[arquitetura orientada ao hardware](sector-generation-hardware-design-2026-11-08.md)
é a direção vigente para CPU, RAM, GPU e a sequência de implementação. Os
achados e as medições desta auditoria permanecem válidos; os trechos propositivos
abaixo devem ser lidos com essa revisão, que integra GPU, residência em RAM e
controle processual de recursos ao desenho desde o início.

O detalhamento de trabalho e aceitação está no
[índice do runtime de setores](sector-runtime/README.md) e em seu
[roadmap](sector-runtime/roadmap.md). Esses documentos não transformam
propostas em funcionalidade implementada.

## Conclusão

É viável uma arquitetura superior, construída sobre o código existente:
grafo executivo incremental de etapas, preparação antecipada por dados
imutáveis, conclusão de tarefas notificando a dona e publicação serial
selecionada por dependências satisfeitas. A maior oportunidade imediata é
reduzir a latência entre etapas e tornar a preparação independente da primeira
cadeia bloqueada. O aumento de orçamento isolado não resolve esse problema.

Os resultados atuais não demonstram um fator de aceleração para a arquitetura
proposta. Também não demonstram que TMX seja o gargalo de todos os setores:
Tracy 29 teve zero factories e ainda apresentou espera de até 9,906 s.
O trabalho sobre TMX continua válido para stutter de estruturas, mas é uma
frente diferente do throughput de setores comuns.

## Método, cobertura e limites

Foram examinados os caminhos e dependências relevantes de:

- `StarWorldServerThread`: ciclo do servidor, tick, sono e sync periódico;
- `StarWorldServer`: interesse, orçamento/fidelidade, prefetch, entrada no
  mundo, geração síncrona, ativação e montagem dos pacotes de tiles;
- `StarWorldStorage`: níveis de carga/geração, fila, reservas, recursão de
  vizinhos, IO, instalação, unload, expiração e persistência;
- `StarWorldGeneration`: BaseTiles, template privado, snapshots, planejamento
  e aplicação de MicroDungeons, CaveLiquid e Finalize;
- `StarWorldTemplate`, `StarTileSectorArray`, `StarWorldGenerationTasks`,
  `StarWorldGenerationDependency`, partição/coordenador regional e worker pool;
- `StarMicroDungeon`, `StarDungeonGenerator`, readers image/TMX,
  `StarTilesetDatabase`, `StarAssets` e uso de RNG compartilhado;
- aplicação de `TileArrayUpdate` no cliente, benchmarks e testes de contratos;
- relatórios das runs 19–29 e planos existentes, em particular IO, filas
  prontas e geração interrompível de dungeons.

Esta cobertura é do pipeline e das fronteiras alcançadas por ele; não é uma
afirmação de leitura linha a linha de todo o motor ou dos mods instalados.
Não foram executados novos mundos, uma nova build, Actions, equivalência de
saves ou um benchmark da arquitetura proposta. As conclusões combinam
inspeção estática com reanálise dos artefatos existentes.

## Evidência reanalisada

A run 29 usa o build `d8f26df1ce5eb5be6325c6f09b6afbb3809c1b90`,
anterior ao preview `7ca7ed2a`. Portanto, ela **não valida esse último preview**.
Configuração: modo `dynamic`, workers automáticos, máximo de geração 40 ms,
teto dinâmico de update 32 ms, filas prontas habilitadas, prewarm de definição
e feeder especulativo de MicroDungeons desligados, profiling regional ligado.

Artefatos recebidos em 08/11/2026, mantidos fora do Git:

| Nome | Bytes | SHA-256 |
| --- | ---: | --- |
| `29.tracy` | 3.834.633 | `57dcf28b9017550920aef3b0217f33a40dae34cab19fd70bdaca57767a8015c7` |
| `exploration.zip` | 17.445.226 | `6b3f650860283dc9f85869f13a6b869a6b0090d8b1486fdc2c77957ae588c544` |
| `starbound.log` | 61.104 | `6f91ff5fd5c468d287c3ca07ae5f171fd4773849b9f2e044113ed86e1d3dc352` |
| `starbound.config` | 10.987 | `2400efb70e994d287ac083c38b62ba15c76ba834a94ee69e9db7b3e6d90dfacf` |

Seleção: apenas o perfil de servidor com início monotônico
`762660399.0736`, 52 janelas, soma de 103,042 s e `final: true`.
Janelas anteriores acumuladas no ZIP foram excluídas. Proveniência do leitor
Tracy e versão compatível constam em [tracy-integration-plan.md](tracy-integration-plan.md).

| Medida da run 29 | Resultado | Implicação |
| --- | ---: | --- |
| Orçamento concedido / passagens com fila | 33.970 ms / 1.214 | média de 27,982 ms por passagem |
| Tempo em `generation.visiblePass` | 2.348,950 ms | média de 1,935 ms; cerca de 6,9% do orçamento concedido |
| `generation.timeBudgetExhausted` | 0 | não há evidência de falta de orçamento nessa rota |
| BaseTiles: cálculo em worker | média 2,458 ms; máximo 32,090 ms | 858 resultados observados |
| BaseTiles: término do worker até commit | média 66,660 ms; máximo 603,613 ms | espera muito maior que o cálculo médio |
| MicroDungeon: descoberta de pronto até aplicação | média 24,483 ms; máximo 4.021,020 ms | métrica não inclui tempo anterior ao primeiro polling |
| Planos agendados / aplicados / stale | 748 / 739 / 6 | pipeline ativo, apesar de não haver factory |
| Snapshots de planos | 2.426.535.936 bytes cumulativos | média de 3,094 MiB por plano; não é RAM residente |
| Pool: workers configurados / ativos nas amostras | 11 / média 0,247 | amostragem esparsa; não é utilização total da CPU |
| Pedidos concluídos / expirados | 730 / 134 | conclusões incluem pedidos já satisfeitos, não só terreno novo |
| Espera máxima de pedido concluído retida | 9,906 s | cauda de entrega continua relevante |
| Finalize | média 1,786 ms; máximo 5,163 ms | 519 chamadas observadas |
| Leitura de tiles no banco | média 0,017 ms; máximo 0,163 ms | não é gargalo dominante medido nesta run |

Esses tempos são inclusivos e parcialmente sobrepostos. Não devem ser somados
como tempo exclusivo da CPU. Os 6,9% não significam que seja possível um ganho
de 14 vezes: dependências, prontidão dos workers e publicação continuam
limitando trabalho útil. A diferença demonstra oportunidade de escalonamento.

## Arquitetura atual, reconstruída do código

```text
monitoringRegions -> signalRegion -> fila de ativação
                                      |
                 generateQueue (por fidelidade/update)
                    |                  |
             resultados prontos    generateSectorToLevel(Complete)
                    |                  |
                    +---- dona <---- recursão de pré-requisitos
                                      |
                 BaseTiles -> MicroDungeons -> CaveLiquid -> Finalize
                    |             |                |            |
                 worker       plano worker       serial       serial
                    +-------------+----------------+------------+
                                      |
                  Loaded + Complete -> pacote de tiles -> cliente
```

`Complete` é alias de `Finalize=4`; `Terraform=5` é um caso especial e não
uma quinta etapa ordinária da primeira geração. O setor físico permanece
32×32. `sectorActive` exige carga de entidades e geração completa, e os
pacotes iniciais só saem depois desse estado. Enviar terreno intermediário
exigiria rever colisão, rede e a expectativa explícita do cliente sobre o
primeiro pacote; não é uma otimização transparente.

`adjacentSectors` usa o quadrado **3×3**, incluindo o próprio setor,
normalizado pela geometria. Para uma raiz interior ainda não gerada,
`Complete` possui a seguinte cobertura deduplicada:

| Etapa | Quantidade de setores exigidos |
| --- | ---: |
| BaseTiles | 49, em um quadrado 7×7 |
| MicroDungeons | 25, em um quadrado 5×5 |
| CaveLiquid | 9, em um quadrado 3×3 |
| Finalize | 1 |

São 84 nós de etapa e 315 arestas diretas para uma raiz interior. Raízes
adjacentes compartilham grande parte desses nós. O número foi conferido por
um modelo de expansão 3×3 com deduplicação; é análise da topologia, não
medição de tempo. Bordas, wrap e setores já gerados reduzem ou unem nós.
Os testes atuais do grafo também usam callbacks em cruz como casos sintéticos;
isso não muda a vizinhança quadrada real do runtime.

## Achados prioritários

### 1. A passagem termina antes de aproveitar trabalho elegível

Em `StarWorldStorage.cpp:1662`, progresso parcial pode criar
`scheduleVisibleChainContinuation`. A condição em `:1742` encerra o loop
incondicionalmente quando esse marcador foi criado. O preview recente de
avançar outros visíveis (`:1709`) pode colocar o setor no fim, mas **não
remove esse encerramento posterior**. Sua eficácia fica limitada precisamente
quando houve progresso parcial. Não deve ser anunciado como solução completa.

Sem esse caso, o fluxo legado também passa a `schedulingOnly` depois do
primeiro bloqueio: os posteriores só tentam BaseTiles. A combinação explica
por que aumentar o teto de geração pode deixar o tempo concedido sem uso.
Corrigir o encerramento exige preservar a oportunidade de justiça e impedir
que a mesma raiz seja retentada indefinidamente no mesmo ciclo.

### 2. Dependências são redescobertas e executadas por recursão

`generateSectorToLevel` (`StarWorldStorage.cpp:2204`) percorre vizinhos e
retorna ao primeiro pré-requisito incompleto. O resultado é muitas vezes
somente `false`, sem transformar o bloqueador em uma continuação executável.
Na chamada seguinte, a árvore é percorrida de novo. O grafo atual em
`StarWorldGenerationDependency` observa a mesma relação e alimenta alguns
previews; não é um executor persistente de nós com contadores de pendências
e listas reversas de dependentes.

Uma raiz pode precisar de muitos setores auxiliares; o tempo de espera
acumula várias rodadas de descoberta, agendamento, polling e commit, mesmo
que cada cálculo privado leve poucos milissegundos. Os 748 bloqueios de
MicroDungeons da run 29 são bloqueios de **etapa**, não 748 estruturas lentas.

### 3. O planejador copia tiles que não usa

O caminho assíncrono (`StarWorldGeneration.cpp:1538`) copia e versiona o halo
3×3, antes de reservar o task global. Entretanto, `planMicroDungeons`
(`:1650`) lê apenas template privado, coordenadas e distribuições de
colocações; não consulta `snapshot.haloTiles` nem `sectorRevisions`.

Esse custo de cópia, retenção e invalidação é desnecessário **para seleção de
candidatos**. Ele será relevante para um futuro planejador que simule escrita
de dungeon, mas esse produtor ainda não existe. A fase atual deve declarar
apenas suas dependências reais: versão do template, região, versão das
distribuições e identidades estáveis das colocações. A aplicação continua
exigindo a vizinhança e o estado vivo previstos pelo contrato legado.

Preparar candidatos antes de BaseTiles ficar publicado pode ser seguro após
essa separação, pois a seleção consulta o template, não os tiles vivos. Isso
não autoriza publicar a etapa MicroDungeons antes de seus pré-requisitos.

### 4. Há cálculo repetido aproveitável entre etapas

BaseTiles usa `blockInfo` para cada tile. A seleção de MicroDungeons percorre
novamente os 1.024 tiles e calcula biome items, inclusive variantes que serão
descartadas por não serem `MicroDungeonNames`. Finalize volta a calcular biome
items e filtra pelo `dungeonId` real antes de criar flora/objetos/tesouros.
O template mantém caches mutáveis privados, mas não há produto explícito por
setor compartilhando resultados puros dessas passagens.

Um produto privado `PreparedSector` pode conter terreno e candidatos
ordenados, permitindo reutilizar cálculos no mesmo worker. Finalize ainda
precisa filtrar pelo mundo atualizado e executar seus efeitos na dona.
Uma prova conservadora de ausência de candidatos permite satisfazer a parte
vazia da etapa imediatamente quando seus pré-requisitos forem cumpridos.
Não é válido pular etapas por altitude, tipo de planeta ou aparência de céu:
mods podem definir distribuições e regras nessas regiões.

### 5. Identidades de colocações pendentes precisam ser estáveis

O snapshot guarda `queuedPlacement` como índice de `m_queuedPlacements`.
O commit usa esse índice diretamente (`StarWorldGeneration.cpp:1686`),
enquanto outros commits compactam a lista (`:1504` e `:1644`). Exemplo:
dois planos capturam índices 0 e 1; um resultado cumpre 0 e remove essa
entrada; o outro ainda conserva 1. Revisões de tiles não validam a identidade
de uma entrada dessa lista.

É um risco de apontar à colocação errada ou fora do vetor, estabelecido pela
estrutura do código; não foi reproduzido como crash nesta auditoria. Deve-se
substituir índice por ID/geração e lookup validado antes de ampliar a admissão
de planos. Também registrar a versão da lista ou seu conjunto de IDs no
contrato do resultado, inclusive para resultados vazios.

### 6. IO paralelo está implementado só parcialmente

O banco e suas consultas continuam no dono. `savedTileDecodeAsyncPreview`
move descompressão/desserialização de bytes capturados para worker apenas
em chamadas Tiles-only. Uma carga `Loaded` segue síncrona e pode refazer
consulta/decode enquanto um resultado de prefetch ainda aguarda.
`generateSectorToLevel` solicita `Loaded` antes da etapa; isso também pode
instalar entidades e executar inicializações de setores auxiliares.

Oportunidade: unificar ticket de leitura/decode e promoção de Tiles para
Loaded, consumindo o mesmo resultado antes da ativação de entidades. Mover o
banco inteiro para um serviço de IO é outra mudança: precisa preservar
visibilidade de writes não commitados, sync, rollback, unload e fechamento.
Um handle independente de leitura não pode ignorar essas transações.

Na run 29 as leituras são baratas; IO não é a prioridade para o maior ganho
imediato de geração inédita. É importante para revisitas e cargas salvas.

### 7. O prefetch chega tarde e prepara somente a primeira etapa

O prefetch normal executa apenas quando a fila visível está vazia e resta
tempo, em `generateQueue`. Sua margem extra vai até 32 tiles, sem previsão de
velocidade/direção. Durante exploração contínua, pode não antecipar trabalho
suficiente. O feeder melhora BaseTiles, mas ainda usa uma fronteira reconstruída
e limites por candidatos/raiz.

Uma fila persistente deve antecipar cálculo privado com base em interesse,
velocidade e prazo estimado de chegada, mesmo com backlog visível, respeitando
uma reserva pequena para trabalho futuro e cancelamento quando o interesse
muda. Entidades e estado ativo não devem ser antecipados só por isso.

### 8. Orçamento de tempo não torna uma operação interrompível

O orçamento dinâmico observa tempo já gasto antes da geração, backlog e um
teto. Não prevê o custo de pacote/cliente, sync ou da próxima operação
indivisível. Worker count é configurado uma vez, com teto processual de 32;
os registros de tarefas/bytes são por mundo, embora a pool seja compartilhada.

A pool FIFO também não reprioriza um job já enfileirado quando o interesse
visível muda. Crescer os limites pode só ampliar snapshots obsoletos. O novo
controle deve observar backlog **executável**, atraso até commit, pressão de
memória, custo por etapa e limite de atraso das ações do jogo. FPS/tick de
simulação não precisam aumentar para a geração receber mais serviço.

### 9. Estruturas têm efeitos de RNG e de mundo durante a construção

Run 28 estabeleceu 129,831 ms inclusivos em partes TMX e 56,898 ms de build;
run 29 não acionou factory e não refinou essa divisão. Estes são outliers
independentes da espera normal das cadeias.

`RandomBrush` e `NpcBrush` (`StarDungeonGenerator.cpp:280` e `:399`)
consomem `Random`, protegido por mutex global, durante materialização de
definições. Mesmo uma leitura antecipada na dona pode mudar a ordem dessas
escolhas em relação a outras operações. Portanto, as descrições anteriores
de prewarm que sugeriam equivalência de RNG garantida eram fortes demais.
Essa equivalência não foi testada e o preview deve permanecer desligado.

Há outra dependência sutil: `WorldTemplate(Json)` delega ao construtor padrão,
que consulta `Root` e chama `Random::randu64()` antes de substituir a seed pelo
valor salvo. Portanto, até a reconstrução do template privado tem efeitos de
inicialização compartilhados. Um futuro kernel inteiramente puro precisa
receber dependências explícitas e evitar esse consumo descartado de RNG; a
existência de um ponteiro privado, sozinha, não prova isolamento de efeitos.

O gate do prewarm também está dentro de `generateMicroDungeons`, chamado por
recursão de auxiliares; não há ali verificação explícita de interesse visível.
Seu deadline reinicia por chamada, em vez de ser uma reserva global do
update. As descrições anteriores de limite por passagem/visibilidade precisam
ser interpretadas com essa limitação do código atual. Não ampliar esse preview
como forma de ocupar CPU ociosa.

Além disso, `clearTileEntities` e outras operações do writer alcançam o
mundo antes do flush final. Separar cálculo/publicação de uma estrutura exige
uma fachada gravadora completa e conflitos versionados. Acrescentar `yield`
no meio da aplicação pode expor estruturas parciais a scripts/saves.

O subsistema `Assets` já tem workers, TTL e `queueJsons`; isso deve ser
reaproveitado sob suas regras, em vez de pressupor que todo IO de asset é
serial. É distinto de permitir que um worker de geração construa readers,
tilesets e brushes com efeitos globais. A subfase TMX dominante ainda precisa
da amostra que faltou na run 29.

### 10. Região lógica não é um executor autoritativo pronto

`WorldRegionPartition` agrupa setores em regiões lógicas 2×2;
`WorldRegionCoordinator` e as threads opcionais de lifecycle/mailbox operam
metadados em sombra. Não há posse exclusiva de callbacks, Lua, entidades,
líquidos e persistência por região capaz de autorizar commits concorrentes.

Uma thread por setor adicionaria milhares de threads sem resolver essas
dependências. O primeiro ganho deve vir de jobs curtos numa pool e uma dona
eficiente. Execução autoritativa regional é uma migração posterior, com
handoff e footprint completos; NPC externo continua suspenso.

### 11. Entrada síncrona também admite trabalho em sequência

`WorldServer::generateRegion` chama `activateSector` para cada setor e espera
seu término antes de pedir o seguinte. Com `initialRegionAsyncPreview`,
`activateSector` continua repetindo a recursão e `Thread::yield()` até a raiz
concluir. Isso exercita workers, mas não forma antecipadamente um lote de
setores independentes. A aceleração da primeira entrada deve usar o mesmo
grafo, admitindo os inputs puros da região inteira antes de esperar uma
condição de conclusão, mantendo o contrato de que a região está pronta no
retorno. Esse caminho precisa ser medido separadamente da exploração.

## Arquitetura recomendada

```text
Interesse visível e previsto
          |
Grafo persistente (mundo, encarnação, setor, etapa)
  | demanda/prioridade herdada       | dependências de publicação
  v                                 v
Admissão global por tarefas/bytes -> executáveis na dona
  |
Pool de preparação privada
  | BaseTiles + candidatos / decode salvo / futuros kernels puros
  v
Mailbox de conclusões -> revalidação -> fila de commits elegíveis
                                         |
                         dona publica e libera dependentes
                                         |
                         Finalize/Loaded -> pacote inicial
```

### Grafo persistente e eventos de conclusão

- Chave de nó: epoch do mundo, encarnação do setor e etapa; compartilhar nós
  auxiliares entre todos os pedidos visíveis que dependem deles.
- Cada nó guarda pré-requisitos não satisfeitos, dependentes, demanda, revisão
  do input, task, bytes reservados e estado. Uma conclusão atualiza apenas os
  dependentes atingidos, sem reconstruir a árvore inteira.
- Distinguir `Preparado` de `Publicado`: um candidato calculado antecipadamente
  ainda espera a etapa anterior nos vizinhos antes de mudar geração persistida.
- A seleção usa prioridade do interesse, espera e custo de desbloqueio;
  herança de prioridade chega aos auxiliares sem transformá-los em ativações.
- Usar distância com wrap horizontal. Hoje a ordenação por `vmag` em
  `StarWorldServer.cpp:1452` trata posições próximas em lados opostos do planeta
  como distantes. Falta um teste de prioridade nessa fronteira.
- Conclusões entram em mailbox com ticket/identidade. A dona a drena em pontos
  controlados, podendo continuar outras tarefas prontas na mesma passagem.
  Esperar a cada tarefa curta ou fazer busy-spin impede esse benefício.

O estágio inicial deve preservar a ordem legada das mutações observáveis.
Um grafo topológico qualquer não garante a mesma ordem de dungeons, objetos e
RNG. Executar preparação pura fora de ordem é o primeiro recorte seguro;
reordenar publicação requer provar ausência de conflito ou manter uma ordem
canônica equivalente. IDs, revisões e estado de Lua devem fazer parte disso.

### Produtos privados menores e reutilizáveis

Separar `CandidatePlan` de uma futura simulação de dungeon. O primeiro usa
template/IDs/distribuições e não possui halo de tiles; o segundo precisará
snapshot do footprint real e reservas de conflito. BaseTiles e candidatos
podem compartilhar cálculo e cache local de template no mesmo job, depois
publicar seus marcos separadamente.

Reservar capacidade antes de alocar/copiar snapshots. Além de tarefas e
arrays, contabilizar outputs e memória transitiva relevante, com limites
processuais para todos os mundos. Templates privados hoje usam cache de uma
fonte por worker: alternar mundos pode reconstruí-los; uma cache pequena
orçada por mundo/revisão é candidata, após medir miss/memória.

### CaveLiquid e Finalize em passos posteriores

CaveLiquid faz flood-fill e escreve potencialmente além do setor; pode ser
extraído em kernel sobre snapshot completo e delta validado, com ordem de
conflitos preservada. O enum `CaveLiquidPlan` existente não é sua implementação.
O problema de oceanos/clima continua no backlog separado até coleta própria.

Finalize mistura normalização de tiles, grama, flora, objetos e tesouros.
Separar cálculo dos parâmetros e candidatos das criações com callbacks pode
reduzir trabalho serial. Operações entre setores e callbacks precisam de
contrato; carregar todas as entidades de auxiliares só pode ser eliminado
após provar quais consultas realmente as exigem.

### Throughput e latência com limites reais

Gerir três recursos separadamente: capacidade computacional, memória e tempo
de publicação. O controlador deve crescer preparação quando há trabalho
executável e workers livres, manter reserva para commits e reduzir distância
de prefetch quando memória/stale sobem. Configuração manual e modos de
desempenho continuam disponíveis; nenhuma política depende de um modelo de CPU.

Operações seriais longas exigem redução de trabalho, preparação realmente pura
ou staging; alterar a cifra do orçamento não as interrompe. GPU fica para
kernels regulares somente se profiling e custo de transferência justificarem;
ela não executa a fila de dependências, objetos ou Lua com eficiência útil.

## Sequência de implementação recomendada

1. **Correção e benchmark de streaming.** Estabilizar IDs de colocações;
   adicionar cenário determinístico com workers controlados e vizinhança 3×3;
   cobrir a interação `continued`/continuação/`break` do último preview.
   `planet_generation_benchmark` usa `generateRegion` síncrono e não mede a
   fila de streaming que está lenta; precisamos de uma rota de interesse e
   ticks reprodutível, com outputs verificáveis.
2. **Planejamento por dependências reais.** Extrair candidatos sem cópia do
   halo, emitir resultado vazio explícito e medir custos/cópias/stale. Manter
   as dependências legadas para o marco publicado. Depois preparar junto de
   BaseTiles e reutilizar resultados puros da varredura.
3. **Grafo executivo em sombra e depois opt-in.** Construir uma vez, atualizar
   por eventos e confrontar seus estados com o caminho recursivo. Inicialmente
   ele antecipa apenas preparação; o commit preserva a sequência autoritativa.
4. **Drenagem e publicação produtivas.** Mailbox, fila elegível e retomada no
   mesmo orçamento, com limite de trabalho por passagem e garantia de justiça.
   Registrar motivos explícitos de saída: sem trabalho, worker, dependência,
   limite de etapas, pausa de política ou deadline.
5. **IO e prefetch integrados.** Promover resultados salvos sem refazer decode;
   antecipar trabalho privado em faixa prevista com orçamento global. Auditoria
   transacional separada antes de tirar `m_db` da dona.
6. **Redução do custo serial restante.** Kernel/delta de CaveLiquid, parte pura
   de Finalize, compilação de operações TMX por fase e fachada de comandos,
   cada um com equivalência própria. Runtime regional autoritativo vem depois.

Esta ordem prioriza setores comuns e permite obter ganho antes de reescrever
Lua, entidades ou formato de save. A investigação TMX pode continuar em
paralelo como frente de stutter, sem bloquear os itens 1–4.

## Critérios de validação e continuidade

- Testes sem assets: dependências quadradas e wrap, uma/duas raízes com nós
  compartilhados, conclusão fora de ordem, zero candidatos, ausência de
  progresso, justiça, deadline, cancelamento, unload/reload e troca de mundo.
- Colocação enfileirada: duas ou mais promises, compactação, resultado antigo,
  ID ausente, retry e garantia de cumprimento único sem acessar índice velho.
- Rotas com assets: céu/terreno sem estrutura, terreno com TMX, save já gerado,
  mundo novo, viagem longa e ida/volta pela borda do planeta; dois jogadores.
- Comparar 1/N workers e profiling ligado/desligado. Separar mundo frio/quente;
  registrar build, save inicial, mods, seed, trajetória, distância e duração.
- Métricas primárias: setores inéditos publicáveis/s e tiles entregues/s;
  p50/p95/p99 de interesse→pacote, maior vazio visível, expirações e justiça.
  Decompor computação, pronto→commit e transmissão/aplicação no cliente.
- Aprovação exige mais entrega sem piorar a cauda de update ou mudar outputs
  de tiles/entidades/saves. Nenhum objetivo numérico foi medido como atingido
  por esta proposta; definir o alvo quantitativo no baseline automatizado.
- Windows é a referência atual. Linux GCC/ARM e macOS permanecem no escopo;
  Linux Clang continua adiado. Não ativar workflows automaticamente.

Validação realizada nesta auditoria: leitura cruzada dos símbolos acima,
reextração de contadores/tempos por sessão dos JSONL, confirmação SHA-256 dos
quatro artefatos de Tracy 29 e cálculo deduplicado da topologia 3×3. Não houve
teste de runtime novo. O primeiro passo concreto de desenvolvimento é o item
1, seguido imediatamente do planejador menor do item 2.

## Referências de continuidade

- [Checklist regional](regional-runtime-execution-checklist.md)
- [Plano de escalonamento](sector-scheduler-progress-plan.md)
- [Backlog de desempenho](performance-backlog.md)
- [Auditoria de IO e resultados prontos](sector-io-ready-queue-audit-2026-11-07.md)
- [Plano de publicação de dungeons](dungeon-generation-resume-plan.md)
- [Tracy 28 — TMX frio](runtime-validation-2026-11-08-tracy28-tmx-definition.md)
- [Tracy 29 — amostra sem factory](runtime-validation-2026-11-08-tracy29-tmx-inconclusive.md)
