# Auditoria de desempenho e continuidade — 07/11/2026

**Branch:** `Experimental`  
**Estado documentado:** após `3339fee2`  
**Objetivo:** referência versionada para retomar com segurança o trabalho de geração regional, uso de CPU e estabilidade.

Este é um relatório de evidência e planejamento. Não autoriza escrita concorrente no mundo vivo nem trata medições ausentes como fatos.

## Artefatos de origem

Os binários de diagnóstico não foram incluídos no Git: são capturas grandes e específicas da máquina/sessão. Os hashes permitem identificar os arquivos sem inserir logs de usuário ou traces volumosos no histórico do código.

| Artefato recebido | SHA-256 | Uso nesta auditoria |
| --- | --- | --- |
| `exploration.zip` (07/11/2026, 15.699.771 bytes) | `3e10f57ee7b2add8b097a7e9d6e5f46114613e46df36e83c6adb2f33c1ffd254` | Relatórios JSONL de geração, filas, mundo, entidades, líquidos e storage. |
| `starbound.log` (07/11/2026, 85.286 bytes) | `524773cccd971bbb5556d6b7eea92c86f5486cebea543968f03fbdb2799b2533` | Diagnóstico do crash ao aproximar-se de um setor. |

O trace Tracy associado à sessão não foi interpretado por parser nesta auditoria; nenhuma atribuição de zonas Tracy abaixo é inferida dele.

## Configuração observada

Não há cópia versionada da configuração completa anexada a esta coleta. Os contadores demonstram que `adaptive` estava ativo e que a pool automática selecionou 11 workers na máquina de teste (Ryzen 5 5600G, 12 CPUs lógicas informadas pelo usuário). A coleta também contém atividade de BaseTiles e planejamento de microdungeons.

As amostras de pool não são medição contínua do uso percentual de CPU do SO; elas demonstram capacidade disponível e pressão do pipeline.

## Resultado de desempenho

Agregação de 183 janelas do mundo `CelestialWorld:383738451:-48324174:-267567852:4`, instância 14, totalizando 789,934 s:

| Indicador | Resultado | Leitura permitida |
| --- | ---: | --- |
| Passagens com política ativa | 4.323 | `adaptive` foi exercitado. |
| Profundidade média da fila | 41,93 setores | Existe backlog autoritativo persistente. |
| Orçamento médio / máximo | 22,38 / 24 ms | O modo adaptativo alcançou o teto. |
| Recuos por pressão do update | 8 | A pressão raramente limitou o burst. |
| Conclusões observadas | 952 (1,205 setor/s) | Referência da rota, não benchmark controlado. |
| Maior idade / espera | 75,623 / 75,642 s | A fila ainda permite atraso alto. |
| Workers BaseTiles disponíveis | 11 em média | Seleção automática funcionou. |
| Workers ativos nas amostras | 0,22 em média | A pool não ficou alimentada. |
| Maior `serverUpdate` | 232,678 ms | Há picos além da geração. |

Bloqueios acumulados: 1.913 por BaseTiles, 2.598 por microdungeons e 8.394 por vizinhos. O prazo de geração esgotou 119 vezes. Aumentar somente workers ou orçamento não é a próxima correção de maior retorno: admissão, dependências e entrega/commit serial deixam capacidade de preparação ociosa.

BaseTiles em worker somou 7.485,90 ms em 1.882 resultados; planejamento de microdungeon em worker, 12.698,10 ms em 1.515 resultados; `CaveLiquid`, 1.802,48 ms em 1.162 amostras; e `Finalize`, 6.223,16 ms em 797 amostras. Esses escopos não devem ser somados como tempo total de frame. Líquidos e clima continuam backlog próprio: nesta rota, `liquids` não foi o maior custo agregado.

## Crash reproduzível e correção

O `starbound.log` registra duas ocorrências de:

```text
WorldStorageException: WorldStorage generation failed while generating from queue
MapException: Key '(91, 125)' not found in Map::get()
```

O encerramento do `WorldServer` explica o retorno à nave. A causa imediata era telemetria de seleção regional em `WorldStorage` assumindo que a cabeça da fila sempre possuía identidade de requisição; a divergência fila/identidade virava exceção fatal.

O commit `33e3a629` remove a chave do setor efetivamente processado, torna observações totais diante de entrada sem identidade, recupera identidade apenas em ativação posterior e mede `headWithoutIdentity`, `completionHeadChanged`, `completionMissingFromQueue`, `completedWithoutIdentity` e `identityRecovered`.

Foram executados compilação sintática de `StarWorldStorage.cpp`, `StarWorldGeneration.cpp` e `StarWorldServer.cpp`, além de `git diff --check`. Não houve build completo, Actions ou regressão Windows após essa correção.

## Arquitetura decidida

A `WorldThread` continua dona serial de tiles, entidades, Lua, rede, persistência e ordem observável. Ela deve coordenar commits, não executar toda preparação serialmente:

1. **Interesse:** ativações, geração inicial e prefetch.
2. **Preparação:** snapshots para BaseTiles, planos de microdungeon, IO privado e etapas puras, limitados por workers, bytes e memória.
3. **Resultados prontos:** revalidados e ordenados por visibilidade, idade e pré-requisitos.
4. **Commit:** publicação determinística pela thread dona.

IO paralelo ainda não existe. O recorte permitido é bytes imutáveis, descompressão/desserialização privada em worker, revalidação de encarnação/revisões e instalação pela thread dona. Banco, estado vivo, callbacks, entidades, saves e rede continuam fora do worker.

## Próxima sequência de trabalho

1. Construir no Windows e repetir a rota do crash com `regionalDetails: true`; confirmar ausência de `MapException`/retorno à nave e registrar os novos contadores de invariável.
2. Executar rota nova comparável com a mesma configuração e preservar novo `exploration.zip`, log e configuração completa.
3. O primeiro recorte do alimentador foi implementado depois desta coleta: o preview `generationPreparationFeederPreview` admite BaseTiles elegíveis sem esperar commit de cadeia bloqueada, sob orçamento de tempo, tarefas e bytes. Ele continua desligado por padrão; validar ocupação, equivalência e regressão Windows antes de qualquer adoção.
4. Criar fila explícita de resultados prontos e medir atrasos e descartes antes de alterar prioridade de commit.
5. Após equivalência de mundos/mods/saves, avançar IO privado e `CaveLiquidPlan` sobre snapshot/halo.

## Guardrails permanentes

- Compatibilidade com mods, saves e multiplayer bloqueia adoção por padrão.
- Externalização de NPCs continua suspensa.
- Linux Clang fica fora do escopo até o encerramento do projeto.
- GitHub Actions é manual; não houve execução para os commits desta auditoria.
- `legacy` permanece fallback; `adaptive`/`aggressive` exigem comparação controlada de seed, assets, mods, rota, unload/reload e multiplayer.

Planos vivos: [regional-runtime-execution-checklist.md](regional-runtime-execution-checklist.md) e [sector-scheduler-progress-plan.md](sector-scheduler-progress-plan.md).
