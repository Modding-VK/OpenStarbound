# Auditoria de IO de setores e fila de resultados prontos — 07/11/2026

**Branch examinada:** `Experimental`  
**Referência de código:** `e8e98838`  
**Método:** auditoria estática; não houve mudança de runtime nem medição nova.

## Objetivo

Definir o primeiro recorte seguro para reduzir trabalho serial de leitura de setores e transformar resultados de workers em fila explícita, sem acesso concorrente ao mundo vivo, Lua, entidades, persistência ou rede.

## Caminho atual

`WorldStorage::loadSectorToLevel` concentra a transição. Em `Tiles`, lê `tileSectorKey(sector)` no `m_db`, descomprime/desserializa `TileSectorStore`, instala tiles, atualiza revisões/metadados e chama callbacks. Em `Entities`, lê `entitySectorKey(sector)`, desserializa `VersionedJson`, cria e inicializa entidades e atualiza o índice de únicos. Unload e `syncSector` serializam/escrevem ambos os tipos e `sync` faz commit do B-tree.

`BTreeDatabase::find` usa `ReadLocker` e retorna `ByteArray` por valor. Isso protege a chamada com o lock interno, mas não cria encarnação/versionamento por setor para uma instalação tardia, nem elimina coordenação com `rollback`, `close`, `sync`, unload e o dispositivo. O primeiro recorte não compartilha `m_db` com workers.

`readTileSector` consulta `Root`, bancos de materiais/líquidos e `/worldstorage.config` para normalizar IDs inválidos. `readEntitySector` apenas produz JSON, mas a etapa seguinte cria/inicializa entidades e pode atingir scripts/mods. Entidades e validação por bancos globais ficam fora do primeiro worker.

## Fronteira segura — tiles salvos

A thread dona deve capturar `SavedTileSectorSnapshot` imutável com:

- encarnação do mundo, setor, sequência de solicitação e nível de carga alvo;
- cópia do `ByteArray` do B-tree, ou marcador de setor ausente/default;
- ticket de instalação único e estado/revisão observados pela dona;
- parâmetros de normalização copiados da configuração, quando necessários.

O worker recebe somente esse valor e executa descompressão e desserialização de tiles sem tocar `WorldStorage`, `m_db`, `Root`, assets, entidades ou callbacks. A primeira versão mantém normalização de IDs e instalação em `m_tileArray` na dona. Antes de instalar, a dona exige mesmo mundo/ticket, setor ainda no estado esperado e ausência de unload/instalação posterior. Resultado inválido é descartado.

Setor ausente continua criando default na dona. Isso preserva a marcação dirty que garante a história de geração após crash e evita misturar IO com geração.

## Fila explícita de resultados prontos

Hoje BaseTiles usa `m_pendingBaseTiles`: a dona faz polling de promises, `readyBaseTileSectors` devolve lista e `generateQueue` ordena/aplica na mesma passagem. Microdungeon usa mapa pendente semelhante. `WorldGenerationTasks` limita tarefas/bytes, mas não há estrutura explícita para resultado pronto aguardando commit.

O contrato proposto é uma fila proprietária `ReadyGenerationResult`:

1. A dona transfere payload concluído, ticket, bytes, `readyAt` e sequência de admissão da promise para a fila.
2. A reserva permanece em `WorldGenerationTasks` enquanto aguarda, mantendo identidade e limite de memória.
3. A dona escolhe commit por interesse atual, prioridade persistente, sequência estável e etapa; término do worker não define ordem.
4. Antes de publicar, revalida ticket, revisões, template e estado de carga. Resultado stale/cancelled é aposentado sem escrita viva.
5. Unload, rollback, troca de mundo e destruição drenam/cancelam a fila antes de liberar a reserva.

A primeira fila aceita apenas BaseTiles e resultados de leitura privada de tiles. Microdungeon continua no caminho atual até que footprint, promises e entidades possam compartilhar o mesmo contrato.

## Métricas e aceitação

Adicionar métricas fixas: `generation.readyResults.pending`, `.enqueued`, `.committed`, `.stale`, `.cancelled`, `.evicted` e `.bytes`; atrasos `worker-ready → ready-queue` e `ready-queue → commit`; tempos de leitura, descompressão, desserialização, normalização e instalação; e tickets de IO rejeitados por unload/recarga/troca de mundo.

Aceitação: mesma seed, assets, mods e save produzem tiles, nível de geração, revisões, unload/reload e persistência equivalentes com 0, 1 e N workers. A dona não pode executar Lua ou criar entidades como efeito de resultado depois descartado.

## Sequência implementável

1. Extrair desserialização de tile pura, sem `Root`, e criar snapshot/ticket unit-testável para bytes compactados e setor ausente.
2. Criar fila explícita para BaseTiles já existentes, sem IO em worker; preservar ordem de commit e medir atrasos.
3. Trocar desserialização serial de tiles por tarefa de snapshot em worker, mantendo B-tree, normalização e instalação na dona.
4. Validar Windows em mundo novo/salvo, mods, unload/reload, prefetch, cancelamento e dois jogadores.
5. Só depois auditar handle de leitura dedicado ou normalização pelos bancos de material/líquido fora da dona.

## Não autorizado

- `m_db` direto em worker.
- Criação/inicialização/serialização/destruição de entidades em worker.
- Lua, callbacks de mods, promises, índices únicos, rede ou persistência fora da dona.
- Commit na ordem de término de worker, ou paralelização de unload, `sync`, `commit`, `rollback` e `close`.

Complementa [performance-audit-2026-11-07.md](performance-audit-2026-11-07.md) e [regional-runtime-execution-checklist.md](regional-runtime-execution-checklist.md).
