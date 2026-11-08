# Validação dos bloqueadores de setores visíveis — Tracy 17

**Build:** `098b2fca9c03b6289df024d70af6656319222e20`  
**Objetivo:** validar os bloqueadores imediatos e a continuidade de cadeia para
setores comuns.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `starbound.config` | 10.461 bytes | `7096fe932d43a72e452bf87e40505597c5b7602bf6326ac24995320783549122` |
| `exploration.zip` | 1.693.959 bytes | `3a2754895fb6744e943f27b40ded4d9a3849ba4504f79c99a76e11e16a9bec4c` |
| `starbound.log` | 101.192 bytes | `94b54f8f122fb0842d617ecd6bcc4e94c4bf16b58e9777933ea1b1ad94a74ac3` |
| `17.z01` / `17.z02` / `17.zip` | 33.554.432 / 33.554.432 / 20.411.684 bytes | `26597900fd7cee46f452387e52dcb30934c5fdf5880390323856e521e86bc7a9` / `6f5eb1618663db339ba15e74322f5307ff6bf741d9e1f32cfc4276295db14846` / `bc01bd7cd26d0919dba1535a7f20682711347cdd472e0f991211c382026f0123` |
| `17.tracy` extraído | 87.513.477 bytes | `670a243359ed06120a3773898e689099e206ef3ce505b1f11979b42ea9f1813e` |

O ZIP de exploração passou na integridade e o log termina normalmente, sem
`Access violation`, `MapException` ou `WorldStorageException`. A configuração
confirma os previews de risco, continuidade, BaseTiles e MicroDungeons.

## Sessão e resultados

O relatório primário do mundo `CelestialWorld:383738451:-48324174:-267567852:6:3`,
instância 19, tem uma única sessão, 74 janelas / 148,469 s e `final: true`.
A rotação de relatórios funcionou.

| Medida | Resultado |
| --- | ---: |
| Setores completos / expirados | 734 / 259 |
| Profundidade média / espera máxima | 44,460 / 17,266 s |
| Planejamentos MicroDungeons do feeder: admissão / commitado / stale | 801 / 377 / 376 |
| Continuidade: agendada / retomada / tentada | 711 / 0 / 0 |

O primeiro bloqueador das tentativas `Complete` foi `microDungeons` em 1.087
casos, seguido por `baseTiles` (335), `levelLimit` (162) e `deadline` (105).
Não houve bloqueio imediato por líquidos ou finalização. MicroDungeons é a
causa dominante desta rota, mas os stales impedem ampliar essa admissão.

## Interação corrigida

A promoção de risco ocorreu nas 2.062 passagens e tinha precedência sobre a
continuidade, tornando-a inalcançável. A correção posterior a esta coleta dá
uma única tentativa à continuidade válida antes da próxima seleção de risco;
depois disso o risco volta a prevalecer. Não renova TTL ou aumenta orçamento.

## Tracy oficial

A concatenação ordenada das partes apresentou aviso de offset central no ZIP,
mas a captura extraída foi aceita por `tracy-csvexport` oficial 0.11.2,
`wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`.

| Zona | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 6,254 ms | 15,452 ms | 36,959 ms | 191,686 ms |
| `WorldStorage generate queue` | 1,103 ms | 21,338 ms | 63,512 ms | 168,120 ms |
| `WorldServer entities` | 4,197 ms | 8,385 ms | 12,534 ms | 172,470 ms |
| `Base tiles worker` | 2,158 ms | 10,353 ms | 15,542 ms | 154,721 ms |
| `Generate cave liquid` | 0,025 ms | 4,984 ms | 6,292 ms | 8,281 ms |

## Próximo passo

Repetir a rota com a precedência corrigida e medir
`visibleChainContinuation.scheduled/resumed/attempted`, expirações e
`sectorWait`. Se MicroDungeons continuar dominante, construir fila explícita de
resultados prontos revalidada e ordenada por visibilidade, sem externalizar
aplicação, entidades ou scripts.
