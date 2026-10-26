# Diagnóstico de dependências e custo espacial

Implementado como coleta opcional no `ExplorationProfile` do servidor. Esta
etapa prepara o plano regional; não executa objetos ou regiões em paralelo.

## Ativação

Mesclar no objeto existente em `starbound.config`, com o jogo fechado:

```json
"explorationProfiling": {
  "enabled": true,
  "regionalDetails": true,
  "intervalSeconds": 5
}
```

Reiniciar o processo. `regionalDetails` é falso por padrão e depende de
`enabled`. Ocultar o HUD não desliga este diagnóstico de log. Para desligar,
alterar `regionalDetails` para falso e reiniciar; desligar `enabled` também
remove o coletor. Nenhum payload de inventário, mensagem ou nome de função
customizada é gravado por esta extensão.

## Campos do log

`regionalDiagnostics.objectDependencies`: chamadas observadas durante a
atualização de um objeto, agrupadas por `rootObjectId`, `targetId` e categoria
`operation`. Registra contagem, tempo inclusivo e última região do objeto raiz.
Cobre as APIs de containers já instrumentadas, `callScriptedEntity` e mensagens
com ID numérico. IDs pertencem à instância/contexto do relatório, não são IDs
persistentes para comparar sessões. O registro ocorre mesmo se a operação
falhar ou tiver permissão negada: representa uma tentativa, não uma mutação.

Chamadas encadeadas são atribuídas à atualização do objeto raiz, não
necessariamente ao chamador imediato. Seus tempos podem se sobrepor; não
somar tempos das arestas para calcular CPU ou tempo exclusivo. Mensagens
medem a chamada de envio, não o processamento futuro nem a resposta.
`regional.objectCallsWithoutEntityTarget` conta chamadas sem alvo numérico,
incluindo `loadRegion` e mensagens por identificador único. Não há resolução
extra de alvos para fins de diagnóstico.

`regionalDiagnostics.entityRegions`: tempo total e Lua das atualizações,
por tipo de entidade e região lógica de 64×64 tiles. Coordenadas usam piso da
posição ao terminar a atualização, incluindo coordenadas negativas. Lua é
um subtotal do tempo total. Estes bins não alteram setores físicos, geração,
ativação de entidades ou saves. Não representam todo o custo regional:
geração, persistência, líquidos, fios e renderização não estão incluídos.

## Limites e interpretação

Por janela são retidas até 1.024 combinações de dependência e 256 combinações
de região/tipo. Entradas já admitidas continuam acumulando; novas combinações
excedentes incrementam `dependencyCallsDropped` e `entityUpdatesDropped`.
A seleção é pela ordem de chegada, não por amostragem estatística nem top-N.
Todos os mapas são limpos após cada relatório. Não há retenção de entidades,
novas threads ou locks: a coleta usa a serialização existente do mundo.

Com a opção desligada não há consultas de posição nem construção desses mapas
por chamada; permanece uma verificação condicional no diagnóstico existente.
Ligada, a coleta acrescenta consultas aos mapas, alocações limitadas e volume
de log. Seu overhead ainda precisa ser medido; não é custo zero.

Dois objetos com o mesmo alvo são candidatos a conflito. Ausência de aresta
não prova independência: ainda faltam rastreamento completo de fios/energia,
acessos a tiles, serviços globais, chamadas fora de atualizações de objetos,
identidade persistente, topologia e recursos por slot. Os dados permitem
orientar a auditoria, mas não autorizam paralelização automática.

## Validação pendente

Esta extensão recebeu revisão do diff, sem compilação nem execução no jogo.
Coletar uma base FU com ITDs separados, depois compartilhando um baú; comparar
alvos comuns, descartes e regiões. Comparar overhead com a opção desligada.
Métricas de barreiras, commits e transferências regionais dependem da futura
implementação do runtime e continuam pendentes.
