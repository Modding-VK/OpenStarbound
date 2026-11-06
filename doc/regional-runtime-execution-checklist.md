# Checklist operacional do runtime regional

**Branch:** `Experimental`  
**Estado:** fundações em andamento; nenhuma escrita paralela no mundo está autorizada ainda.

Este documento consolida as entregas distribuídas entre os planos de geração,
regiões, microdungeons, persistência, objetos e diagnóstico.

## Regra de publicação

Workers recebem snapshots imutáveis e retornam planos ou batches. Somente a
thread dona da região pode validar revisões, criar entidades, alterar tiles,
confirmar promises ou publicar efeitos de rede e persistência.

## Estado atual

| Componente | Estado |
| --- | --- |
| BaseTiles em pool | Implementado, com identidade e validação de revisão |
| Regiões lógicas em sombra | Implementado para observação e métricas |
| Planejador de microdungeon por snapshot | Extraído, ainda síncrono |
| `MicroDungeonPlan` task | Tipo e pendência criados; produtor não conectado |
| `WorldMutationBatch` | Contrato e validação pura criados; sem operações vivas |
| Escrita paralela | Não implementada |

## Microdungeons

- [ ] Conectar `PendingMicroDungeonPlan` ao pool de workers.
- [ ] Capturar template, setor, halo e revisões no snapshot.
- [ ] Revalidar identidade, template e revisões no retorno.
- [ ] Registrar agendamento, fila, execução, descarte, retry e bytes retidos.
- [ ] Criar operações reais de tile em `WorldMutationBatch`.
- [ ] Aplicar batches somente pela região dona, inicialmente em preview.
- [ ] Calcular footprint completo de microdungeons compostas.
- [ ] Reservar regiões do footprint e ordenar conflitos deterministicamente.
- [ ] Separar entidades, fios, líquidos e promises do batch de tiles.
- [ ] Converter `DungeonGeneratorWorld` em fachada gravadora de comandos.

**Saída:** mesma seed, assets e mods geram resultados equivalentes com um ou
vários workers; resultados obsoletos nunca publicam.

## Líquidos e finalização

- [ ] Definir `CaveLiquidPlan` sobre snapshot com halo.
- [ ] Validar revisões e fronteiras antes de publicar líquidos.
- [ ] Criar protocolo de borda para líquidos e fios entre regiões.
- [ ] Separar preparação de `Finalize` da criação de objetos, plantas e colisão.
- [ ] Publicar entidades e colisão pela região dona.

## Agendamento e memória

- [ ] Construir grafo executivo de BaseTiles, MicroDungeons, CaveLiquid e Finalize.
- [ ] Propagar prioridade visível aos pré requisitos.
- [ ] Orçar bytes, tarefas pendentes e regiões globalmente.
- [ ] Separar admissão, execução e commit em filas distintas.
- [ ] Reter resultados úteis sob pressão de memória sem livelock.
- [ ] Agregar dependências com cardinalidade limitada e contadores de perda.

## Runtime regional, persistência e compatibilidade

- [ ] Implementar mailboxes autoritativas entre regiões e handoff de entidades.
- [ ] Introduzir update local por região com barreira inicial medida.
- [ ] Preparar leitura de setores salvos em workers e instalação validada.
- [ ] Preservar checkpoint, rede, saves e reload sem duplicação.
- [ ] Definir API opt-in e fallback legado para mods, objetos e containers.
- [ ] Auditar monstros, veículos e stagehands antes de migrá-los.

## Validação obrigatória

- [ ] Repetir Mantizi, Glitch, mundo salvo e mundo inédito com a mesma seed.
- [ ] Comparar 1/N workers, pouca memória, prefetch, unload/reload e dois jogadores.
- [ ] Comparar tiles, dungeons, entidades, líquidos, colisão, iluminação e saves.
- [ ] Medir p50/p95/p99, bytes, retries, descartes e trabalho útil por etapa.
- [ ] Correlacionar Tracy e JSONL; medir profiling ligado e desligado.
- [ ] Corrigir `storage` quando o próximo log expuser `detail=`.
