# Plano de geração interrompível de dungeons compostas

Estado: **proposta de arquitetura; não implementada**. Escopo inicial: dungeons
colocadas por `WorldServer::init(firstTime)`; geração por comando e
microdungeons só entram depois de auditar seus contratos separadamente.

## Motivo e limite observado

Na coleta Windows `ceb0017b`, o mundo `383738425:-48324202:-255713414:5`
executou sete `dungeon.generate` em 5,88 s, com uma chamada de 1,66 s.
`dungeon.flush` acumulou 3,70 s: 1,55 s em entidades e 1,41 s em objetos.
Essas dungeons nascem durante a inicialização do mundo; dividir apenas a fila
normal de setores não divide esse pico. Os contadores adicionados em
`9cbdc328` ainda precisam informar o volume de cada tipo de colocação.

Atualmente `buildDungeon` chama `part->place`, `clearTileEntities` e
`finishPart` durante a escolha das partes. Depois, `flushLiquid` e `flush`
marcam regiões, escrevem materiais, colocam objetos/veículos/bioma, criam
NPCs/stagehands, conectam fios, aplicam líquidos e IDs. Algumas operações
podem atingir o mundo **antes** de `flush`. `WorldServer::sync` grava metadata
e chama `WorldStorage::sync`, que grava setores e faz commit do banco. O
primeiro passo do rework é mapear todos os efeitos anteriores ao `flush`;
nenhuma pausa pode ser inserida no fluxo atual presumindo que o writer é
privado ou que um save excluirá esses efeitos.

## Contrato de visibilidade

### Observação inicial na Experimental

`DungeonGeneratorWriter::flush` agora conta os setores físicos 32×32 atingidos
pelos buffers de materiais e mods, com wrap horizontal. A coleta exige
`explorationProfiling.enabled` e `regionalDetails`. Por flush examina no máximo
16.384 entradas e retém até 1.024 coordenadas de setor temporárias.
`dungeon.footprint.*` registra samples, bufferedTileWritesExamined,
bufferedTileSectors, outsideWorld, crossSectorSamples e truncatedSamples;
`dungeon.footprint.observe` mede seu custo, incluso em `dungeon.flush.markRegions`.
Os contadores são somas por janela: um setor pode aparecer em vários flushes.
Uma amostra truncada fornece somente um limite inferior; a seleção segue a
iteração dos buffers e não é representativa espacialmente.

Esta é cobertura parcial de escrita, sem reservas nem execução paralela nova.
Não cobre leituras, clearTileEntities anterior ao flush, blending de terreno,
líquidos, área ocupada por entidades ou efeitos dos callbacks. A próxima etapa
é capturar esses acessos na facade e distinguir alcance conhecido de efeitos
dinâmicos antes de permitir uma reserva autoritativa. Bounding boxes de peças
sozinhas não comprovam isolamento de uma estrutura.

Uma dungeon é uma unidade lógica com identidade, footprint e conjunto de
efeitos. Antes do commit, seus tiles, objetos, NPCs, fios e índices não são
visíveis para jogadores, scripts, IA, rede nem save do mundo ativo. O servidor
reserva o footprint para impedir geração conflitante; consultas à área seguem
o estado antigo ou esperam a publicação conforme o contrato da operação.
Não publicar metade de uma porta, circuito, vila ou conjunto de NPCs.

Os setores continuam 32×32 e o formato de mundo existente permanece no
caminho legado. O protocolo novo será opt-in por versão/capacidade de mundo,
com fallback síncrono quando um mod ou efeito não tiver contrato seguro.

## Estados e dados persistentes

```text
Proposta -> Reservada -> Preparando -> Pronta -> Aplicando -> Confirmada
                  |           |           |           |
                  +-----------+-----------+-----------+-> Abortada
```

`Aplicando` não é estado publicável. O save registra somente `Reservada` ou
`Pronta` com intenção durável, ou `Confirmada` com efeitos completos. Um
estado intermediário visto no carregamento implica recuperação antes de
ativar clientes. Dados mínimos da intenção:

- versão do protocolo, identidade do mundo/dungeon, nome da definição e hash
  dos assets/mods que influenciaram o plano;
- seed, posição, índice/tentativa na sequência de geração, estado RNG ou
  escolhas já fixadas, ID de dungeon reservado e ordem estável de conflitos;
- footprint normalizado no wrap horizontal, setores lidos/escritos e revisões
  semânticas exigidas para validar a aplicação;
- plano ordenado ou referência verificável a ele, incluindo materiais, mods,
  líquidos, IDs, objetos, veículos, bioma, NPCs, stagehands, fios, drops,
  player start, proteção, gravidade e ar respirável quando aplicáveis;
- estágio, checksum e versão do payload; marcador de commit e identidade dos
  efeitos para impedir duplicação após retry.

Não persistir ponteiros, closures Lua ou objetos C++ vivos. Se assets mudarem
entre preparação e retomada, não reinterpretar silenciosamente o plano com a
definição nova. Recuperar do payload durável compatível ou cancelar antes de
qualquer efeito publicado e usar o caminho legado sob política explícita.

## Preparação e aplicação

1. **Admissão:** escolher dungeon/tentativa na mesma ordem do gerador atual.
   Reservar o footprint inteiro e IDs sob o dono do mundo. Reservas que cruzam
   wrap ou setores compartilham a mesma identidade. Não manter mutex durante
   planejamento ou execução de scripts.
2. **Plano privado:** adaptar writer/facade para produzir comandos sem tocar
   `WorldServer`. Leituras usam snapshot de cobertura completa. Capturar
   decisões de RNG, inclusive sementes implícitas em `spawnNpc`; preservar a
   ordem de escolhas e de `flush`. Registrar efeitos ocultos de
   `clearTileEntities`, `finishPart` e brushes antes de declarar esta fase pura.
3. **Validação:** no dono do mundo, conferir revisões de conteúdo de todos os
   setores e dependências, proteção, posição, identidade dos assets e conflitos
   com reservas anteriores. Cache derivado de colisão não invalida o plano;
   alterações reais invalidam. Uma falha volta à preparação com limite de
   retries, depois ao caminho serial seguro.
4. **Publicação:** aplicar comandos em ordem equivalente à atual, sob posse
   exclusiva do footprint e com staging que impeça observadores de ver efeitos
   parciais. Tiles, entidades, índice único, fios, líquidos e metadata entram
   na mesma fronteira lógica. Scripts com efeitos só rodam quando o conjunto
   necessário está consistente. Pacotes de rede saem após confirmação.
5. **Checkpoint:** persistir efeitos e marcador `Confirmada` juntos. Verificar
   atomicidade real do B-tree e da gravação de entidades/metadata antes de
   afirmar que o commit é resistente a crash. Se não houver transação que
   cubra tudo, usar journal de intenção mais aplicação idempotente, com
   recuperação obrigatória antes de abrir o mundo.

O planejamento pode ser fatiado entre ticks e futuramente executado em
worker. A fase de publicação só pode ser fatiada depois de existir staging
isolado e recuperação comprovada. Um orçamento por tick sem essas garantias
apenas deslocaria o travamento e permitiria save parcial.

## Recuperação, cancelamento e conflitos

| Estado durável | Após reinício ou unload | Garantia |
| --- | --- | --- |
| Sem intenção | Usar caminho normal. | Nenhuma reserva fantasma. |
| Reservada/Preparando | Descartar trabalho transitório e reconstruir plano com entradas verificadas. | Nenhum efeito visível; mesma seed e tentativa. |
| Pronta | Validar checksum, assets, footprint e revisões; aplicar ou reprojetar. | Plano não reaplicado em conflito real. |
| Aplicando sem confirmação | Recuperar pelo journal antes de atender cliente; completar idempotentemente ou desfazer staging isolado. | Nenhum objeto/NPC/fio duplicado. |
| Confirmada | Carregar mundo existente; liberar reserva. | Não gerar a dungeon novamente. |

Um worker tardio carrega época do mundo e identidade da reserva; resultado de
época antiga é descartado. Unload aguarda checkpoint seguro ou conserva a
intenção durável. Dois footprints conflitantes são ordenados por chave
estável derivada da sequência do mundo, nunca pela velocidade do worker.
Falha de alocação ou plano acima do teto segue caminho síncrono explícito.

## Entregas e critérios de aceite

1. **Auditoria de efeitos:** listar cada chamada do writer/facade com leitura,
   escrita, callback e dependência de Lua/RNG; confirmar limites do commit do
   B-tree. Nenhuma implementação de pausa antes desta auditoria.
2. **Planejador privado:** comparar comando a comando com execução legada na
   mesma seed e stack de assets, inclusive tentativas fracassadas e wrap.
3. **Reserva e validação:** provar ausência de interposição de setor, modificação
   de jogador e geração concorrente durante o commit, sem bloquear outros
   mundos ou manter locks enquanto aguarda worker.
4. **Journal/retomada:** injetar interrupção em cada fronteira de estágio e
   commit; recarregar e comparar tiles, entidades, IDs, fios, líquidos e
   metadata com uma geração sem interrupção.
5. **Compatibilidade:** repetir com vilarejos Glitch/Mantizi, FU, mundo flutuante,
   saves antigos, multiplayer e mods que criam NPCs/objetos customizados.
   Comparar distribuição, spread, posição, IDs, inventários e callbacks.
6. **Desempenho:** medir p95/p99 do tick e tempo até região íntegra na mesma
   cópia de universo, com 1 e vários workers. Exigir melhora sem crescimento
   sem limite de fila, memória ou retries; desligado deve usar caminho legado.

Este protocolo é pré-requisito para fatiar `dungeon.flush` e integra o item
WRG-07 do [plano regional](world-region-rework-advanced-concept.md). Não
autoriza mudar tamanho de setor nem formato de save sem migração separada.
