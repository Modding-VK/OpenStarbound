# Progresso da geração e contratos para threads por setor

Estado em 03/11/2026: identidade de pedido, comparação observacional e a
primeira política experimental de justiça implementadas. A política permanece
desligada por padrão e não altera o modo legado.
Branch Experimental. Externalização de NPCs
suspensa conforme o backlog. Este documento complementa a
[auditoria dos 41 logs](log-audit-2026-11-03-deep.md) e a
[auditoria de dungeons](dungeon-access-audit.md).

## Casos reconstruídos

| Evidência | Estado observado | Conclusão permitida |
|---|---|---|
| Parte 0017, linhas 870 e 872, 18:27:47.794–18:27:48.287 | `[408,134]`, nível 3, idade 19,5–20 s, zero tentativas, `notSelected` | O setor antigo não foi escolhido no intervalo acompanhado. |
| Parte 0017, linha 881, 18:27:58.310 | Mesmo setor, 30 s, 40 tentativas, zero completas, zero avanços | Visitas à fila não garantem oportunidade de finalizar. |
| Parte 0017, linhas 882 e 885 | `[407,134]`, nível 3, 29,7–30,1 s; três tentativas sem tentativa completa | O padrão alcança mais de um setor. |
| Parte 0018, linha 358, 18:46:47.250 | `[499,134]`, nível 3, 24,3 s, `notSelected` | O padrão reaparece em outro trecho da exploração. |
| Parte 0018, linha 371, 18:46:57.951 | Conclusão de `[499,134]` com espera de 3,723 s | Coordenada não basta para ligar uma conclusão ao pedido antigo. |

O diagnóstico reinicia os contadores ao trocar o setor mais antigo e após
emitir um aviso. Não se pode interpretar zero como ausência de tentativas em
toda a vida do pedido. Também não existe evidência suficiente para atribuir o
reset de espera de `[499,134]` exclusivamente à expiração.

## Mecanismo confirmado no código

1. `queueSectorActivation` renova o TTL e move o pedido para a frente. A data
   de entrada é criada somente com coleta de desempenho ativa.
2. `WorldServer::update` fornece ordenação por distância plana ao jogador mais
   próximo; idade e bloqueadores não participam dessa prioridade.
3. `generateQueue` drena BaseTiles prontos, consumindo prazo e cota de níveis.
4. O primeiro pedido não concluído faz a passagem entrar em `schedulingOnly`.
   Isso ocorre para qualquer retorno incompleto, não apenas espera de worker.
5. Os pedidos seguintes só solicitam BaseTiles. Um setor no nível CaveLiquid
   retorna sem avanço; `lastBlock=none` significa ausência de bloqueio nessa
   chamada limitada, não que ele recebeu autorização de finalizar.
6. A passagem seguinte reordena por distância. Mover uma entrada para o fim
   não fornece justiça persistente entre passagens.

Há, portanto, evidência conjunta de atraso de seleção e acesso restrito às
etapas superiores. Isso não exclui dependências legítimas ou orçamento curto.
O limite deve continuar existindo; simplesmente remover `schedulingOnly`
mudaria a ordem de efeitos de estruturas sem estabelecer sua independência.

## Contratos por etapa

| Etapa | Leituras e escritas atuais | Contrato necessário no runtime por setor |
|---|---|---|
| BaseTiles | Preparação isolada, template privado; instalação valida identidade e revisões do setor | Resultado imutável; instalação pela thread dona; tentativa única por identidade e etapa; descarte seguro após unload. |
| MicroDungeons | Template, colocações ordenadas, `m_queuedPlacements`, promises, facade do mundo, alterações de tiles e entidades | Planejar sobre snapshots; registrar footprint real; reservar os donos envolvidos; publicar efeitos e promises uma vez em ordem estável. |
| CaveLiquid | Flood fill com wrap horizontal, materiais e IDs de dungeon; grava líquido/background na área acolchoada, inclusive vizinhos | Snapshot do halo e versões; plano de escrita por setor; coordenação dos donos e publicação coerente. A etapa anterior dos vizinhos não é um lock. |
| Finalize | Ajustes de tiles e colisão, grass, plantas, objetos e tesouros; acesso a vizinhos e mapa de entidades; ativação de líquidos | Separar cálculo local, planejamento das colocações e publicação; footprint das plantas/objetos; callbacks de ativação respeitam autoridade e dependências. |
| Terraform | Caminho especial, alterações de bioma e estado persistente | Identidade e revisões próprias; preservar ordem das edições e revalidar entradas; não tratar como Finalize comum. |
| Load/unload e persistência | Carregamento de entidades, metadados, TTL, serialização e banco | Dono definido, handoff confirmado, revogação de pedidos e checkpoint coerente; nenhuma referência mutável sobrevive ao unload sem contrato. |

Fontes: `StarWorldStorage.cpp` (`queueSectorActivation`, `generateQueue`,
`generateSectorToLevel`), `StarWorldGeneration.cpp` (`generateSectorLevel`,
`generateMicroDungeons`, `generateCaveLiquid`, `prepareSector`) e
`StarWorldServer.cpp` (`update`). O acolchoamento das microdungeons não prova
o limite de todos os efeitos de objetos, plantas e callbacks.

## Recorte de implementação proposto

### A. Identidade de pedido independente do profiling

- Identificar cada permanência na fila por mundo/encarnação, setor e sequência
  de admissão. Separar essa identidade daquela da tarefa BaseTiles.
- Renovar interesse preserva sequência e idade; expiração, conclusão e unload
  encerram o episódio; nova entrada recebe nova sequência.
- A política de execução precisa funcionar com HUD e profiling desligados.
  Não reutilizar `m_generationQueuedAt` como única fonte de prioridade, pois
  seu ciclo de vida depende da coleta.
- Eventos limitados devem conter identidade, nível atual/alvo e motivo terminal.
  Contadores agregados continuam corretos quando os detalhes forem descartados.

### B. Seleção e dependências

- Separar candidato visível, preparação independente e elegibilidade para
  publicar efeitos. Preservar os critérios atuais de carregamento e TTL.
- Introduzir uma oportunidade limitada de tentativa completa para o pedido
  visível mais antigo ainda solicitado, além da prioridade por proximidade.
  O orçamento de tempo/níveis é compartilhado, sem trabalho extra ilimitado.
- Se esse pedido depender de vizinhos, transmitir sua prioridade aos
  pré-requisitos. Evitar repetir visitas que só confirmam BaseTiles já prontos.
- Desempatar com sequência e coordenadas normalizadas; a ordem de término de
  workers não define prioridade de aplicação.
- Primeiro comparar seleção atual/proposta sem executar os efeitos alternativos.
  Depois habilitar a política experimental mediante validação dos conflitos.
  Isso preserva uma referência para entender alterações na geração.

Esse desenho não garante espera máxima absoluta: uma dependência externa ou
etapa indivisível pode continuar atrasando. A garantia pretendida é oportunidade
de progresso para um conjunto finito e continuamente solicitado com orçamento
positivo, mais causa explícita quando esse progresso não for possível.

### C. Execução por threads donas

- Manter a premissa de uma thread por região lógica carregada. A pool de
  preparação auxilia os donos, mas não substitui a thread local da região.
- Avançar primeiro operações com leitura imutável e escrita local comprovadas.
  Publicação com efeitos em vizinhos exige o contrato correspondente da tabela.
- O grafo atual de dependências é observacional e pode ser truncado. Não usar
  sua fronteira incompleta para autorizar publicação. O grafo executivo precisa
  representar todas as dependências ou adiar a operação explicitamente.
- Chamadas legadas sem footprint conhecido precisam de caminho coordenado
  explícito. Não executar Lua ou callbacks arbitrários em setores concorrentes
  apenas por sua âncora estar em setores diferentes.

## Critérios de validação antes de adotar a política

1. Pedidos renovados mantêm idade; reentrada após expiração/unload tem nova
   identidade; não há instalação de resultado de encarnação antiga.
2. Com conjunto finito, orçamento positivo e dependências concluíveis, todos
   os pedidos recebem tentativas completas e chegam ao estado final.
3. Zero orçamento, saturação de pool, mudança de rumo e dois jogadores não
   geram loops, crescimento ilimitado de filas ou execução fora de interesse.
4. Comparar mesma seed, assets/mods e rota com referência: tiles, estruturas,
   IDs, entidades, líquidos, colisão, saves e reload. Qualquer diferença precisa
   ser explicada antes de declarar compatibilidade.
5. Reduzir idade dos pedidos e passagens sem avanço; medir também frame do
   cliente e ciclo completo da thread, incluindo `sync`, para não transferir
   a latência para outro ponto.
6. Separar ganhos do agendador dos ganhos de paralelismo. Workers ocupados ou
   mais setores preparados não demonstram conclusão mais rápida por si só.

## Política experimental de justiça

`worldThreading.generationFairnessSchedulerPreview` habilita uma promoção
limitada do pedido com menor sequência ainda presente na fila. A cada
`generationFairnessIntervalPasses` passagens (padrão 4; limitado a 1–60), a
fila é primeiro ordenada pela distância atual ao jogador e então esse único
pedido antigo é movido para a frente. O orçamento de tempo e de níveis continua
o mesmo; não há trabalho adicional garantido nem prioridade permanente.

Ao ser selecionado, o setor continua usando `generateSectorToLevel` com alvo
`Complete`. A rotina existente percorre os pré-requisitos de geração antes de
avançar a etapa dependente, portanto a oportunidade é herdada por eles dentro
da chamada autoritativa. Esta versão não insere vizinhos como pedidos de jogador
nem permite publicar estruturas fora da ordem já existente.

Com profiling ativo, `generation.fairness.promoted` registra promoções e
`generation.fairness.alreadySelected` registra quando o pedido antigo já era o
mais próximo. Com `regionalDetails`, o evento limitado de pedido usa
`fairnessPromoted`. Comparar essa opção desligada e ligada na mesma rota é
obrigatório antes de adotá-la como padrão.

## Primeiro recorte implementado

`WorldStorage` mantém uma sequência por entrada e um epoch por instância de
armazenamento. Renovação preserva sequência/idade; conclusão e expiração
removem a identidade; reentrada recebe outra sequência. A identidade existe
com HUD e profiling desligados. O epoch é único no processo, não persistido
em save nem comparável entre execuções distintas.

O unload atual pode preservar interesse na fila: por isso `unloaded` é uma
observação não terminal, e não força cancelamento ou reentrada artificial.
Encerramento do mundo ainda não emite evento terminal individual para todos
os pedidos restantes; ausência de conclusão nesse caso não prova perda.

Com `regionalDetails`, a comparação seleciona a cabeça atual e o pedido de
menor sequência, sem alterar ordem, orçamento ou efeitos. Esse candidato
antigo ainda não inclui elegibilidade por footprint nem prioridade herdada;
é apenas a primeira medição da diferença entre distância e antiguidade.
O registro da cabeça ocorre antes da drenagem de resultados prontos, portanto
não prova que houve orçamento para executar essa cabeça na mesma passagem.

Eventos de ciclo de vida têm limite de 64 por janela e observações de seleção
limite separado de 16; contadores de descarte tornam a perda explícita. O
detalhamento e a varredura adicional são desligados com `regionalDetails`.
Sem profiling, permanece somente o custo de manter identidade/data por entrada.
Essa etapa não cria novos workers nem transfere a simulação para as threads
das regiões lógicas.
