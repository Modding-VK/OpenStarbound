# Métricas do ciclo de vida das threads por região lógica

O preview `worldThreading.logicalRegionThreadLifecyclePreview` é desligado por
padrão. Quando ligado, cada região lógica de até quatro setores físicos 32×32
tem uma thread enquanto ao menos um de seus setores estiver carregado. Sem
`worldThreading.logicalRegionThreadMailboxPreview`, ela apenas espera o unload.
Com a segunda opção ligada, processa um ticket de metadados imutáveis de um
setor representativo por região a cada 60 passos. Nenhum trabalho de simulação
roda nela. Os nomes antigos `sectorThreadLifecyclePreview` e
`sectorThreadMailboxPreview` continuam aceitos por compatibilidade. A coleta de métricas
de ciclo de vida ocorre com o primeiro preview; a coleta de tickets exige o
segundo. O servidor publica um resumo a cada 60 passos no mapa de
diagnóstico `server_<worldId>_*` e uma linha `Logical region thread preview` no log
após 60 passos e depois a cada 300 passos.

| Chave | Significado |
| --- | --- |
| `logical_region_threads` | Threads atuais de região / setores físicos carregados, divisão entre `Tiles` e `Entities`, pico desse mundo, total e pico do processo. O total do processo soma todos os mundos com o preview ativo. |
| `logical_region_thread_lifecycle` | Criações, encerramentos, falhas de criação, falhas em runtime, despachos rejeitados e desligamento automático. |
| `logical_region_thread_start_us`, `logical_region_thread_stop_us` | p95, p99, máximo e soma, em microssegundos, para criação ou parada com `join`. |
| `logical_region_thread_owner_us` | Tempo gasto pela thread do mundo ao inspecionar falhas e, se habilitado, recolher e enviar tickets. |
| `logical_region_mailbox`, `logical_region_mailbox_us` | Estado do mailbox e p95/p99 de espera e cálculo, em janela de 256 resultados. |

O pico do processo vale desde o início da execução, inclusive mundos que já
foram descarregados. Os percentis usam o posto mais próximo (*nearest rank*)
dos **últimos 256 eventos** de cada tipo e são calculados
somente durante a publicação. As somas, máximos, contadores e pico cobrem a
vida inteira do mundo. Os tempos de criação/parada são tempo decorrido visto
pela thread do mundo; incluem espera pelo escalonador, não são tempo de CPU.
O tempo `owner_us` separa o custo da distribuição de tickets do tempo que eles
aguardam nos workers; não inclui o custo de formatar e publicar o resumo.
Uma falha de criação, de worker ou de protocolo encerra as threads do preview, registra o
motivo no log e mantém o mundo no fluxo legado.
Quando a falha parte de um worker, o aviso inclui a coordenada do setor.

O mailbox comporta uma entrada e um resultado por região. O worker só recebe
coordenadas da região e de um setor representativo, sequência, incarnação,
níveis de carga/geração e revisão dos tiles;
não recebe ponteiro para o mundo. O dono compara o resultado com os metadados
atuais. Uma rejeição `stale` significa que o setor mudou durante a viagem da
mensagem; o ticket nunca altera tiles, entidades, scripts ou save. Os tickets
submetidos que ainda não terminaram não são contados como `completed`.

## Revisão estática antes da primeira build

- A thread começa após instalar o primeiro setor físico da região e antes do
  callback de carga; uma falha de leitura anterior não deixa uma thread viva.
- Cargas adicionais da mesma região reutilizam a thread registrada; somente o
  unload do último setor retira o registro e aguarda `join`.
- A mailbox só aceita a coordenada regional e a incarnação de sua thread, com sequência
  crescente. A resposta é conferida novamente contra coordenada, sequência,
  incarnação, níveis e revisão atuais antes de contar como aceita.
- O unload cancela o pedido ou resultado pendente. Uma falha na criação ou
  dentro do worker encerra o preview, preservando o processamento legado.
- Se a espera pela confirmação de início falhar após criar a thread, o
  construtor sinaliza a parada e faz `join` antes de propagar o erro. Uma
  divergência entre threads e setores carregados é registrada como aviso no log.
- O projeto já liga suporte a threads nas plataformas suportadas; a nova
  mailbox usa apenas a biblioteca padrão e a instrumentação Tracy existente.
  A API `tracy::SetThreadName` foi conferida no commit oficial de Tracy fixado
  pelo CMake; nenhuma dependência nova foi adicionada.
- Essa revisão examina contratos e fluxo de controle; compilação e observação
  em jogo ainda são necessárias para validar integração e custo.

Para avaliar o custo, comparar a mesma rota e o mesmo save com o preview
desligado e ligado. Usar junto o painel de desempenho do mundo (ticks lentos,
tempo ocupado e espera de setores), os dados de geração e o tempo até os tiles
ficarem visíveis. A diferença entre as duas execuções mede o efeito agregado
do preview; os tempos de criação e parada localizam parte desse efeito. Observar
também dois jogadores distantes e unload/reload repetido, pois ampliam o número
de threads ou a taxa de criação.

**Memória e escalonamento do processo** exigem medição do sistema operacional:
registrar memória residente/comprometida, total de threads e trocas de contexto
no Gerenciador de Tarefas/Monitor de Desempenho do Windows, ou ferramenta nativa
equivalente. A build diagnóstica Tracy nomeia as threads por coordenada de setor e marca as
zonas de criação/parada; sua visualização não transforma tempo em espera em
tempo de CPU. Estes contadores do sistema não são estimados pelo HUD, porque
medidas indiretas de stack ou de CPU por thread seriam enganosas. Nenhum desses
dados demonstra ganho de paralelismo da simulação enquanto as threads apenas
aguardarem ou processarem tickets de metadados.
