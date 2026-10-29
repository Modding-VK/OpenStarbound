# Métricas do ciclo de vida das threads por setor

O preview `worldThreading.sectorThreadLifecyclePreview` é desligado por padrão.
Quando ligado, cada setor carregado tem uma thread que apenas espera o unload;
nenhum trabalho de simulação roda nela. A coleta deste documento também só é
ativada nesse modo. O servidor publica um resumo a cada 60 passos no mapa de
diagnóstico `server_<worldId>_*` e uma linha `Sector thread preview` no log
após 60 passos e depois a cada 300 passos.

| Chave | Significado |
| --- | --- |
| `sector_threads` | Threads atuais / setores com tiles carregados, divisão entre `Tiles` e `Entities`, pico de threads desde a abertura do mundo. A diferença entre os dois primeiros números aponta uma falha na correspondência 1:1. |
| `sector_thread_lifecycle` | Criações, encerramentos, falhas de criação e desligamento automático do preview. |
| `sector_thread_start_us` | p95, p99, máximo e soma, em microssegundos, desde o pedido de criação até a thread sinalizar que começou a esperar. |
| `sector_thread_stop_us` | p95, p99, máximo e soma, em microssegundos, para sinalizar a parada e concluir o `join`. |

Os percentis usam os **últimos 256 eventos** de cada tipo e são calculados
somente durante a publicação. As somas, máximos, contadores e pico cobrem a
vida inteira do mundo. Os tempos de criação/parada são tempo decorrido visto
pela thread do mundo; incluem espera pelo escalonador, não são tempo de CPU.
Uma falha de criação encerra as threads do preview, registra o motivo no log e
mantém o mundo no fluxo legado.

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
equivalente. A build diagnóstica Tracy identifica as threads do preview e as
zonas de criação/parada; sua visualização não transforma tempo em espera em
tempo de CPU. Estes contadores do sistema não são estimados pelo HUD, porque
medidas indiretas de stack ou de CPU por thread seriam enganosas. Nenhum desses
dados demonstra ganho de paralelismo da simulação enquanto as threads só
aguardarem o unload.
