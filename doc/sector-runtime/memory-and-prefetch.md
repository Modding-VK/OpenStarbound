# RAM, produtos residentes e antecipação da rota

[Índice](README.md) · [CPU](cpu-and-stages.md) · [IO](io-persistence-network.md)

## Propósito e ganho esperado

Usar memória disponível para que o próximo setor e seus auxiliares já tenham
produtos calculados quando forem necessários. Evitar recomputação, decode
repetido e descarte de resultados úteis. A antecipação cresce com recursos
livres e probabilidade de consumo, mantendo limite processual verificável.

## RAM-01 — Reserva e contabilização de bytes

Reservar antes de alocar/copiar. Contabilizar inputs, outputs, campos,
templates privados, estruturas compiladas, staging e buffers transitórios.
Referência compartilhada deve contar bytes únicos, além do overhead de
índices/consumidores. Os contadores atuais de arrays são rasos e não representam
todo o custo de memória do motor.

Manter orçamento por processo e cotas por classe/mundo; vários mundos não
podem multiplicar silenciosamente um limite pensado para um só. Resultados
prontos preservam sua reserva até consumo/descarte e GPUs preservam buffers
até a conclusão física, mesmo após cancelamento lógico.

RAM e VRAM têm contabilidade separada em GPU dedicada. Em memória unificada,
os buffers GPU também ocupam o orçamento do sistema. Não contar o mesmo
buffer duas vezes nem presumir que VRAM anunciada esteja toda livre.

Primeiro recorte de H0 implementado: `WorldGenerationTaskPreparation` reserva
chave/bytes antes das cópias de BaseTiles/halo e desfaz uma preparação ainda
não submetida no retorno antecipado ou exceção. O registro da pendência é
alocado antes do dispatch; o handoff preserva a reserva normal até o drain.
O produto quantidade × tamanho é verificado contra overflow, sem alterar
os limites de tarefas/bytes existentes. Ver
[relatório e escopo](../sector-runtime-h0-memory-admission-2026-11-09.md).

O [recorte processual seguinte](../sector-runtime-h0-process-memory-2026-11-09.md)
adiciona um ledger compartilhado para as reservas de arrays privados, ligado
ao lifetime dos dados e não apenas à tarefa. Referências ao mesmo wrapper
contam uma vez; fechar o mundo não esquece buffers retidos pelo worker ou pela
promise. A publicação de BaseTiles transfere o array ao mundo vivo e encerra
sua classificação privada quando o último wrapper termina.

O ledger runtime é observacional, sem novo teto global; cotas locais continuam
iguais. O teto finito da API é usado apenas em testes isolados. Isso não mede
RSS e não conclui o orçamento processual de RAM-01.

### Inventário de ownership e cobertura atual

| Produto/recurso | Dono e término da retenção | Contabilidade atual |
| --- | --- | --- |
| Array BaseTiles privado | wrapper capturado pelo worker e devolvido na promise; até descarte/publicação e último wrapper | reserva processual de `sizeof(Array)`, única por grupo |
| Halo do planejador | wrapper do snapshot, incluindo capturas da closure; não é parte da lista de planos | quantidade de arrays × `sizeof(Array)`; grupo único |
| Arrays autoritativos após publicação | `WorldStorage`/`TileSectorArray`, até unload | fora do ledger privado; não foram liberados fisicamente ao publicar |
| Template serializado | `m_baseTileTemplateStore`/`m_microDungeonTemplateStore` e cópias de Json nos jobs | fora; deduplicar storage compartilhado antes de cobrar bytes |
| Template privado/cache do worker | `cachedWorkerTemplate` thread-local, até troca de revisão ou saída da thread | fora; pode sobreviver ao job/mundo, chave fonte é weak_ptr |
| Revisões/distribuições no snapshot | mapas/listas do snapshot e metadados pendentes | fora; overhead/capacidade profunda pendentes |
| Descritores de planos e strings | promise/resultado e pré-aquecimento, até consumo/descarte | fora; tamanho variável não equivale aos bytes do halo já destruído |
| Decode de tiles salvos | caminhos específicos de `WorldStorage` | fora deste recorte |
| Tokens, control blocks e allocator | ledger/wrappers/containers | fora dos bytes declarados; há overhead por grupo, não por tile |

RAM-01 permanece aberto para o orçamento processual completo e os produtos
restantes. Não ampliar admissão com base apenas nesta contabilidade parcial.

## RAM-02 — Camadas de cache e invalidação

| Camada | Chave mínima conceitual | Evento que invalida |
| --- | --- | --- |
| Template privado | mundo/revisão/contexto | mudança de layout/parâmetros/geração |
| Campo puro | seed, coordenadas, versão do kernel e inputs | mudança de qualquer input descrito |
| Candidatos/vazio | revisão do template e das colocações | nova distribuição/colocação relevante |
| Decode salvo | storage epoch, ticket, versão dos bytes | write/unload/recarga posterior incompatível |
| Estrutura estática | geração de assets e formato intermediário | reload de assets ou compilador incompatível |
| Resultado de snapshot vivo | encarnação e read set completo | edição semântica, unload ou conflito |

Uma cache não amplia a validade de um resultado. Conservar o output só é útil
se sua identidade e seus inputs puderem ser verificados. Cache de terreno
puro não pode substituir um setor salvo modificado pelo jogador.

## RAM-03 — Política de retenção

Reter trabalho de maior benefício esperado: interesse visível, auxiliares
compartilhados, rota próxima, custo de recomputar e histórico de reutilização.
Sob pressão, reduzir primeiro especulação distante e resultados baratos.
LRU isolada pode expulsar uma dependência crítica; combinar recência com
demanda e referências ativas, sem tornar toda entrada permanentemente pinada.

Limites soft/hard e histerese evitam oscilações. Estimar pressão externa com
memória própria reclamável em conta: ocupar uma cache não deve disparar o
esvaziamento da mesma cache numa sequência infinita. Expiração de cache e
TTL de setor ativo são mecanismos distintos.

## RAM-04 — Prefetch previsto e cobertura de dependências

Usar posição, velocidade, direção, destino conhecido e wrap. A faixa prevista
inclui os auxiliares das etapas, não só o retângulo visível imediato. Crescer
horizonte quando CPU/RAM/GPU estiverem livres e os resultados forem úteis;
encolher ao mudar de direção, teleportar ou acumular stale.

Reservar uma parcela configurável para antecipação enquanto houver backlog
visível, preservando a prioridade do setor urgente e a justiça entre jogadores.
Só preparar dados privados: não ativar scripts, entidades ou threads regionais
adicionais por manter um produto residente.

O prefetch atual de `WorldStorage`, que efetivamente carrega Tiles, conserva
seu lifecycle. A futura cache privada deve usar um estado distinto de carga;
misturar ambos alteraria TTL, callbacks e número de regiões carregadas.

## RAM-05 — Cancelamento e reaproveitamento

Retirar um consumidor não descarta um produto usado por outro pedido.
Cancelar um job invalidado sem reciclar buffers ainda em uso. Quando só muda
a prioridade, conservar o resultado válido e repriorizar o consumo. Quando
muda o input, recomputar ou revalidar apenas pelo contrato da camada.

Definir política de limpeza no fechamento de mundo e processo. Contextos
privados por worker precisam de teto e métricas de reconstrução para evitar
reter todos os mundos já visitados indefinidamente.

## Interfaces, métricas e aceitação

O [grafo](dependency-scheduler.md) fornece consumidores/prioridade; o governador
[CPU-05](cpu-and-stages.md) atribui capacidade; IO/GPU retornam produtos pelo
mesmo protocolo de resultado versionado.

Medir bytes únicos residentes e em voo, pico, falhas de reserva, hits úteis,
misses, expulsões, idade, taxa de consumo do prefetch e tempo economizado.
O volume cumulativo de snapshots não é pico de RAM. Medir também custo de
cópia e pressão de banda/cache: RAM abundante não elimina esse custo.

Aceitação: ganho em revisita/antecipação, limites respeitados sob vários mundos,
nenhum output stale publicado e ausência de crescimento persistente após
cancelamento/unload. Testar pouca memória, muita memória, GPU integrada,
mudança de rota e reload de assets. Ver [validação](validation-and-telemetry.md).
