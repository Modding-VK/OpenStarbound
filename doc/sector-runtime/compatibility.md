# Compatibilidade, invariantes e fronteiras de adoção

[Índice](README.md) · [Validação](validation-and-telemetry.md) · [Roadmap](roadmap.md)

## Propósito

Permitir ganhos de hardware sem alterar silenciosamente mundos, mods,
scripts, rede ou persistência. Compatibilidade é uma condição de publicação
de cada entrega. Um caminho novo pode ser rápido e continuar inelegível por
alterar resultados ou tornar estado parcial observável.

## Invariantes de bloqueio

| Domínio | Invariante |
| --- | --- |
| Estado vivo | Uma dona autoritativa por unidade/instante; worker não publica por conta própria |
| Identidade | Epoch, encarnação e sequência impedem aceitar outputs de outro ocupante |
| Tiles | Edição real após snapshot não pode ser sobrescrita por output antigo |
| RNG | Fluxos e ordem de escolhas preservados no perfil de equivalência |
| Estruturas | Mesma seleção, tentativas, conflitos e integridade de footprint |
| Promises | Cumprimento único após efeito confirmado; cancelamento/erro explícitos |
| Entidades | Uma autoridade e no máximo um update aplicável por passo |
| Lua | Callback síncrono não vira operação atrasada sem contrato explícito |
| Persistência | Save descreve estado confirmado e recuperável |
| Rede | Primeiro pacote e deltas respeitam a ordem/formato suportados |
| Hardware | CPU completa disponível; GPU diferente não muda o planeta |
| Geometria | Setor físico 32×32, wrap e bordas mantidos |

## MOD-01 — Classificar operações por efeito

Inventariar cada leitor/escritor e inicializador em terreno, assets, objetos,
Lua, líquidos, fios, índices, RNG e comandos externos. Classificar como kernel
puro, preparação que exige dependências resolvidas, callback autoritativo ou
efeito multirregião. Consulta que pode invocar script não é pura por nome.

Workers de cálculo e kernels GPU recebem valores imutáveis. Não lhes entregar
`WorldServer`, VM Lua, banco mutável ou facade viva. O sistema de assets tem
suas próprias threads e scripts de patch; reutilizar sua API auditada não
autoriza executar esses scripts num worker de geração arbitrário.

## MOD-02 — Equivalência e versionamento

Definir inputs que produzem um output: seed, template/layout, geração de
assets, distribuição, versão do algoritmo, ordem de comandos e estado vivo
relevante. Uma cache é válida apenas sob essa identidade.

Comparar tiles, líquidos, proteção, colisão, entidades, IDs, inventários,
promises e metadata. Para entidades/IDs com diferenças permitidas, documentar
normalização exata do comparador; não mascarar divergência de conteúdo como
ruído. Alteração de matemática GPU ou seed que mude mundo exige decisão de
versão de geração separada, fora do perfil equivalente.

## MOD-03 — Perfis de execução e fallback

| Perfil conceitual | Uso |
| --- | --- |
| Legado com preparação acelerada | Mods existentes mantêm efeitos síncronos; kernels privados podem ganhar velocidade após prova de equivalência |
| Regional experimental | Somente subsistemas e conteúdo com capacidades auditadas podem ter donos regionais |
| Regional ampliado | Mais APIs/subsistemas após handoff, transações, save e multiplayer aprovados |

O ganho de CPU/RAM/GPU na preparação não exige reescrever todos os mods.
Já a simulação regional concorrente de callbacks antigos não tem equivalência
automática. Conteúdo desconhecido precisa de fallback por contrato antes da
ativação do modo regional; nunca por uma lista de nomes de mods supostamente
seguros. FU e roteadores síncronos continuam no caminho compatível até auditoria
das operações que compartilham estado.

Não decidir fallback depois de publicar metade de uma operação. Retorno de
modo requer fronteira de checkpoint/reload. Fallback de um kernel numérico
antes do commit é diferente de migrar um mundo regional parcialmente ativo.

## MOD-04 — Integração de subsistemas e plataformas

Transferência de entidades, fios, líquidos, clima, quests e scripts globais
tem protocolos próprios. Handoff de NPCs existentes não retoma o plano
suspenso de externalização de IA. No modo central inicial, esses efeitos
continuam sob o contrato atual.

Manter builds normais e diagnósticas distinguíveis. GPU compute é opcional;
driver/contexto ausente preserva CPU. Windows, Linux GCC/ARM e macOS seguem
no escopo; Linux Clang permanece adiado. Uma dependência futura exige origem,
versão, licença e integridade conforme a
[política do projeto](../dependency-sources.md).

## Melhoria esperada e aceitação

O objetivo é permitir adoção incremental com regressões localizáveis e caminho
compatível preservado. Uma entrega termina apenas quando seu teste de efeitos,
erro e recuperação estiver documentado, além do benchmark de velocidade.

Revisão independente é necessária para mudanças de alto impacto antes de
integração a `main`. O resultado deve informar limites conhecidos: aprovação
de um kernel não aprova todo o runtime regional. Ver a matriz de
[validação](validation-and-telemetry.md).
