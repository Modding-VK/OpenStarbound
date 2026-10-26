# Hipóteses para um futuro rework da geração de mundos

Estado: documento de estudo, sem decisão de implementação. Registrado em
25/10/2026 a partir da discussão sobre o vilarejo Mantizi do Frackin’ Universe
(FU) 6.5.8. As propostas abaixo não descrevem alterações já feitas no motor.
Uma proposta separada para [execução do mundo por regiões de setores](world-region-rework-advanced-concept.md)
trata a reconstrução da `WorldServerThread` e das fronteiras entre subsistemas.

## 1. Problema e objetivos

O jogador percebe áreas e estruturas aparecendo lentamente ao atravessar um
vilarejo Mantizi. A observação visual ainda não atribui o atraso a uma etapa
específica. Queremos reduzir o tempo até a área ficar completa e preservar:

- integridade de tiles, objetos, NPCs, iluminação e líquidos;
- distribuição, volume e posicionamento de estruturas;
- determinismo, recarregamento, mundos salvos, mods e multiplayer;
- fluidez em máquinas com recursos de hardware diferentes.

Uma otimização deve medir separadamente **vazão** (área preparada por segundo),
**latência** (espera por uma área específica) e picos de duração dos updates.
Melhorar uma dessas medidas pode piorar outra.

## 2. Funcionamento confirmado no código atual

`WorldSectorSize` vale 32 tiles ([StarGameTypes.hpp](../source/game/StarGameTypes.hpp)).
Um setor ocupa 32 × 32 = 1.024 tiles. Quatro setores cobrem a mesma área de
tiles que um setor de 64 × 64 = 4.096 tiles, desconsiderando bordas do mundo.

O servidor solicita setores da região monitorada pelo cliente, acrescida de
`playerActiveRegionPad`. A fila ordena os pedidos pela distância ao jogador.
Se houver dados salvos, o armazenamento lê e desserializa tiles e entidades.
Para terreno novo, a progressão é `BaseTiles` → `MicroDungeons` → `CaveLiquid`
→ `Finalize`. Etapas podem exigir que setores vizinhos tenham alcançado o
nível anterior ([StarWorldStorage.cpp](../source/game/StarWorldStorage.cpp)).

Com `worldThreading.sectorWorkers > 0`, apenas a preparação isolada de
`BaseTiles` usa workers. O resultado é validado e instalado na thread do
mundo. Há no máximo duas tarefas pendentes por worker; a pool é compartilhada
entre mundos. O padrão é zero workers. Microdungeons, finalização, carregamento
de entidades, scripts e escrita do mundo continuam na thread do mundo
([StarWorldGeneration.cpp](../source/game/StarWorldGeneration.cpp)).

A fila ativa tem um limite de tempo por update: 6, 8, 12 ou 16 ms conforme a
fidelidade configurada em [worldserver.config.patch](../assets/opensb/worldserver.config.patch).
Uma etapa já iniciada termina inteira, mesmo que ultrapasse o limite. O
prefetch adicional prepara somente `BaseTiles` fora da região ativa, com
workers habilitados, quando a fila ativa está vazia e ainda resta tempo no
update. Ele não conclui estruturas distantes. Os tiles iniciais só são
enviados ao cliente quando o setor está completamente ativo
([StarWorldServer.cpp](../source/game/StarWorldServer.cpp)).

Assim, o tempo percebido pode incluir espera na fila, leitura do banco,
dependências de vizinhos, geração, instalação de entidades, envio de pacotes
e processamento no cliente. Essas etapas podem se sobrepor; seus totais de
profiling não devem ser simplesmente somados.

O mundo persistente fica em um banco B-tree local, não em um serviço como
Redis. Setores carregados permanecem em arrays e mapas de entidades na
memória até serem descarregados. O banco tem um cache LRU de nós de índice
com limite padrão de 64 nós ([StarBTreeDatabase.cpp](../source/core/StarBTreeDatabase.cpp));
isso não equivale a um cache em RAM de todos os setores. O sistema operacional
também pode manter páginas do arquivo em memória. Ao carregar um setor salvo,
a thread do mundo consulta o banco, descomprime e reconstrói tiles e entidades;
a inicialização de entidades pode executar callbacks. `storage.*.read` mede a
consulta ao banco, não apenas leitura física do SSD/HD. Já existe persistência
incremental dos tiles alterados, mas ela atende ao salvamento, não acelera o
primeiro cálculo de terreno inédito.

A área solicitada acompanha as regiões retangulares monitoradas pelo cliente;
ela não é uma onda radial de geração do planeta inteiro. A fila ativa ordena
setores pela distância entre o centro do setor e o jogador mais próximo.
Dependências com vizinhos podem mudar a ordem efetiva de conclusão. O prefetch
atual acrescenta a mesma margem em X e Y: 0, 8, 16 ou 24 tiles por lado,
conforme a fidelidade, limitada a 32. Em `medium`, a margem extra é 16 tiles
por lado. Como o setor mede 32 × 32, isso pode alcançar um setor vizinho,
dependendo do alinhamento da região. A fila de prefetch não usa uma previsão
de movimento nem prioriza setores por distância dentro dessa área extra.

## 3. Dois mecanismos de estruturas que não devem ser confundidos

### Microdungeons e encontros locais

`generateMicroDungeons` percorre as posições de um setor, obtém candidatos do
template, ordena os posicionamentos e aplica os resultados ao mundo. Uma
estrutura pode alcançar tiles além do setor que iniciou a tentativa. Logo,
trocar os limites dos setores pode alterar agrupamento, ordem, conflitos e
resultado final mesmo quando a semente do planeta não muda.

### Dungeons compostas

O FU 6.5.8 define
[`mantiziestate.dungeon`](https://github.com/sayterdarkwynd/FrackinUniverse/blob/6.5.8/dungeons/fumantizi/mantiziestate.dungeon)
com o senado como âncora, 11 tipos de peças na definição e `maxParts: 20`.
Entre as peças há villas, casa do cônsul, jardins, fazenda e áreas de moradia.
Elas são conectadas pelo gerador de dungeons; não equivalem a 20 microdungeons
independentes. O mod também contém
[`fumantizi_villa.json`](https://github.com/sayterdarkwynd/FrackinUniverse/blob/6.5.8/dungeons/fu_racialencounters/fumantizi_villa.json),
um encontro distinto. Não sabemos qual mecanismo produziu o vilarejo visto
no trecho visual específico sem correlacionar posição e horário. Porém, um
`starbound.log` já fornecido pelo usuário confirma que o planeta
`383738425:-48324202:-255713414:2` recebeu `mantiziestate` em `[478, 1000]`.
O mesmo log mostra `Creating celestial world` às 14:16:47 e `Placing dungeon
mantiziestate at [478, 1000]` às 14:16:49. Ele não registra a duração isolada
da montagem dessa dungeon.

Na **primeira criação do mundo**, o servidor percorre as dungeons previstas
no template, escolhe uma posição a partir dos parâmetros do planeta e da
semente, tenta a colocação e monta suas peças. O log registra `Placing dungeon
<nome> at <posição>`. A montagem devolve as regiões ocupadas. Enquanto a
dungeon é montada, `markRegion` solicita leitura/ativação de regiões, mas o
modo `m_generatingDungeon` evita a geração normal dos setores nesse ponto.
Mundos especiais do tipo *floating dungeon* forçam a geração das regiões
retornadas; o caminho normal não faz isso para todas as dungeons. Assim, a
montagem antecipada da cidade não prova que seus setores já estão completos
e prontos para serem enviados ao cliente.

### Custos internos da montagem composta

O caminho confirmado em [StarDungeonGenerator.cpp](../source/game/StarDungeonGenerator.cpp)
e [StarWorldGeneration.cpp](../source/game/StarWorldGeneration.cpp) é:

1. `DungeonDefinitions` consulta um cache LRU de até 20 definições. Ao carregar
   uma definição TMX, lê e decodifica seus mapas, tilesets e conectores.
2. O gerador escolhe a âncora e examina cada conector aberto. Para encontrar
   opções, `findConnectablePart` percorre todas as peças e seus conectores.
   Cada candidato passa por limites de quantidade/distância, combinações,
   sobreposição, ameaça e, quando necessário, consultas aos tiles do mundo.
   A escolha usa pesos e a sequência do gerador aleatório.
3. Uma peça aprovada verifica entidades existentes, pinta em **dez fases**
   (`Clear`, paredes, mods, objetos, árvores, itens de bioma, fios, itens,
   NPCs e IDs de dungeon) e percorre o mapa inteiro em cada fase. Há ainda
   passagens para descobrir tiles modificados e preservados. Uma candidata
   rejeitada também pode exigir uma passagem para testar colocação.
4. O `DungeonGeneratorWriter` reúne alterações e depois as aplica ao mundo:
   materiais, objetos, plantas, NPCs, fios, itens, líquidos e IDs. Escritas
   de tile passam por `modifyServerTile`, que invalida colisão em uma região
   de influência, informa o motor de líquidos e prepara atualizações de rede.
   Mesmo sem clientes próximos, parte dessas operações é executada.

O FU define, por exemplo, senado de 80 × 60 tiles, villa de 134 × 60, fazenda
de 60 × 60 e um tipo de moradia de 60 × 60. Esses tamanhos mostram que as
passagens repetidas podem ser relevantes, mas o código e o log atual **não
quantificam** o custo de cada fase. A definição e os mapas já são cacheados
enquanto permanecerem no LRU; simplesmente adicionar outro cache de arquivos
não é uma prioridade fundamentada.

Oportunidades a avaliar em ordem de risco crescente:

- Indexar previamente conectores compatíveis por tipo/direção, sem alterar a
  ordem das opções nem o consumo de números aleatórios. Isso reduz buscas
  repetidas em cidades com muitas peças.
- Preparar, junto com a definição TMX, listas de operações por fase ou uma
  representação compacta das células relevantes. Deve preservar a ordem das
  camadas e dos objetos, inclusive regras e brushes de mods.
- Medir rejeições em `canPlace` e ordenar somente verificações comprovadamente
  independentes e baratas antes das consultas ao mundo.
- Estudar aplicação de tiles em lote, com invalidação de colisão e notificações
  por região. Exige provar equivalência em líquidos, objetos, proteção,
  callbacks e atualização dos clientes.
- Só considerar cálculo paralelo da montagem após definir um snapshot do mundo
  e commit ordenado; peças ligadas por conectores e conflitos não podem ser
  colocadas livremente em paralelo.

Instrumentar separadamente carregamento da definição, busca de conectores,
quantidade de candidatos tentados/aceitos, `canPlace`, pintura por fase,
`flushLiquid`, aplicação de tiles, criação de entidades e `flush` total.
O profiler atual mede `serverUpdate` e geração de setores, mas não fornece
essa decomposição da montagem inicial. Um benchmark só de dungeon composta,
com a mesma semente, assets e mod stack, permitiria comparar alternativas
sem confundir compilação, entrada no planeta e carregamento posterior.

## 4. Hipótese: setores físicos de 64 × 64

O [plano de regiões](world-region-rework-advanced-concept.md) começa com
setores **físicos** de 32 × 32. Quatro deles podem formar uma região de
simulação **lógica** de 64 × 64 sem converter saves nem trocar a unidade de
geração. Considerar um setor físico de 64 × 64 apenas após medir se o custo
fixo por setor permanece dominante nessa arquitetura.

Um pedido físico de 64 × 64 pode substituir quatro pedidos de 32 × 32 para
cobrir a mesma área. Isso reduz parte do custo fixo de fila, metadados,
persistência e transferência. Também concentra quatro vezes mais tiles por
pedido. O trabalho que percorre tiles continua existindo, e um setor maior
pode aumentar a espera pelo primeiro pacote completo. Quatro trabalhos
independentes de `BaseTiles` também virariam um único trabalho nessa área,
podendo reduzir o paralelismo útil. Memória, armazenamento e núcleos livres
não aceleram automaticamente operações que ainda são seriais.

Há duas variantes diferentes:

1. **64 físico / 32 lógico:** armazenar ou transferir blocos maiores, mas
   continuar avaliando quatro regiões lógicas de 32 × 32 com sementes, ordem
   e dependências atuais. É a variante mais promissora para reproduzir a
   geração existente; preserva parte do custo de quatro operações.
2. **64 nativo:** refazer avaliação, dependências e aplicação para a nova
   unidade. Permite eliminar mais custo fixo, mas exige regras de geração
   baseadas em coordenadas globais e resolução explícita de conflitos. Mesmo
   preservando integridade e densidade média, o mundo pode diferir tile a
   tile do produzido com setores de 32 × 32.

Para preservar **spread** e volume, medir estruturas por área de mundo, não
apenas por setor. Uma contagem fixa por setor de 64 × 64 reduziria a densidade
por tile. Reproduzir exatamente as mesmas estruturas nas mesmas posições
requer conservar o comportamento lógico de 32 × 32, inclusive sementes,
ordem de avaliação, regras de vizinhança e conflitos. Dungeons compostas
exigem, adicionalmente, que peças cruzando novos limites continuem sendo
salvas e recarregadas integralmente.

`WorldSectorSize` participa de armazenamento, geração, vizinhança e envio de
tiles. Mudar apenas a constante não é uma migração. É necessário definir
tratamento de mundos antigos, versões de formato, clientes/servidores com
versões diferentes e retorno seguro à build anterior. Uma estratégia de
migração deve preservar também os níveis de geração e as entidades de cada
região antiga.

## 5. Hipótese: pré-gerar dungeons compostas conhecidas

Para dungeons previstas pelo template, a posição candidata é escolhida na
criação do mundo; os limites finais das peças ficam conhecidos após a
montagem. Poderíamos usar as regiões devolvidas para completar de antemão os
setores que cobrem a dungeon, sem depender da distância do jogador. Isso
troca trabalho durante a exploração por tempo na primeira abertura do
planeta, mais memória e armazenamento. Precisa respeitar a ordem atual entre
colocação de peças, terreno, líquidos, entidades e persistência para não
sobrescrever a dungeon nem alterar seu conteúdo.

Essa estratégia não antecipa automaticamente microdungeons ou encontros
escolhidos durante a geração local posterior. Para o planeta registrado no
log, `mantiziestate` foi de fato montada; ainda falta correlacionar a posição
visitada no vídeo e medir se seus setores chegaram incompletos ao cliente.

## 6. Hipótese: paralelizar o cálculo de microdungeons

A função atual lê o template e usa uma fachada ligada ao mundo vivo para
colocar tiles, objetos e entidades. Chamá-la inteira em workers seria inseguro.
Uma possível divisão é:

1. a thread do mundo seleciona candidatos, sementes e uma cópia limitada do
   terreno e dos dados necessários;
2. workers calculam planos de estruturas sem ler ou modificar o mundo vivo;
3. a thread do mundo valida versões do terreno/template e aplica os planos em
   ordem determinística, resolvendo sobreposições e descartando planos velhos.

O cálculo poderia ocorrer em paralelo a scripts e outros trabalhos. A
aplicação final permaneceria serial. Essa proposta precisa cobrir callbacks
de mods, objetos, NPCs, posições que atravessam setores e conflitos entre
estruturas antes de ser considerada segura para multiplayer.

## 7. Hipótese: cache de setores e leitura antecipada

É preciso distinguir três usos de memória:

1. **Setor ativo em RAM:** evita reler e reconstruir um setor ainda carregado.
2. **Cache limitado de setores salvos:** poderia manter dados comprimidos ou
   decodificados de regiões próximas e reduzir custo de revisitas ou leituras
   antecipadas. Não torna mais rápido o primeiro cálculo de terreno novo.
3. **Cache de cálculos intermediários:** poderia evitar recomputar partes
   custosas do template, seletores ou biomas. Workers já reutilizam seu
   template privado; só adicionar novos resultados após medir repetição real.

O arquivo do mundo já é a persistência do resultado final da geração. Um
segundo cache persistente precisaria de chave e invalidação para semente,
coordenadas, versão do gerador, template e dados relevantes dos mods, além de
modificações feitas pelo jogador. Um resultado obsoleto pode restaurar tiles
antigos ou produzir vizinhanças inconsistentes. Redis ou outro serviço externo
traria tráfego, cópias e operação adicional; não remove descompressão,
instalação de entidades nem etapas seriais da geração.

Uma alternativa a investigar é ler e, quando seguro, descomprimir setores
salvos em segundo plano. A thread do mundo validaria a revisão dos dados e
instalaria o resultado. Limitar memória e quantidade de pedidos pendentes
impede que deslocamentos rápidos preencham o cache com áreas abandonadas.
Comparar **primeira visita**, **revisita na mesma sessão** e **revisita após
reiniciar** para separar geração, cache de RAM e cache do sistema operacional.

## 8. Hipótese: prefetch horizontal e guiado pelo movimento

Na exploração terrestre comum, deslocamentos horizontais podem atravessar
setores novos mais rapidamente que descidas por cavernas. Subidas muitas
vezes percorrem áreas conhecidas ou céu aberto. Essa é uma hipótese de uso,
não uma regra da engine: queda, voo, veículos, teleporte e cavernas verticais
podem demandar terreno abaixo ou acima rapidamente. Mesmo céu aberto precisa
ter setores carregados e enviados.

Uma primeira variante aumentaria apenas a margem horizontal, por exemplo
32 tiles extras em X e 16 em Y no perfil `medium`. O código atual usa um
valor único para ambos os eixos, então isso exigiria configuração vetorial e
nova lógica. Um retângulo mais largo solicita mais setores, mas não garante
conclusão: o prefetch atual só trabalha quando a fila ativa está vazia e sobra
tempo no update. Ele prepara apenas `BaseTiles`; estruturas, entidades e envio
inicial continuam esperando a ativação. Pedidos distantes podem gastar
memória e capacidade dos workers sem benefício se o jogador mudar de rumo.

Uma variante mais seletiva estimaria posição futura a partir de velocidade
e direção recentes, suavizadas e limitadas por tempo/distância. Prioridade
proposta: (1) setores visíveis ou necessários para evitar bloqueio; (2)
setores à frente do movimento; (3) margem restante. Parar ou virar deve
reduzir rapidamente o viés de direção. Manter uma margem mínima em todos os
lados evita falhas em deslocamentos verticais e mudanças bruscas. Em
multiplayer, o critério deve atender o jogador que mais precisa de cada
setor, com proteção contra um jogador monopolizar a fila. Mundos com
horizontalidade circular também exigem distância que respeite o wrap.

Reordenar a fila não paraleliza microdungeons: apenas escolhe qual setor
recebe tempo primeiro. Medir taxa de acerto do prefetch (setores preparados
que depois se tornaram necessários), idade e profundidade da fila, tempo
até o setor visível ficar completo, uso de RAM e picos da thread do mundo.

## 9. Como decidir entre as abordagens

O log já identifica `mantiziestate` em `[478, 1000]` para um planeta do teste.
Correlacionar essa posição com o percurso e medir com a build Windows #21.
Usar o procedimento de
[performance-backlog.md](performance-backlog.md) e
[exploration-profiling.md](exploration-profiling.md). A build compila, mas a
fila de prefetch ainda aguarda validação local.

| Resultado observado | Hipótese a investigar primeiro |
| --- | --- |
| Tiles básicos aguardam workers | Capacidade, ordenação e alcance do prefetch |
| Microdungeons dominam o tempo de geração | Cálculo em snapshot e aplicação serial |
| Dungeon composta já montada, mas setores aparecem tarde | Pré-geração de suas regiões ou caminho de leitura, ativação e envio |
| Primeira criação do planeta domina o atraso | Instrumentar montagem composta; avaliar índices de conectores e passagens TMX antes de paralelismo |
| Leitura/descompressão de setores salvos domina a revisita | Cache limitado e leitura antecipada com validação de revisão |
| Muitos pedidos de prefetch nunca são usados | Reduzir alcance ou priorizar direção e velocidade do jogador |
| Espera ocorre à frente de deslocamento horizontal contínuo | Comparar margem horizontal maior e previsão de movimento |
| Muitos setores pequenos custam mais que o trabalho nos tiles | Protótipo 64 físico / 32 lógico antes de 64 nativo |
| O cliente recebe os setores em tempo, mas desenha tarde | Instrumentação de aplicação de pacotes e painter, separada da geração |

Qualquer protótipo deve comparar mundos novos e já salvos, estruturas que
atravessam limites, repetição com a mesma semente, Frackin’ Universe, outros
mods, multiplayer, iluminação e carregamento após reinício. Avaliar tempo
até a área ficar completa, picos de update, vazão, uso de memória e tamanho
dos dados gravados. Nenhuma das hipóteses acima foi escolhida ou implementada.
