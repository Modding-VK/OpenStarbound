# Protocolo de validação — risco de expiração da fila setorial

**Objetivo:** determinar se `generationExpiryRiskSchedulerPreview` reduz pedidos
expirados e a espera de setores novos sem regressão material da thread do mundo.
O protocolo é para a build que contém `fd33ebe2` ou sucessora compatível.

## Variantes

Execute duas capturas, em ordem alternada se possível, com o mesmo executável,
save, seed/mundo, mods, jogador, rota de voo, duração e demais chaves de
`worldThreading`:

```json
"explorationProfiling" : {
  "enabled" : true,
  "intervalSeconds" : 2,
  "regionalDetails" : true,
  "writeReportsToFiles" : true
},
"worldThreading" : {
  "generationExpiryRiskSchedulerPreview" : false
}
```

Na variante experimental, trocar somente para:

```json
"generationExpiryRiskSchedulerPreview" : true,
"generationExpiryRiskMilliseconds" : 3000,
"generationVisibleChainContinuationPreview" : true,
"generationDependencyMicroDungeonPreparationPreview" : true,
"generationDependencyMicroDungeonCandidateLimit" : 4
```

Preservar também os previews de BaseTiles/dependências que já estiverem ativos
na linha de base. Não comparar sessões de planetas, rotas, duração ou conjuntos
de mods diferentes.

## Percurso e artefatos

1. Entrar em região ainda não gerada e voar continuamente por novos setores,
   incluindo céu aberto, por ao menos 180 s.
2. Evitar teleporte, troca de mundo, pausa longa ou regeneração de config no
   intervalo medido.
3. Encerrar normalmente e preservar `starbound.log`, `exploration.zip`, trace
   Tracy e a cópia integral de `starbound.config` usada em cada variante.
4. Registrar hashes, build/commit e nomes originais no próximo relatório;
   artefatos brutos não entram no Git por padrão.

O escritor de exploração inicia um JSONL primário novo para cada lançamento
quando encontraria o mesmo `context-instance`; a sessão anterior é rotacionada
conforme `reportFileBackups`. Usar o arquivo primário sem sufixo numérico na
coleta atual; backups são históricos e não devem ser somados à run nova. Cada
linha também identifica `sessionStartedMonotonicMilliseconds`.

## Métricas e decisão

| Métrica | Fonte | Resultado desejado |
| --- | --- | --- |
| `generation.sectorsExpired` | contadores do relatório | queda clara contra baseline |
| `generation.sectorWait` p50/p95/p99/máx | estágios do relatório | cauda menor, sem piora de média relevante |
| `generation.expiryRisk.*` | contadores/estágios | promoção, tentativa e avanço observáveis |
| `generation.visibleChainContinuation.*` | contadores/eventos | continuidade limitada sem repetição infinita |
| `generation.requests.expired` e eventos | detalhes regionais | pedidos em risco não terminam sem tentativa |
| `WorldServerThread update` p95/p99 | Tracy oficial | sem regressão material |
| `WorldStorage generate queue` p95/p99 | Tracy oficial | orçamento ainda respeitado |

O preview não pode tornar-se padrão somente por aumentar setores/s. Ele deve
reduzir expirações/espera sob cenário equivalente e manter compatibilidade de
saves, mods e multiplayer. Se a promoção ocorrer mas não avançar, o próximo
recorte é comparar o planejamento privado de MicroDungeons contra a mesma rota
antes de priorizar qualquer aplicação; se o update piorar, reduzir a janela de
risco antes de alterar líquidos.

## Instrumentação por pedido

Com `regionalDetails: true`, `generationRequests` agora contém, por pedido
visível, `tilesAvailable`, `baseTilesReady`, `microDungeonsReady`,
`caveLiquidReady`, `finalizeReady`, `terraformReady`, `expiryRiskPromoted`,
`completed` ou `expired`. Todos preservam época, sequência, setor, idade e
nível observado. Esses eventos permitem atribuir a espera a uma fase sem expor
trabalho recursivo de vizinhos como um falso pedido de jogador.
