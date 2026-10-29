# Métricas do ciclo de vida das threads por setor

O preview `worldThreading.sectorThreadLifecyclePreview` é desligado por padrão.
Quando ligado, cada setor carregado tem uma thread. Sem
`worldThreading.sectorThreadMailboxPreview`, ela apenas espera o unload. Com a
segunda opção ligada, processa um ticket de metadados imutáveis por setor a
cada 60 passos. Nenhum trabalho de simulação roda nela. A coleta de métricas
de ciclo de vida ocorre com o primeiro preview; a coleta de tickets exige o
segundo. O servidor publica um resumo a cada 60 passos no mapa de
diagnóstico `server_<worldId>_*` e uma linha `Sector thread preview` no log
após 60 passos e depois a cada 300 passos.

| Chave | Significado |
| --- | --- |
| `sector_threads` | Threads atuais / setores com tiles carregados, divisão entre `Tiles` e `Entities`, pico desse mundo, total e pico do processo. A diferença entre os dois primeiros números aponta uma falha na correspondência 1:1 enquanto o preview estiver ativo; após falha de criação, o desligamento é esperado. O total do processo soma todos os mundos com o preview ativo. |
| `sector_thread_lifecycle` | Criações, encerramentos, falhas de criação, falhas em runtime (worker ou protocolo), despachos rejeitados por identidade/sequência e desligamento automático do preview. Um mailbox ocupado é contado separadamente em `sector_mailbox`. |
| `sector_thread_start_us` | p95, p99, máximo e soma, em microssegundos, desde o pedido de criação até a thread sinalizar que começou a esperar. |
| `sector_thread_stop_us` | p95, p99, máximo e soma, em microssegundos, para sinalizar a parada e concluir o `join`. |
| `sector_thread_owner_us` | Tempo gasto pela thread do mundo ao inspecionar falhas e, se habilitado, recolher e enviar tickets. p95/p99 dos últimos 256 passos de coleta, máximo, soma e amostras. |
| `sector_mailbox` | Estado ativo do mailbox e tickets enviados, concluídos, aceitos, obsoletos, cancelados no unload e não enviados por mailbox ocupado. Continua visível após desligamento por falha para preservar os contadores finais. |
| `sector_mailbox_us` | p95/p99 de espera entre envio e início do cálculo, e de duração do cálculo; janela de 256 resultados. |

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

O mailbox comporta uma entrada e um resultado por setor. O worker só recebe
coordenada do setor, sequência, incarnação, níveis de carga/geração e revisão dos tiles;
não recebe ponteiro para o mundo. O dono compara o resultado com os metadados
atuais. Uma rejeição `stale` significa que o setor mudou durante a viagem da
mensagem; o ticket nunca altera tiles, entidades, scripts ou save. Os tickets
submetidos que ainda não terminaram não são contados como `completed`.

## Revisão estática antes da primeira build

- A thread começa após instalar os tiles e antes do callback de carga; uma
  falha de leitura anterior não deixa uma thread para um setor ausente.
- Uma segunda carga do mesmo setor reutiliza a thread registrada; o unload
  retira o registro e aguarda `join` antes de aceitar uma nova incarnação.
- A mailbox só aceita a coordenada e a incarnação de sua thread, com sequência
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
