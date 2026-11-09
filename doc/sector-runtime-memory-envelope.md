# Envelope de memória da geração: cobertura e pressão

Estado: H0 / RAM-01, `Experimental`, 09/11/2026.
[Programa](sector-runtime/README.md) · [Inventário de ownership](sector-runtime/memory-and-prefetch.md)
· [Roadmap](sector-runtime/roadmap.md) · [Relatório da entrega](sector-runtime-h0-memory-envelope-2026-11-09.md)

Este contrato delimita o **subconjunto privado de geração contabilizado**.
Não redefine RAM-01 como orçamento de todo o motor e não transforma estimativa
em limite superior. O [schema executável](../source/game/StarWorldGenerationMemoryEnvelope.hpp)
e seus [testes](../source/test/world_generation_memory_envelope_test.cpp) acompanham
este documento; a coleção principal continua com os mesmos 49 contratos.

## Três quantidades distintas

| Quantidade | Significado | Não significa |
| --- | --- | --- |
| ArrayPayload | sizeof dos arrays privados BaseTiles/halo reservados | Pedido completo de malloc, todos os heaps dos tiles ou estado autoritativo publicado |
| AllocatorRequest | Bytes solicitados pelo allocator rastreado, incluindo header/padding e elementos inline | RSS, malloc usable size ou backing profundo de cada elemento |
| Estimate | Fórmula explícita para parcelas conhecidas, observadas ou previstas | Censo físico instantâneo, cobertura integral ou garantia hard de heap |

`trackedBytes` é a soma atual das três parcelas. Inclui previsões/sobreposição
quando retidas pelo ledger, não apenas bytes residentes que malloc entregou.
Os picos históricos não são somados ao uso atual nem usados como pressão atual.
Não somar picos independentes como se tivessem ocorrido simultaneamente.

## Schema das 14 classes

Os IDs são os de WorldGenerationMemoryClass; uma validação em compile time exige
classificar cada enum exatamente uma vez e na ordem. Always significa que a
cobrança básica existente não exige profiling; não certifica todo heap daquela
categoria. RegionalProfiling descreve a condição de criar a estimativa: tokens
já criados continuam retidos até o fim de seu lifetime.

| IDs / classes | Quantidade | Coleta | Fronteira e exclusões principais |
| --- | --- | --- | --- |
| 0 BaseTilesArray / 1 MicroDungeonHalo | ArrayPayload | Always | Reserva antes de construir/copiar; fora: wrappers/control blocks e heaps não incluídos no array |
| 2 TemplateJsonEstimate | Estimate | RegionalProfiling | Observação pós-construção de owners compartilhados; template e Json enfileirado deduplicam no mesmo registro; scan/SSO/overhead parciais |
| 3 SnapshotMetadataEstimate / 4 MicroDungeonPlanEstimate | Estimate | RegionalProfiling | Observações/fórmulas de objetos e containers conhecidos; não todos os índices/ponteiros externos ou picos de construção |
| 5 WorkerTemplateEstimate | Estimate | Always | Objeto/cache e storage solicitado da LRU observado na fronteira do job; não todo layout/biomas/seletores nem picos internos durante o job |
| 6 MicroDungeonPlanBuffer / 7 MicroDungeonCandidateBuffer / 8 MicroDungeonDistributionBuffer / 10 MicroDungeonNameBuffer | AllocatorRequest | Always | Reserva antes do raw allocator, incluindo overlap de realocação; não backing dos elementos nem malloc overhead |
| 9 MicroDungeonDistributionEstimate | Estimate | Always | Original adotado após construção; previsões/reconciliação do bundle; Json opcional separado na classe 2, sem cobrança duplicada |
| 11 MicroDungeonNameEstimate / 12 MicroDungeonCandidateEstimate | Estimate | Always | Previsão de parcelas conhecidas e reconciliação; SSO/privados e previsões intermediárias não são heap completo |
| 13 MicroDungeonTemporaryEstimate | Estimate | Always | Listas originais observadas e cópia/saída previstas; capacidade retida após clear; não picos anteriores à adoção |

Os lifetimes físicos e fórmulas detalhadas continuam no inventário e relatórios
dos recortes. Fechar mundo/facade ou cancelar logicamente um pedido não apaga
reservas ainda retidas por workers, promises, resultados ou bundles. Publicar
um array transfere-o para o mundo vivo, fora do ledger privado, sem afirmar que
a RAM foi fisicamente liberada.

## Coerência não é completude

O envelope verifica, com soma protegida contra overflow:

- soma de bytes/recursos das classes contra totais do snapshot;
- totais de arrays, buffers e estimativas contra suas categorias;
- bytes sem recurso proprietário e consistência dos recursos Json/registro
  compartilhado.

O snapshot é obtido sob o mutex já existente; a conversão usa somente esse valor,
sem novos locks, alocações ou travessia de payloads. Ela não confere se cada
fórmula estima bem um heap nem se todos os produtores foram observados.
`coherent=true` não concede `CompleteHeap`: este permanece **false**, até vazio.
Em caso incoerente/overflow, não usar os totais parciais para recomendar admissão.

`historicalIncompleteUpdates` é diagnóstico acumulado do ledger, não o número
de owners atualmente incompletos. Não bloquear para sempre porque houve uma
falha antiga, nem interpretar zero como prova de cobertura. Falta um censo
processual da qualidade atual dos owners; flags locais continuam separados.

## Exclusões assumidas neste escopo

Não cobertos integralmente: estado autoritativo de WorldStorage, entidades/Lua,
assets/caches globais, IO/decode salvo, Json ambiental e de templates privados,
layout/biomas/seletores, nós/allocator de TileDamage, malloc/control blocks não
rastreados, bookkeeping do próprio diagnóstico e picos anteriores à adoção.
RAM/VRAM de GPU não entram neste ledger. Memória unificada exigirá regra própria
para não contar o mesmo buffer duas vezes.

Json comum/assets podem reter backing após o último tracker. Capacidade/SSO
podem superestimar e exclusões podem subestimar. Um saldo zero do ledger não
prova RSS zero nem desalocação global. Nenhuma margem ou teto total é inferido
automaticamente dessas parcelas.

## Modelo de pressão implementado, isolado da admissão

WorldGenerationMemoryPressurePolicy é um modelo owner-local, sem callbacks,
locks, alocação, relógio, SO ou modelo de CPU. Não há consumidor de admissão no
runtime nesta entrega: o jogo **não** pausa/cancela trabalho por suas decisões.
Não há chaves novas de config, orçamento finito padrão ou ajuste de workers.

O chamador futuro fornece limites explícitos de um escopo declarado:
`resumeBytes < softBytes < hardBytes`. Por padrão o modelo está desligado,
retorna Unrestricted e não escolhe valores conforme a máquina. Limites ativados
inválidos, envelope incoerente ou demanda desconhecida recusam a recomendação
de pedido, mas nunca a drenagem existente.
Entradas inválidas também preservam a última pressão válida para a histerese:
uma falha transitória não pode autorizar recuperação precoce na próxima amostra.

| Pressão observada | Crescimento especulativo | Visível / dependência | Trabalho existente |
| --- | --- | --- | --- |
| Normal | Somente até o limite soft | Até o headroom hard | Drenagem preservada |
| Soft | Adiar novos bytes | Até o headroom hard | Drenagem preservada |
| Hard | Não crescer | Não crescer | Reuso sem bytes novos e drenagem preservados |
| Invalid | Não recomendar pedido | Não recomendar pedido | Drenagem preservada; corrigir a observação/contrato |
| Unrestricted | Política desligada, sem garantia de limite | Política desligada, sem garantia de limite | Nenhuma decisão de runtime é alterada |

Com limites/observação/demanda válidos, pedido de zero bytes pode ser aconselhado
em Normal/Soft/Hard. Isso pressupõe ausência real de crescimento; não autoriza
um kernel que aloca a declarar zero nem altera as cotas de CPU/publicação.

Hard entra em uso atual >= hard. Ao cair abaixo de hard, vira Soft enquanto
uso >= soft ou a recuperação ainda estiver acima de resume. Só volta a Normal
em uso <= resume; assim não oscila junto ao soft. Pedido recusado não vira
consumo nem aumenta pressão por si só. Pedidos aceitos podem chegar exatamente
ao limite correspondente; o próximo snapshot refletirá a fronteira atingida.
Comparações usam headroom por subtração verificada, sem soma que possa dar wrap.

## Fronteira para a próxima integração

Uma recomendação de snapshot **não é reserva atômica**. Outro worker pode
alocar entre observação e uso. Antes de impor um limite real:

1. Delimitar quais reservas/produtos o limite governa e testar a admissão sob
   o lock/ownership processual apropriado, incluindo overlap e rollback.
2. Preservar headroom e progresso de dependências/consumo: soft/hard sozinhos
   não provam ausência de deadlock com resultados bloqueados e caches retidas.
3. Usar controlador processual único e fairness/cotas explícitas entre mundos;
   não multiplicar o orçamento por WorldGenerator ou instância de policy.
4. Definir pressão externa, limites do usuário/hardware e memória unificada
   com observações válidas. Esta API não detecta RAM livre nem oferece teto RSS.
5. Validar MOD-01/VAL-02 e streaming equivalente antes de ampliar admissão ou
   ligar comportamento por padrão. Um protótipo H1 em sombra pode usar escopo
   delimitado sem exigir contabilizar cada heap do motor.

Evicção, cancelamento e mudança de ownership continuam contratos separados.
Não liberar buffers ainda em uso nem mudar TTL, ordem/RNG, save, callbacks ou
publicação para satisfazer uma recomendação de pressão. A prioridade segue
entrega de setores comuns; utilização de hardware e budgets maiores não são
ganho demonstrado de throughput.
