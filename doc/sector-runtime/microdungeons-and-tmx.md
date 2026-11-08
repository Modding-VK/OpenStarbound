# Microdungeons, TMX e publicação de estruturas

[Índice](README.md) · [CPU](cpu-and-stages.md) · [Compatibilidade](compatibility.md)

## Propósito e distinção de gargalos

Reduzir custo de selecionar, materializar e aplicar estruturas, preservando
seed, tentativas, regras de mods e footprint. Setores comuns sem estrutura
também atravessam a etapa MicroDungeons: bloqueio dessa etapa não significa
que uma factory esteja consumindo CPU. O pipeline padrão e o stutter TMX
possuem métricas separadas.

Microdungeon local e dungeon composta são mecanismos diferentes. Um encontro
pode ser selecionado por um setor e escrever além dele; uma dungeon composta
liga peças por conectores. Ambos exigem mapa de efeitos e conflitos.

## MD-01 — Seleção leve e identidade

Após DAG-01, produzir candidatos a partir do template privado, coordenadas e
IDs/versionamento das colocações. O halo copiado atualmente não é lido pela
seleção; separar esse produto de uma futura simulação de efeitos sobre tiles.

Resultado vazio tem revisão das distribuições e das colocações pendentes.
Nova colocação pode invalidá-lo. Cumprimento de promise permanece após
sucesso autoritativo e não após escolher um nome ou aquecer uma definição.
Ordenação e sementes usam o contrato existente de coordenadas canônicas.

## MD-02 — Formato intermediário estático de TMX

Separar leitura dos dados e decodificação estática de instanciação de brushes.
O produto intermediário pode conter camadas decodificadas, geometria,
propriedades, conectores e índices por fase. Dados de arquivos já resolvidos
entram no kernel privado; aquisição usa o sistema de assets auditado.

As chaves incluem geração dos assets e versão do formato intermediário.
Registrar tamanho e reutilização. Cache não atravessa reload de mods sem
invalidação explícita. A aquisição antecipada deve partir de referências
selecionadas e demanda do grafo, com limite de memória, não de varredura
indiscriminada de todo o conteúdo instalado.

## MD-03 — Ligação e RNG autoritativos

`RandomBrush` e `NpcBrush` podem consumir RNG compartilhado durante a
materialização. Preserve uma fase de ligação com escolhas/identidades
registradas na ordem autoritativa. Uma definição privada construída fora de
ordem pode mudar o mundo mesmo sem escrever tiles.

O prewarm entregue continua experimental/desligado fora de testes: ele
desloca o custo frio dentro da mesma thread e sua equivalência de RNG não
foi demonstrada. O bypass de planos vazios já foi validado para aquele
preview; isso não valida a publicação antecipada de definições em geral.

Aceitação: retorno invertido de preparação não muda seleção, sementes de
entidades nem conteúdo de estrutura. Retries não repetem escolhas ou efeitos.

## MD-04 — Comandos, footprint e reserva

Converter a facade/writer em descrição completa de leituras e efeitos:
materiais, mods, líquidos, IDs, proteção, player start, limpeza de entidades,
objetos, flora, NPCs, veículos, stagehands, drops, fios e marcações.
`clearTileEntities` e outros efeitos anteriores ao flush precisam entrar no
contrato; buffers de materiais sozinhos não tornam o writer privado.

O footprint inclui read set, write set e alcance dos callbacks/entidades,
normalizados no wrap. Cobertura truncada é um limite inferior diagnóstico e
não pode autorizar commit. Reservas multirregião são ordenadas e revalidadas;
conflito rejeita ou replaneja a operação inteira antes de publicar efeitos.

Enquanto um callback opaco puder tocar estado global, conservar a fase serial
compatível. Ativar publicação por regiões exige o contrato de
[threads regionais](regional-threads.md) e [checkpoint](io-persistence-network.md).

## MD-05 — Reduzir aplicação e permitir retomada

Avaliar representação compacta de tiles relevantes, operações por fase e
índices de conectores, preservando ordem de opções e RNG. Agrupar escritas e
invalidações somente após provar equivalência de consultas e notificações.

Uma estrutura grande só pode ser fatiada entre ticks se o staging permanecer
invisível e a recuperação cobrir a unidade lógica. Seguir o
[plano de dungeons interrompíveis](../dungeon-generation-resume-plan.md),
com intenção, versão de assets, sequência e commit recuperável. O caminho
síncrono de comandos também precisa de contrato explícito antes de mudar.

## Evidência, melhorias e aceitação

Run 28 apontou 129,831 ms inclusivos em duas partes TMX; a subdivisão mais
recente ainda necessita de uma amostra fria válida. Run 29 não chamou factory.
As medições detalhadas ficam nos relatórios, ligados pelo [baseline](baseline.md).

Esperam-se seleção com menos cópias, menos materialização repetida e menor
custo serial por estrutura. Medir JSON, tilesets, camadas/decompressão, objetos,
scan/âncora, rejeições, pintura por fase, flush, criação de entidades e
publicação. Não somar tempos inclusivos como tempo total exclusivo.

Aceitação: mesmo mapa/conteúdo com mod stack de referência, seeds, quantidade
de tentativas e ordem equivalentes; Mantizi/Glitch, estruturas cruzando wrap,
unload/reload, save parcial simulado e dois jogadores. Esta frente não bloqueia
as entregas de setores vazios/padrão no [roadmap](roadmap.md).
