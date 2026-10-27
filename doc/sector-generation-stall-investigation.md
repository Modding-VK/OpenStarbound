# Investigação: revisões de setor e descarte de tiles básicos

Estado: análise de código e do log de 26/10/2026. A instrumentação de causas
de revisão foi acrescentada na branch de trabalho; nenhuma correção de geração
foi aplicada nesta etapa. Referência quantitativa em
[performance-baseline.md](performance-baseline.md).

## Evidência do log

A build `6bc48199d070e1a6c30be8172fde747fffe3eb15` visitou o planeta já
observado nos testes do Mantizi Estate e do vilarejo Glitch. Houve um setor
concluído após 40,6 s e outro ainda pendente após 60,0 s. O servidor agendou
3.570 tarefas de tiles básicos: 1.295 resultados foram recolhidos, dos quais
1.115 foram rejeitados por revisão alterada e 180 aplicados. Outros 2.272
resultados **já prontos** foram descartados ao abrir vagas no limite de
pendências; o limite foi atingido 629 vezes. Os workers configurados eram
dois, com limite de quatro tarefas pendentes.

Dos resultados recolhidos, 86,1% estavam obsoletos. Em relação a todas as
tarefas agendadas, apenas 5,0% produziram um resultado aplicado nessa sessão.
Essas proporções mostram retrabalho, mas não identificam a rotina que alterou
cada setor nem demonstram tarefas simultâneas para a mesma chave de setor.
As 89 amostras do pool mostraram dois workers configurados, zero tarefas
ativas e zero tarefas na fila interna. Amostras a cada segundo não excluem
atividade curta entre leituras; o atraso máximo entre enfileirar e começar
um trabalho **recolhido** foi 1,3 ms.

## Como uma revisão invalida o worker

1. `WorldGenerator::prepareTilesAsync` copia os tiles e guarda a revisão do
   setor antes de agendar o cálculo isolado.
2. Ao recolher o resultado, exige a mesma identidade do array de tiles,
   revisão do setor, template do mundo e revisão do template. O log registrou
   1.115 falhas da verificação de **revisão do setor** e nenhuma falha das
   outras três verificações.
3. Toda chamada bem-sucedida a `TileSectorArray::modifyTile` aumenta a
   revisão **antes** de devolver o ponteiro mutável, mesmo que o chamador só
   leia o tile ou altere um cache derivado. `sectorArray` mutável e chamadas
   explícitas a `markSectorDirty` também aumentam a revisão.

Há um caminho concreto de aumento sem alteração persistente do terreno:
`WorldServer::forEachCollisionBlock` chama `freshenCollision`, cuja primeira
passagem usa `modifyTile` para **ler** `collisionCacheDirty` em todos os tiles
consultados. Isso aumenta a revisão de cada setor carregado alcançado pela
consulta. Movimento, pathfinding e consultas Lua de colisão podem chegar a
esse caminho. Quando há um cache realmente sujo, as passagens seguintes também
usam `modifyTile` para atualizar `collisionCache` e `collisionCacheDirty`.
Esses dois campos não são gravados por `ServerTile::write`; o cache é derivado.
`dirtyCollision` usa o mesmo acesso mutável numa região ampliada em dois tiles
ao redor de alterações e pode alcançar setores vizinhos.

Logo, **é possível pelo código** invalidar um worker de tiles básicos com
consulta/reconstrução de cache de colisão, sem uma edição persistente do tile.
O log não mede a origem individual das 1.115 mudanças; modificações reais de
terreno, líquidos, estruturas e entidades continuam sendo causas possíveis e
devem preservar a invalidação por segurança.

## Por que resultados prontos são descartados

`prepareTilesAsync` limita `m_pendingBaseTiles` a duas entradas por worker.
Quando o limite está cheio, escolhe a tarefa **concluída** cujo
`lastRequestedTime` é mais antigo e apaga seu resultado, sem consultar se o
setor ainda está na fila visível. O setor volta a precisar de agendamento se
for solicitado depois. A fila ordenada e as dependências vizinhas podem
alternar os setores consultados, tornando um resultado pronto elegível para
descarte antes de chegar ao seu commit. O log conta 2.272 descartes por esse
caminho, mas não contém suas coordenadas nem informa quantos ainda eram
necessários para o jogador.

A entrada de uma tarefa ainda em execução é preservada enquanto permanece em
`m_pendingBaseTiles`; nesse caso, nova consulta ao mesmo setor espera o mesmo
resultado. `sectorLoadLevelChanged(None)` remove a entrada ao descarregar o
setor, inclusive se o worker ainda não terminou. Assim, uma nova carga pode
agendar outro trabalho para a mesma chave antes do primeiro terminar. O log
não contém IDs de tarefa para comprovar que esse caso ocorreu na sessão.

## Próxima medição e ordem segura de correção

Antes de atribuir percentuais a uma rotina, registrar por setor e de forma
limitada: origem das mudanças de revisão (consulta de colisão, reconstrução de
cache, geração/estrutura, líquido, ação ou script), revisão inicial/final,
coordenadas das invalidações e coordenadas/motivo dos descartes prontos. Essa
coleta deve ficar desativada fora da build diagnóstica ou do perfil de
exploração, sem nomes ou argumentos dinâmicos de mods no log.

A instrumentação agora mantém contadores por causa somente nos setores com
resultado de tiles básicos pendente. O relatório separa causas em resultados
invalidados e descartados, e conserva coordenadas limitadas por janela; ver
[campos do perfil](exploration-profiling.md). Falta uma execução da nova build
com o mesmo snapshot para obter os percentuais por causa. A categoria `other`
permanece necessária para alterações fora dos escopos conhecidos.

Com a origem confirmada, separar revisão de conteúdo persistente das mudanças
de cache derivado, **sem** permitir que a aplicação do snapshot sobrescreva
um cache válido ou uma alteração real feita no mundo vivo. Rever a política
de descarte para proteger resultados prontos ainda solicitados, mantendo
limite explícito de memória e tratamento de setores descarregados. A garantia
de um worker ativo por mundo+setor exige uma entrada de tarefa que sobreviva
ao descarregamento até o worker terminar; pedidos novos aguardam o término
ou marcam o resultado obsoleto.

Comparar a mesma cópia de jogador/universo e rota antes/depois. Verificar
tiles, colisão, dungeons, líquidos, objetos, iluminação, mods, multiplayer,
salvamento e recarregamento antes de aceitar qualquer correção.

## Correção inicial após a coleta da build #27

A coleta `defb650f` confirmou concentração de incrementos em consultas/cache
de colisão nos resultados rejeitados do primeiro planeta. Os dados por mundo
estão na seção 8.1 do [plano regional](world-region-rework-advanced-concept.md).
O diagnóstico histórico acima foi seguido por este recorte de correção:

- A consulta de `collisionCacheDirty` usa acesso constante. Alterações do cache
  usam `modifyCollisionCache`, cujo callback só recebe o flag e a lista de
  colisões, sem acesso mutável aos demais campos do tile. Elas não incrementam
  a revisão usada pelo snapshot e pela persistência. Escritas de conteúdo
  continuam passando pelos acessos que incrementam a revisão.
- Continuam obrigatórias a identidade do array, revisão de conteúdo, identidade
  e revisão do template. Não foi removida a validação contra edições reais.
  A cópia usada pelo worker já marca o cache como sujo; ao instalar seu resultado,
  também se invalidam os caches derivados do halo de influência da colisão.
- Antes de admitir mais trabalhos, `WorldStorage::generateQueue` aplica
  resultados BaseTiles prontos de setores já carregados, respeitando o prazo
  e o limite de etapas existentes. O caminho atualiza o nível persistido para
  BaseTiles sem carregar entidades, executar microdungeons ou declarar Complete.
  Também atende pré-requisitos que não aparecem diretamente na fila visível.
- Ao atingir o limite de pendências, a admissão espera em vez de apagar um
  resultado pronto. O limite continua sendo duas entradas por worker, no máximo
  16 arrays de setor por mundo (cada array tem tamanho fixo de 32×32 tiles).
  Isso limita os buffers de tiles; não é um orçamento global de toda memória
  de templates, mundos, filas ou entidades. Esse orçamento permanece no plano.
- Unload invalida a identidade de origem, mas preserva a chave até terminar o
  producer. Reload não inicia outro worker para o mesmo setor enquanto ele
  estiver executando. Resultados concluídos de setores descarregados podem ser
  liberados; seus efeitos nunca são instalados numa nova encarnação do setor.

A política conserva a ordem existente das etapas posteriores. Não há novo
runtime regional, paralelização de Lua nem promessa de eliminar todo atraso:
conflitos reais, microdungeons, persistência e callbacks longos ainda podem
bloquear progresso. O orçamento zero também impede instalação de resultados.

### Métricas para a próxima comparação

- `generation.baseTiles.readyResultsCommitted`: resultados drenados antes da
  admissão normal, subconjunto de `resultsApplied` (não somar os dois).
- `generation.baseTiles.admissionDeferred`: solicitações adiadas por limite;
  não é contagem de setores únicos nem falha de geração.
- `generation.baseTiles.pendingInvalidatedOnUnload` e
  `unloadedResultsDiscarded`: invalidação e descarte após término, separados
  dos antigos `pendingRemovedOnUnload` e `readyResultsEvicted`.
- Comparar `staleRevision`, `resultsApplied`, idade/espera dos setores e picos
  de tick no mesmo universo restaurado. Ausência de `readyResultsEvicted`
  agora decorre da remoção desse caminho, não prova por si só progresso.

Revisão: unidades C++ `StarWorldServer`, `StarWorldStorage` e `StarWorldGeneration`
compiladas como objetos no Linux. Compilação completa Windows e observação em
jogo ainda pendentes; não há medição de ganho após esta alteração. Conferir
colisão/iluminação nas bordas, mineração/líquidos durante geração, estruturas,
unload/reload, save e multiplayer antes de promover a correção.

## Espera restante após a correção: mundo `…:3`

Na coleta de `c0da7a4b`, o mundo
`383738425:-48324202:-255713414:3` aplicou 391 de 392 resultados BaseTiles
recolhidos; apenas um resultado ficou obsoleto, associado a dois incrementos
de revisão classificados como armazenamento. Nenhum resultado pronto foi
expulso. A retenção e a separação do cache de colisão cumpriram seu objetivo
nesta sessão, embora a equivalência de rota ainda não esteja provada.

Ainda houve espera concluída de 17,45 s. Em 641 amostras, a fila acumulou
27.399 setores (média aproximada de 42,7); 213 passagens esgotaram o orçamento,
959 bloqueios vieram de vizinhos e 386 do limite de níveis. Microdungeons
acumularam 2,81 s, `Finalize` 1,23 s e a montagem de dungeons 3,39 s. Esses
valores explicam trabalho serial e pressão de fila, mas os contadores agregados
anteriores não dizem quantas passagens terminaram sem qualquer avanço.

Foram adicionadas métricas limitadas por janela:

- `generation.visiblePass`: duração da parte visível da passagem;
- `visiblePasses`, `visiblePassQueueDepthEndTotal` e
  `visiblePassSectorsCompleted`;
- `visiblePassReadyCommitted` e `visiblePassLevelsAdvanced`;
- `visiblePassesWithoutProgress` e `visiblePassesWithPartialProgress`.

Os contadores de níveis incluem BaseTiles drenados e níveis avançados pelo
caminho normal; não representam setores únicos. Uma passagem parcial avança
algum nível sem concluir setor visível. Essa instrumentação não altera o
orçamento configurado nem a ordem da geração. A próxima decisão será baseada
na proporção de passagens sem progresso e no tempo p95/p99, evitando aumentar
o orçamento antes de saber se ele produziria progresso ou apenas ticks maiores.
