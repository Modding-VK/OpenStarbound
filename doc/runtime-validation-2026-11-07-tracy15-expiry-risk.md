# Validação do escalonador por risco de expiração — Tracy 15

**Build observada:** `c36e4dba19c3f792ae30a4010d5e7da605ba7ed2`  
**Objetivo:** exercitar o preview `generationExpiryRiskSchedulerPreview` e os
marcos por pedido visível durante geração de setores novos.

## Artefatos e cenário

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `15.tracy` | 2.286.756 bytes | `7e906659c4803c8dc8a8d30ef5dd3bc49076a6abd7fa38ad58e69bb457cebdb1` |
| `starbound.config` | 10.282 bytes | `f2e1c7eccce6b0305fabca8387e5c8e71e589177ba75b13d447c05296866fdc6` |
| `exploration.zip` | 22.407.793 bytes | `70b13e97e73d2e7da726fc6f0c5977b56d9f669806f3414133559f7bf89032b6` |
| `starbound.log` | 61.378 bytes | `604ff69497a25850532e53767e6d1d05073c0feb23803c4bffff428c55034168` |

O ZIP passou na integridade. O log confirma a build e encerramento normal, sem
`Access violation`, `MapException` ou `WorldStorageException`. Há avisos de
assets de mods, sem exceção de mundo relacionada.

A configuração confirma profiling regional, política `adaptive`, pool automática
e os previews de decode, alimentador/dependências, resultados prontos, fairness,
prioridade persistente e risco de expiração. A opção nova estava em
`true`, com janela de 3.000 ms.

O arquivo de mundo contém janelas acumuladas de sessões anteriores. A sessão
atual foi isolada pela descontinuidade de 12.166 s no relógio monotônico: 21
janelas, 42,906 s, mundo `CelestialWorld:383738451:-48324174:-267567852:4:3`,
instância 14. Métricas abaixo usam somente esse recorte; não somam o arquivo
inteiro.

## Resultado funcional

| Medida | Resultado |
| --- | ---: |
| Setores completos / expirados | 479 / 29 |
| Profundidade média da fila | 47,64 |
| Idade média / máxima da fila | 6,094 / 11,902 s |
| Espera média / máxima de setor | 3,189 / 11,905 s |
| Promoções / tentativas / avanços por risco | 541 / 515 / 291 |
| TTL restante médio / máximo na promoção | 933 / 1.000 ms |
| Planos de dependência / truncados | 541 / 0 |
| Admissões BaseTiles da cadeia | 163 |
| BaseTiles agendados / aplicados / commitados | 332 / 332 / 317 |

O preview foi acionado em todas as passagens visíveis medidas e obteve avanço
em 291 tentativas reservadas. Não houve crash, deadline bloqueado ou plano de
dependência truncado.

## Marcos por pedido

Os novos eventos foram observados: `tilesAvailable` (79), `baseTilesReady`
(78), `microDungeonsReady` (129), `caveLiquidReady` (131), `finalizeReady`
(135), `expiryRiskPromoted` (241 dentro do limite de retenção por janela),
`completed` (133) e `expired` (16). Contadores não limitados são a fonte para
totais; os arrays de eventos são deliberadamente limitados por janela.

O maior marco observado revela a cadeia restante: Tiles chegou em até 74,805
ms e BaseTiles em 250,543 ms, enquanto microdungeons, líquidos e finalização
chegaram até 5,669 s, 7,995 s e 9,450 s. Isso sustenta que o próximo recorte,
caso o A/B confirme o TTL, deve mirar o pré-requisito serial posterior a
BaseTiles, não ampliar a pool.

## Tracy oficial

Exportado com `tracy-csvexport` 0.11.2,
`wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`.

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 2.829 | 0,894 ms | 4,421 ms | 9,184 ms | 89,494 ms |
| `WorldStorage generate queue` | 690 | 1,171 ms | 5,824 ms | 14,594 ms | 25,205 ms |
| `WorldServer entities` | 2.782 | 0,163 ms | 0,348 ms | 0,503 ms | 2,816 ms |
| `Base tiles worker prepare` | 332 | 1,308 ms | 3,321 ms | 4,700 ms | 6,535 ms |
| `Generate microdungeons` | 641 | 0,537 ms | 0,946 ms | 1,220 ms | 2,407 ms |
| `Generate cave liquid` | 283 | 0,014 ms | 0,252 ms | 2,724 ms | 3,017 ms |

## Decisão e próximos passos

O resultado é favorável: não há evidência de custo excessivo na thread do mundo
e a espera máxima ficou abaixo dos ~16 s observados antes. Porém, a duração é
curta e não existe uma run `false` de mesma rota/duração/configuração; isso não
estabelece causalidade nem autoriza habilitar por padrão.

1. Repetir exatamente a rota por ao menos 180 s com a opção `false` e `true`.
2. Comparar expirações por minuto, p95/p99 de `sectorWait`, marcos por pedido e
   p95/p99 das zonas Tracy.
3. Se a redução persistir, implementar preview limitado que, após promoção de
   risco bloqueada, priorize o pré-requisito imediato de MicroDungeons/
   CaveLiquid na thread dona; não externalizar NPCs, scripts ou publicação.

Complementa [regional-runtime-expiry-risk-validation-protocol.md](regional-runtime-expiry-risk-validation-protocol.md)
e o [checklist operacional](regional-runtime-execution-checklist.md).
