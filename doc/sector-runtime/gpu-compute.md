# Backend GPU para preparação de setores

[Índice](README.md) · [CPU](cpu-and-stages.md) · [Compatibilidade](compatibility.md)

## Propósito e estado

Usar GPU disponível para reduzir o tempo de kernels regulares em lotes e
liberar CPU para outros produtores. O renderer atual negocia contexto OpenGL
moderno quando disponível, mas não fornece uma API de compute de worldgen.
Todo o backend deste módulo é proposto; não há geração GPU implementada.

## GPU-01 — Interface e capacidades

Definir um serviço independente de `WorldServer`/renderer:

```text
submit(kernelVersion, inputIdentity, immutableBuffers, parameters)
  -> completionTicket
poll(completionTicket) -> pending | immutableResult | failure
cancel(completionTicket) -> publicationRevoked
```

As operações são conceituais. O contrato comum suporta CPU de referência e
GPU opcional, com mesmos tipos de saída e validação. Os inputs não contêm
ponteiros para mundo, Lua, entidades, banco ou `Root`.

Primeiro backend candidato: compute OpenGL 4.3+ ou extensões equivalentes
realmente detectadas. Consultar limites de buffers, grupos de trabalho,
sincronização e memória; conseguir criar contexto 4.1 não comprova compute.
macOS/driver sem capacidade e servidor dedicado têm caminho CPU completo.
Não elevar o requisito gráfico mínimo para abrir saves existentes.

## GPU-02 — Thread de contexto e buffers em voo

Submissão ocorre na dona de um contexto válido, por fila explícita. Um
contexto separado só é usado com compartilhamento/sincronização auditados.
A WorldThread não chama métodos GL de um renderer pertencente a outra thread.

Usar buffers reutilizáveis, lotes de setores e múltiplas operações em voo.
Consultar fences sem bloquear o update e evitar readback síncrono por setor.
Persistência/mapeamento de buffers dependem de capacidade detectada.
Contabilizar staging RAM e VRAM em [RAM-01](memory-and-prefetch.md).

Cancelamento invalida publicação; a memória só volta à pool após a GPU
terminar de usá-la. Falha de contexto/driver invalida os tickets afetados,
recalcula na CPU e rejeita duplicatas de resultado que retornem depois.

## GPU-03 — Kernels e equivalência

| Ordem | Candidato | Condição de adoção |
| --- | --- | --- |
| Primeiro | máscaras/transformações inteiras densas | saída exata, ganho completo incluindo transferências |
| Seguinte | campos de terreno/bioma de operadores suportados | mesmos valores/decisões de geração em todo domínio coberto |
| Seguinte | classificação de candidatos sobre campos | nenhuma perda ou reordenação semântica |
| Após kernel CPU | partes puras de líquidos/Finalize | snapshot, conflitos e deltas aprovados |
| Frente visual separada | iluminação/preparação gráfica | ganho visual medido, sem confundir com entrega autoritativa |

Ruído float, FMA e precisão de driver podem mudar decisões em fronteiras.
Operador sem equivalência fica na CPU. Aproximação só pode filtrar candidatos
se houver prova de limite de erro e cobertura CPU de todos os casos
necessários; um epsilon arbitrário não é essa prova.

Não mudar matemática/seed/distribuição para caber no shader e declarar o
resultado equivalente. Uma geração intencionalmente diferente exigiria
versionamento de mundo e decisão de produto fora deste trabalho.

## GPU-04 — Comparação em sombra

Executar CPU e GPU sobre os mesmos inputs e comparar resultados, sem publicar
saída GPU divergente. Cobrir coordenadas negativas/canônicas, wrap, bordas,
casos de decisão próximos do limite e múltiplos drivers. Guardar versões,
contagem de divergências e reprodutor numérico mínimo, sem payload de mod.

Para RNG, fornecer fluxos explícitos aprovados e preservar consumo/ordem.
Um resultado GPU aprovado ainda precisa de revalidação do input no momento
do commit; equivalência de kernel não substitui validade do ticket.

## GPU-05 — Seleção heterogênea e render

Comparar custo CPU com espera GPU + upload + dispatch + fence + readback +
materialização. Escolher backend por classe/tamanho e prazo, com histerese.
Lotes antecipados podem amortizar transferências; setor urgente pode ter
menor latência na CPU. GPU ociosa é capacidade disponível, não prova de ganho
para qualquer tarefa.

Limitar duração de dispatch e serviço de compute conforme prazo de render.
Uma carga longa já iniciada pode não ser preemptível. O perfil alto throughput
amplia uso conforme metas do usuário, observando ganho de entrega e resposta.

Single-player pode conectar servidor local ao serviço disponível no processo.
Em multiplayer, a GPU de um cliente remoto não vira autora do mundo do
servidor. GPU de servidor dedicado é expansão posterior com contexto próprio;
nenhuma parte do grafo deve depender dela para funcionar.

## Melhoria esperada e aceitação

Esperam-se maior vazão de campos densos e menor ocupação CPU por esses
kernels. O ganho pode desaparecer em tarefas pequenas por custo de transporte.
Medir tempo completo por kernel/lote, bytes transferidos, espera, duração de
dispatch, divergência, fallback e efeito no frame.

Aceitação: equivalência CPU/GPU, resultado cancelado nunca publicado,
recuperação de falha, ganho ponta a ponta e execução CPU independente. A
estrutura do backend pode avançar em paralelo a H1/H2; a adoção segue H4/H5
do [roadmap](roadmap.md).
