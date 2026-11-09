# Baseline, evidência e limites atuais

[Índice](README.md) · [Validação](validation-and-telemetry.md) · [Roadmap](roadmap.md)

## Propósito

Fixar o ponto de partida para impedir que uma entrega documental ou uma métrica
isolada seja interpretada como ganho já implementado. Referência de runtime:
`7ca7ed2a`. Evidência primária: runs 19–29, sobretudo 28 e 29, vinculadas na
[auditoria](../sector-generation-architecture-audit-2026-11-08.md).

## Estado observado no código

| Componente | Estado e fronteira |
| --- | --- |
| WorldThread | Única dona do mundo vivo no fluxo atual |
| BaseTiles | Worker privado, revisões/ticket e commit central implementados |
| Seleção de MicroDungeons | Worker opt-in com saída de descritores; ainda copia halo não usado pela seleção |
| Aplicação de MicroDungeons | Factory/writer vivos na dona, com efeitos e RNG compartilhado |
| CaveLiquid | Flood-fill e escrita seriais; enum de plano não implementa kernel privado |
| Finalize | Normalização, grama, flora, objetos/tesouros e ativação de líquidos misturados |
| Fila pronta | Índices/polling para resultados de BaseTiles e planos; não é mailbox executiva completa |
| Grafo | Modelo de dependências e observação; executor persistente ainda pendente |
| IO salvo | Leitura B-tree na dona; decode de tiles opt-in apenas em caminhos Tiles-only |
| Threads regionais | Lifecycle/mailbox em sombra; nenhuma simulação regional autoritativa |
| GPU | Renderer gráfico existente; backend compute de geração ainda ausente |
| Orçamento | Modos legacy/adaptive/aggressive/dynamic; pool inicial automática, sem governador global completo |

`Complete=Finalize=4`; `Terraform=5` é um caminho especial. Carga e geração
são eixos distintos. O setor inicial só é transmitido quando estiver ativo,
com carga e geração exigidas pelo protocolo.

## Evidência de desempenho

Run 29: sessão `762660399.0736`, 52 janelas, cerca de 103,042 s, encerrada
normalmente. Build `d8f26df1`, portanto anterior ao preview de avanço
`7ca7ed2a`. Os dados não validam esse preview.

| Observação | Valor | Consequência para o projeto |
| --- | ---: | --- |
| Orçamento médio por passagem com fila | 27,982 ms | existe folga concedida |
| Tempo médio da passagem | 1,935 ms | investigar encerramento e prontidão |
| Esgotamentos de orçamento | 0 | teto maior não é a primeira solução nesta rota |
| BaseTiles: cálculo médio / pronto→commit médio | 2,458 / 66,660 ms | reduzir rodadas de espera |
| Planos agendados / aplicados | 748 / 739 | preparação existe mesmo sem factory |
| Snapshots acumulados dos planos | 2.426.535.936 bytes | reduzir cópias; não confundir com RAM residente |
| Factories executadas | 0 | TMX não explica a espera desta amostra |
| Pedidos completos / expirados | 730 / 134 | separar pedidos satisfeitos de geração inédita |
| Espera máxima retida de pedido concluído | 9,906 s | medir a cauda de entrega |

Run 28 registrou um caso distinto: factory de 192,313 ms, 129,831 ms
inclusivos nas partes TMX e 56,898 ms de build. Ele justifica a frente de
stutter de estruturas; não substitui a análise de throughput dos setores comuns.

## Pendências estabelecidas pela inspeção

- Na base auditada, a continuação encerrava o loop mesmo quando o preview de
  avanço tentava prosseguir. `2131ead3` corrige esse recorte em modo opt-in;
  ver [testes e limites](../sector-runtime-h0-streaming-pass-2026-11-09.md).
  A run 29 continua anterior à correção e não mede seu ganho.
- Na base auditada, índices de colocações pendentes podiam sobreviver à
  compactação da lista. O primeiro recorte de H0 substitui-os por IDs estáveis;
  ver [validação e pendências](../sector-runtime-h0-placement-identities-2026-11-08.md).
  Isso não atribui à falha um crash não reproduzido nem prova ganho de throughput.
- `planMicroDungeons` não lê o halo de tiles que seu snapshot copia/versiona.
- Os benchmarks de `generateRegion` são síncronos e não reproduzem a fila
  de exploração. Falta baseline de streaming automatizado.
- Há diferenças históricas de terminologia setor/região a resolver na
  documentação, mantendo a decisão 2×2 e o estado real do preview.

## O que a evidência não demonstra

Baixa atividade amostrada na pool não mede toda a CPU do computador. Tempos
inclusivos não podem ser somados como tempo exclusivo. Conclusões de pedidos
incluem setores que já estavam prontos. Uma rota diferente não prova ganho
percentual de um commit. A redução de custo numa factory não prova entrega
mais rápida ao cliente.

Há também fallback recuperável de `storage`/`BadVariantType` no perfil
transitório `Nowhere`. Sua causa raiz segue pendente; registrar quando uma
janela perde detalhe, para não comparar telemetria incompleta como equivalente.

## Trabalho e aceitação

VAL-01/02 em [validação](validation-and-telemetry.md) devem estabelecer uma
rota e um comparador de outputs. Toda nova evidência deve informar build,
config, cenário, duração, hashes e limitações. Os manifestos originais ficam
nos relatórios; esta coleção referencia esses registros e não duplica logs.

Melhoria esperada: decisões baseadas na rota crítica real e ciclos de validação
mais rápidos. Aceitação: outro agente consegue reproduzir a configuração,
distinguir trabalho novo de revisita e localizar a última entrega validada.
