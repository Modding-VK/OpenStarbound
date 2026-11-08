# Grafo executivo, prioridade e ciclo de tarefas

[Índice](README.md) · [WorldThread](worldthread.md) · [CPU](cpu-and-stages.md)

## Propósito e melhoria esperada

Eliminar rodadas de redescoberta e espera que deixam o orçamento ocioso.
Representar pré-requisitos uma vez, compartilhar auxiliares entre pedidos e
liberar os dependentes por eventos. Espera-se reduzir pronto→commit,
expirações e tempo até o setor ficar utilizável, mantendo justiça.

## Modelo que deve ser preservado

`WorldStorage::adjacentSectors` usa vizinhança 3×3, incluindo o próprio setor.
No interior de um mundo vazio, uma raiz Complete requer 49 BaseTiles, 25
MicroDungeons, 9 CaveLiquid e 1 Finalize: 84 nós e 315 arestas deduplicadas.
Raízes próximas compartilham esse trabalho. Wrap, bordas e estado já gerado
alteram o conjunto efetivo.

O modelo existente de `WorldGenerationDependencyGraph` não é ainda um executor
persistente. Seu limite de observação pode truncar um plano; uma observação
truncada nunca autoriza ignorar pré-requisitos.

## DAG-01 — Identidades estáveis

Separar identidade de pedido, nó, produtor e colocação:

| Identidade | Campos conceituais |
| --- | --- |
| Pedido | mundo/epoch, sequência, consumidor e motivo |
| Nó | mundo, setor, encarnação e etapa |
| Produtor | nó, input revision e task sequence |
| Colocação pendente | ID monotônico, geração, distribuição e estado |
| Operação publicada | task/colocação, sequência de efeito e commit |

Substituir índices de `m_queuedPlacements` por IDs validados. Compactar uma
lista não pode mudar a colocação referida por um plano antigo. ID ausente,
cumprido, cancelado ou de outra geração impede o efeito. Promises são
cumpridas uma única vez e somente após sucesso autoritativo.

Aceitação: testes com dois planos em voo, compactação, cancelamento, nova
colocação e retorno invertido não acessam outra entrada nem duplicam efeitos.

Primeiro recorte implementado em H0: `WorldGenerationPlacementQueue` atribui
IDs monotônicos por instância de `WorldGenerator`, mantém a ordem relativa de
inserção e retira a entrada após sucesso da factory, antes de cumprir a
promise. Planos síncronos/assíncronos transportam apenas o ID e inputs copiados.
A validação de epoch do mundo continua pertencendo à tarefa existente.
ID retirado/ausente é ignorado antes da factory. Não há índice nem ponteiro
persistente na descrição assíncrona. Testes sem assets cobrem retorno
invertido, duplicação, remoção, inserção, retry e ordem; o
[relatório](../sector-runtime-h0-placement-identities-2026-11-08.md) delimita
a validação. Cancelamento público de colocações, versão do conjunto de inputs
e comparação completa em jogo permanecem fora deste recorte.

## DAG-02 — Grafo persistente e demanda compartilhada

Cada nó guarda estado de preparação/publicação, dependentes, contagem de
pré-requisitos, revisões necessárias, consumidores e custo estimado.
Atualizar o grafo por mudança de interesse, conclusão, publicação e unload.
Remover demanda de um jogador não cancela uma dependência ainda necessária
a outro. Auxiliares herdam prioridade sem virar pedidos de ativação.

Manter dois conjuntos de dependências: inputs para calcular um produto e
condições para torná-lo autoritativo. Candidatos de MicroDungeons podem ser
preparados antes dos tiles vizinhos; a publicação da etapa continua esperando
o contrato de vizinhança. A versão das colocações faz parte dos inputs até
mesmo de um resultado vazio.

Primeira implementação em sombra: confrontar o grafo com o caminho recursivo,
registrando divergências sem mudar resultados. Depois permitir antecipação
pura; publicação fora da sequência antiga exige validação própria.

## DAG-03 — Prioridade, justiça e distância correta

Definir prioridade por classe de interesse, prazo estimado de chegada, idade
original, benefício de desbloqueio e desempate estável. A menor cadeia útil
de um setor visível transmite sua urgência aos produtores auxiliares.
Deduplicação não deve apagar a idade do consumidor mais urgente.

Usar distância da geometria com wrap horizontal. Prevenir starvation entre
jogadores, mundos e classes; uma sequência densa de pedidos novos não remove
a oportunidade de um antigo. TTL e política de carga permanecem explícitos,
sem renovação oculta causada por prioridade.

Registrar quem foi escolhido, qual razão e qual bloqueador impediu o avanço,
com cardinalidade limitada. Não confundir total de bloqueios com quantidade
de dungeons executadas.

## DAG-04 — Conclusões e progresso dentro da passagem

Uma mailbox transporta identidade e referências imutáveis a resultados; a
dona a consome, valida e atualiza os nós dependentes. O instante de término
do worker e o instante da primeira observação são registrados separadamente.
Resultado pronto continua ocupando sua reserva até commit ou descarte.

Corrigir a interação do preview de avanço com o encerramento por continuação:
continuar candidatos distintos quando elegíveis e houver orçamento. Impor
limite de tentativas sem progresso e impedir retentativa infinita da mesma
raiz. A oportunidade reservada de justiça não pode ser consumida pelo drain.

Não fazer um worker aguardar uma tarefa que só pode começar na mesma pool
ocupada. A dona também não espera um produtor individual enquanto houver
outro caminho executável.

## DAG-05 — Cancelamento, falha e invalidação

```text
Demanda -> Reservado -> Executando -> Preparado -> Elegível -> Publicado
                \             \             \          \
                 +-------------+-------------+-----------> Descartado
```

Este é um modelo lógico proposto, não a enumeração atual do código.
Invalidar retira autorização de publicar, mas não prova que o produtor parou.
Preservar a exclusão da chave de tarefa até sua conclusão confirmada; unload
não libera uma reserva ainda usada por worker/GPU. Nova encarnação não aceita
o resultado antigo, mesmo que o endereço de memória seja reutilizado.

Revisões de conteúdo e de template precisam ser independentes de cache
derivado de colisão. Um retry tem motivo e limite, seguido de fallback
documentado; falha de geração essencial não vira sucesso vazio.

## Interfaces e aceitação

Inputs: eventos de interesse e níveis atuais. Outputs: jobs elegíveis para
CPU/GPU/IO e commits elegíveis para a dona. Entidades, Lua e banco não ficam
no grafo como ponteiros mutáveis acessíveis a workers.

Aceitação conjunta: raízes compartilhadas, 3×3, wrap, retorno invertido,
deadline, erro, cancelamento, dois jogadores e pressão de memória. Medir
revisitas de nó, tentativas sem progresso, tempo bloqueado por motivo e
dependentes liberados por conclusão. Contratos de bytes e backends estão em
[RAM](memory-and-prefetch.md), [GPU](gpu-compute.md) e [IO](io-persistence-network.md).
