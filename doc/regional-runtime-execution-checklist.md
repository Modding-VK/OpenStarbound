# Checklist operacional do runtime regional

**Branch:** `Experimental`  
**Estado:** fundações em andamento; nenhuma escrita paralela no mundo está autorizada ainda.

Este documento consolida as entregas distribuídas entre os planos de geração,
regiões, microdungeons, persistência, objetos e diagnóstico.

## Regra de publicação

Workers recebem snapshots imutáveis e retornam planos ou batches. Somente a
thread dona da região pode validar revisões, criar entidades, alterar tiles,
confirmar promises ou publicar efeitos de rede e persistência.

## Compatibilidade com mods — requisito de bloqueio

Compatibilidade não é uma otimização posterior. Uma etapa não pode ser ligada
por padrão nem declarada equivalente enquanto a comparação com os mods e
assets da rota de referência não tiver passado. Em particular:

- workers não chamam Lua, callbacks, APIs de mods, promises, bancos de
  entidades ou fachadas do mundo vivo;
- a ordem observável de geração, criação de entidades, promises, rede e saves
  permanece na thread dona até existir um contrato explícito para cada efeito;
- qualquer API nova é opt-in, preserva fallback legado e registra somente
  métricas de cardinalidade fixa, nunca nomes ou payloads de mods;
- testes de equivalência sempre incluem mundo salvo e novo, reload,
  multiplayer e um conjunto de mods representativo.

## Estado atual

| Componente | Estado |
| --- | --- |
| BaseTiles em pool | Implementado, com identidade e validação de revisão |
| Admissão de tarefas | Estágios declarados validados; chaves por `(setor, etapa)` podem coexistir |
| Regiões lógicas em sombra | Implementado para observação e métricas |
| Planejador de microdungeon por snapshot | Extraído, ainda síncrono |
| `MicroDungeonPlan` task | Planejador em worker opt-in; publicação ainda serial |
| `WorldMutationBatch` | Contrato e validação pura criados; sem operações vivas |
| Escrita paralela | Não implementada |

## Escopo de plataformas nesta fase

A estabilização e a validação corrente cobrem Windows, Linux GCC/ARM e macOS.
Linux Clang fica explicitamente adiado até o encerramento do projeto: seu preset
de teste e o job correspondente não são critério de bloqueio nesta fase e não
devem receber trabalho de compatibilidade isolado. Essa suspensão não reduz a
exigência de compatibilidade funcional ou de mods nas plataformas que seguem
no escopo.

## Microdungeons

- [x] Conectar `PendingMicroDungeonPlan` ao pool de workers, desligado por padrão.
- [x] Capturar template, setor, halo, cópias de tiles e revisões no snapshot.
- [x] Revalidar identidade, template e revisões no retorno.
- [x] Registrar agendamento, fila, execução, descarte, retry e bytes retidos.
- [ ] Criar operações reais de tile em `WorldMutationBatch`.
- [ ] Aplicar batches somente pela região dona, inicialmente em preview.
- [ ] Calcular footprint completo de microdungeons compostas.
- [ ] Reservar regiões do footprint e ordenar conflitos deterministicamente.
- [ ] Separar entidades, fios, líquidos e promises do batch de tiles.
- [ ] Converter `DungeonGeneratorWorld` em fachada gravadora de comandos.

**Saída:** mesma seed, assets e mods geram resultados equivalentes com um ou
vários workers; resultados obsoletos nunca publicam.

### Recorte vertical seguinte — somente planejamento assíncrono

Este é o próximo item implementável, mas permanece **desligado por padrão**
até passar a validação comparativa. O worker receberá um snapshot que possui
os próprios dados; `WorldTemplatePtr`, `WorldServer`, `WorldStorage`, fachada
de dungeon, entidades e promises não podem ser lidos pelo produtor.

1. A thread dona captura `templateStore`, revisão/identidade do template, o
   setor solicitado, o halo de 3×3 setores e a revisão de cada setor do halo.
   As colocações pendentes são copiadas com seu índice estável.
2. O worker reconstrói seu `WorldTemplate` privado a partir do store e retorna
   somente descritores ordenados. Ele não instancia `DungeonGeneratorWorld` e
   não produz `WorldMutationBatch` neste recorte.
3. No retorno, a região dona exige tarefa válida, mesma encarnação do mundo,
   mesma identidade/revisão do template e todas as revisões do halo. Falha,
   unload ou divergência descartam o resultado e aposentam a reserva.
4. Apenas depois dessa revalidação, a thread dona chama o caminho legado de
   aplicação, na mesma ordem de hoje. Promises e entidades continuam nesse
   caminho autoritativo.

Critérios de aceitação: sequência idêntica de planos para a mesma seed, assets
e mods com 0, 1 e N workers; nenhum acesso do worker a tile vivo; e contagem
equilibrada de `reserved`, `committed`, `stale`, `cancelled` e `failed` após
unload/reload. A captura precisa informar bytes do halo, profundidade da fila e
tempo de fila/execução, sem nomes de dungeons ou payloads de mods.

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

## Ordem de execução atual

1. **Estabilização:** manter os contratos puros, a checagem de etapas e a
   integração com `main`; não ampliar o número de escritores do mundo.
2. **BaseTiles:** repetir o teste com mundos reais e assets licenciados antes
   de mudar limite, política de descarte ou validação de revisão.
3. **Microdungeons:** executar somente o recorte vertical de planejamento
   descrito acima; batches de mutação e a fachada gravadora vêm depois da
   equivalência do planejador.
4. **Agendador:** converter a observação atual em grafo executivo somente
   depois de existir uma fronteira completa e versionada; prioridade nunca
   cria interesse nem autoriza publicação.
5. **Persistência e runtime regional:** introduzir leitura privada e instalação
   validada antes de mailboxes, handoff ou atualização concorrente de entidades.

A externalização de NPCs permanece suspensa e não é pré-requisito de nenhuma
etapa acima.

## Validação obrigatória

- [ ] Repetir Mantizi, Glitch, mundo salvo e mundo inédito com a mesma seed.
- [ ] Comparar 1/N workers, pouca memória, prefetch, unload/reload e dois jogadores.
- [ ] Comparar tiles, dungeons, entidades, líquidos, colisão, iluminação e saves.
- [ ] Medir p50/p95/p99, bytes, retries, descartes e trabalho útil por etapa.
- [ ] Correlacionar Tracy e JSONL; medir profiling ligado e desligado.
- [ ] Confirmar em Windows a escrita da categoria `storage` após a proteção
  contra seções opcionais ausentes no relatório (`BadVariantType` observado em
  2026-11-06).
