# Validação da preparação da cadeia visível — Tracy 14

**Build observada:** `f78a02021a6721847c3c9cf3fa791363184bc7ca`  
**Objetivo:** validar o preview de preparação de `BaseTiles` para a fronteira
de pré-requisitos de setores visíveis, com foco no carregamento de setores
novos durante voo.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `14.tracy` | 9.913.666 bytes | `45160c7d9ee6b000aa6f5e1c8bd203e662ba522aa4b28a1c3f4f9c45fb603c4f` |
| `exploration.zip` | 22.096.870 bytes | `c708b5acb72227cccced3c170b0994000ab3c9102a95615843ae3905c97bbffd` |
| `starbound.log` | 72.246 bytes | `b49c17e4f8b5a3d76db57394dd413d4f854e92364eca7f9fcaf4ef58aac847b8` |

O ZIP passou no teste de integridade. O log identifica a build acima e termina
com `Application: stopped gracefully`; não há `Access violation`,
`MapException` ou `WorldStorageException`. Existem avisos de assets de mods
ausentes, já presentes fora desta alteração e sem exceção de mundo associada.
A configuração integral não foi anexada, mas os contadores comprovam que os
previews de alimentador e de cadeia foram exercitados.

## Resultado da cadeia de preparação

O mundo `CelestialWorld:383738451:-48324174:-267567852:4:3`, instância 14,
cobriu 117 janelas / 232,631 s.

| Medida | Resultado |
| --- | ---: |
| Planos de dependência / truncados | 1.863 / 0 |
| Raízes visíveis / nós de fronteira | 1.863 / 7.206 |
| Tentativas / admissões BaseTiles pelo alimentador | 1.126 / 1.094 |
| BaseTiles agendados / aplicados / commitados | 2.115 / 2.114 / 2.075 |
| Setores completos | 1.638 (7,04/s) |
| Workers BaseTiles ativos médios / disponíveis | 0,118 / 11 |
| Profundidade média da fila | 63,85 |
| Espera média / máxima de setor | 2,715 / 16,375 s |
| Idade média / máxima da fila | 7,663 / 16,366 s |

Portanto o recorte novo funciona tecnicamente: não truncou planos, não criou
crash e efetivamente preparou a fronteira de vizinhos. Porém, não demonstrou
redução da cauda de espera: o máximo continua perto de 16 s, como no A/B lunar
anterior. A amostragem de ocupação de worker segue baixa; ela não mede CPU
contínua, mas é consistente com admissão limitada pelo caminho dono.

## Gargalo confirmado: seleção da fila visível

| Sinal | Resultado |
| --- | ---: |
| Pedidos admitidos / completos / expirados | 2.963 / 1.638 / 1.301 |
| Amostras em que o mais antigo não era a cabeça | 2.202 / 3.061 (71,9%) |
| Promoções de justiça / tentativas reservadas / avanços | 736 / 743 / 449 |
| Orçamento adaptativo médio | 23,57 ms por passagem |
| Passagens limitadas por pressão prévia de update | 1 |

As linhas finais do log mostram repetidamente setores de nível 2–3, com 10–15
s de idade, `attempts=0` e `lastBlock=notSelected`. A justiça existente atua e
produz avanços, mas não sustenta throughput suficiente: 1.301 pedidos expiram.
Esta é a causa operacional mais forte para o setor novo permanecer atrasado
depois de sua preparação privada.

## Tracy oficial

Exportação com `tracy-csvexport` 0.11.2,
`wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`.

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 8.778 | 1,717 ms | 10,283 ms | 16,281 ms | 144,236 ms |
| `WorldStorage generate queue` | 2.166 | 1,309 ms | 8,956 ms | 14,709 ms | 133,301 ms |
| `WorldServer entities` | 8.732 | 0,321 ms | 6,899 ms | 8,981 ms | 23,554 ms |
| `Base tiles worker` | 2.770 | 1,776 ms | 4,151 ms | 5,211 ms | 31,455 ms |
| `Generate microdungeons` | 2.430 | 0,573 ms | 0,901 ms | 1,264 ms | 132,968 ms |
| `Generate cave liquid` | 987 | 0,015 ms | 3,543 ms | 4,373 ms | 6,355 ms |

## Decisão e próximos passos

Não ampliar a preparação de dependências ainda: ela já alimenta a fronteira e
o gargalo mensurável passou a ser a seleção serial de pedidos visíveis. O
próximo preview deve tornar a seleção sensível a idade/risco de expiração,
preservando prioridade de proximidade, a reserva de justiça existente e o
contrato de `WorldThread`. Critérios: reduzir expirações e p95/p99 de
`sectorWait` sem elevar materialmente `WorldServerThread update`.

Complementa [runtime-validation-2026-11-07-ready-scheduler-ab-moon.md](runtime-validation-2026-11-07-ready-scheduler-ab-moon.md)
e o [checklist operacional](regional-runtime-execution-checklist.md).
