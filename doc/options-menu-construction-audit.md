# Auditoria da construção do menu Options

**Data:** 2026-11-05  
**Referência comparada:** `upstream/main` em `73d6b5147005dcc42b85358b9efd84b90174f16c`  
**Árvore avaliada:** `Experimental` em `3f7301aa`

## Correção posterior

Após a auditoria, o patch do Options recuperou os widgets completos do
OpenStarbound para voz, atalhos, instrumento e rotação de cabeça. O botão
Diagnostics inicialmente foi colocado em `[87, 74]`, entre a linha de atalhos
`[*, 95]` e a linha de rotação de cabeça `[*, 51]`. A captura da build mostrou
que essa posição atravessava os rótulos e checkboxes de multiplayer do ativo
vanilla. O layout foi então reorganizado: a faixa superior agora tem **Voice**
à esquerda e **Tools** à direita. `showVoiceSettings` ocupa `[30, 185]` e
`showDiagnostics` ocupa `[133, 185]`, cada um com imagem de `71 × 14` pixels.
O botão desativado `TODO: Players` foi removido com `null` no patch JSON; ele
não tinha função. As colunas não se cruzam horizontalmente e recuperam a faixa
das opções de multiplayer. Cada controle
obrigatório buscado por `OptionsMenu.cpp` agora é validado antes do uso. Se um
mod remover ou alterar o tipo de um deles, a exceção informa seu nome.

As descrições abaixo registram o estado defeituoso anterior à correção, para
preservar a cadeia de evidências. A construção final ainda parte do
`optionsmenu.config` vanilla e usa apenas patches para as mudanças necessárias.
Isso mantém os campos exigidos pelo jogo e evita reconstruir manualmente o
arquivo base ausente deste repositório. A disposição final depende também dos
patches dos mods da instalação; a confirmação visual exige abrir o menu na
próxima build.

## Objetivo

Entender por que a inicialização do jogo alcançava a criação de `OptionsMenu` e
terminava em *stack overflow*, e evitar que a tela de diagnósticos torne o menu
original frágil para instalações com mods.

## Resultado resumido

O fluxo essencial de `OptionsMenu` em `Experimental` permanece igual ao do
OpenStarbound original. A adição do botão **Diagnostics** não mudou o contrato
dos controles já existentes. Há, contudo, duas propriedades relevantes da
implementação original:

1. O Options é criado durante a construção da tela inicial, antes que o jogador
   veja o menu principal.
2. Ao ser criado, ele constrói imediatamente quatro painéis que ainda não estão
   visíveis: voz, atalhos de mods, atalhos do jogo e gráficos.

Assim, uma falha em qualquer configuração de interface usada por esses painéis
impedia o jogo de chegar ao menu principal. A alteração em `3f7301aa` adia a
criação do Options até o clique em **Options**. Ela preserva a função do menu e
isola a falha da inicialização geral do jogo.

Uma revisão posterior do código encontrou uma regressão mais concreta: o patch
atual removeu também as definições de `instrumentSlider`,
`instrumentValueLabel` e `headRotationCheckbox` presentes no patch original,
enquanto `OptionsMenu.cpp` continua a buscá-las e desreferenciá-las sem
verificação. O `fetchChild` retorna ponteiro vazio se a chave faltar. Na
ausência de outro mod que forneça essas chaves, `m_instrumentSlider->setRange`
é a primeira desreferência inválida. Esse é um defeito do layout atual mesmo
que não seja possível confirmar, sem o JSON resolvido da instalação, que ele
tenha sido a primeira exceção da execução observada.

O minidump ainda não identifica com segurança a primeira falha para o *stack
overflow*. O log da build `8f3cb274` delimita o ponto: a última mensagem é
`[TitleScreen] initializing options menu`. Essa build ainda não continha as
marcas internas de `OptionsMenu` nem o carregamento sob demanda introduzidos em
`3f7301aa`.

## Como o OpenStarbound original constrói o Options

O fluxo em `source/frontend/StarTitleScreen.cpp` do upstream é:

1. `TitleScreen` constrói os painéis de menu principal, personagens,
   multijogador, Options e mods, nessa ordem.
2. `initOptionsMenu(client)` cria `OptionsMenu`, centraliza o painel, bloqueia
   sua posição e o registra no `PaneManager` como `optionsMenu`.
3. Somente depois disso a tela inicial é considerada pronta.

O construtor em `source/frontend/StarOptionsMenu.cpp` executa, na ordem:

1. Obtém `Root::assets()` e cria um `GuiReader`.
2. Registra os callbacks dos botões e sliders, usando o nome do widget como
   callback padrão.
3. Lê o ativo resolvido
   `/interface/optionsmenu/optionsmenu.config`.
4. Entrega `paneLayout` ao `GuiReader`, que cria cada widget.
5. Busca, por nome e tipo, os sliders, checkboxes e rótulos obrigatórios.
6. Configura as faixas dos sliders com `sfxDelta` e `musicDelta`.
7. Cria `VoiceSettingsMenu`, `BindingsMenu`, `KeybindingsMenu` e
   `GraphicsMenu`.
8. Copia as configurações atuais para o estado local do painel.

Os itens de 4 a 7 são todos parte da construção, mesmo que o usuário nunca abra
as telas secundárias. Essa é a maior superfície de falha do desenho original.

## Contrato do JSON de widgets

`GuiReader` usa `WidgetParser` (`source/windowing/StarWidgetParsing.cpp`). Para
cada entrada em `paneLayout`, o valor final precisa ser um objeto com:

- `type`: tipo conhecido pelo leitor, como `button`, `slider` ou `label`;
- um nome, que é preenchido a partir da chave do mapa;
- os campos exigidos pelo tipo. Um botão visível, por exemplo, exige `base`;
- um callback registrado. Se `callback` não for declarado, o nome do widget é
  usado.

O erro que apareceu anteriormente — `member configuration is either not a map,
or does not specify a widget name and type` — vem diretamente dessa validação.
Ele ocorreu porque `showVoicePlayers` foi introduzido como uma entrada parcial
com apenas `visible: false`; essa entrada não existe no layout vanilla, logo a
mesclagem não tinha um `type` prévio para preservar. A correção `78b8b365`
removeu essa entrada inválida.

O botão `showDiagnostics` atual contém `type`, imagens e legenda, e o callback
`showDiagnostics` é registrado antes de o layout ser construído. Sob esse
contrato, ele é um widget válido.

## Semântica dos patches de assets

O arquivo local é:

`assets/opensb/interface/optionsmenu/optionsmenu.config.patch`

Em `Assets::applyJsonPatches`
(`source/base/StarAssets.cpp`), um arquivo `.patch` cujo topo é um objeto usa
`jsonMergeNulling`; não é uma substituição completa nem, nesse caso, uma lista
RFC 6902. `jsonMergeNulling` desce recursivamente em objetos e remove chaves
cujo valor de patch é `null`.

Consequências verificadas:

- Uma alteração parcial, por exemplo `"musicSlider": { "position": [...] }`,
  é correta **somente** se `musicSlider` já vier do ativo base com `type` e os
  demais campos exigidos.
- Um widget adicional deve ser descrito por completo no patch. `showDiagnostics`
  segue essa regra.
- Um patch posterior de mod pode alterar ou remover campos do mesmo widget.
  O construtor recebe o JSON resolvido após todos os patches, não o conteúdo
  bruto do arquivo deste repositório.

O upstream já usa esse mecanismo para voz, atalhos e rotação de cabeça. O patch
original contém vários widgets próprios completos e ajustes parciais de widgets
vanilla. A nossa versão atual conserva apenas o widget adicional de diagnósticos.
Ao eliminar as alterações de geometria que causavam sobreposição visual, a
alteração eliminou também definições exigidas pelo código C++.

## Diferenças efetivas da nossa árvore

| Área | Upstream | Experimental | Efeito |
| --- | --- | --- | --- |
| Callback | Não existe `showDiagnostics` | Registrado antes do layout | Permite o botão novo sem alterar callbacks existentes. |
| Layout | Patch de voz/atalhos/gráficos, instrumento e rotação de cabeça | Apenas `showDiagnostics` completo | Remove controles exigidos por `OptionsMenu.cpp`. |
| Painel de diagnósticos | Não existe | Criado somente no clique | Não carrega recursos de diagnósticos na abertura do Options. |
| Criação do Options | Durante `TitleScreen` | No primeiro acesso a **Options** | O menu principal deixa de depender de toda a cadeia do Options. |
| Observabilidade | Sem marcos desse fluxo | Marcas `[TitleScreen]` e `[OptionsMenu]` | A próxima falha poderá ser localizada por etapa. |

## Evidências do caso observado

O log de 2026-11-05 para a build `8f3cb274` registra a construção normal de
sky, menu principal, seleção/criação de personagem e multijogador. A última
linha de fase é:

```text
[TitleScreen] initializing options menu
```

O processo cai antes de `initializing mods menu`. O minidump correspondente
confirma `0xc00000fd` (*stack overflow*) no processo do cliente. A correção de
tratamento de stack overflow em `8f3cb274` permitiu gerar o dump sem repetir a
rotina de erro, mas a build não trazia logs internos suficientes para separar:

- resolução e mesclagem do `optionsmenu.config`;
- construção do `paneLayout`;
- busca dos controles obrigatórios;
- construção dos quatro submenus.

A leitura dos endereços presentes na pilha do dump mostra mais de cem
ocorrências de endereços associados a `StarException::captureStack` e ao código
de exceções. Isso é compatível com repetição durante o tratamento de uma falha,
mas não identifica sozinho qual instrução provocou a primeira exceção. O
tratador Windows em `StarSignalHandler_windows.cpp` redireciona uma violação de
acesso para `sehTrampoline`, que executa captura de pilha e formatação. Assim,
um *stack overflow* no dump não demonstra necessariamente recursão no layout.

`3f7301aa` resolve essa lacuna: o menu principal deve abrir mesmo se o Options
estiver comprometido; ao abrir Options, o log passa a informar `loading layout`,
`layout constructed`, `controls resolved`, `sliders configured` e cada submenu
criado.

## Regressões e limites identificados no código

O patch original cria `instrumentSlider`, `instrumentValueLabel` e
`headRotationCheckbox`. O patch atual não os contém. O projeto não inclui o
`optionsmenu.config` vanilla, que vem do `packed.pak` do jogo, por isso a
existência desses widgets no ativo final depende também dos assets da instalação.
O desenho original do OpenStarbound os acrescenta explicitamente, evidência de
que não se deve presumir que o vanilla os forneça.

`Widget::fetchChild` devolve `{}` quando não encontra o nome. O construtor de
Options não valida o resultado antes de chamar `m_instrumentSlider->setRange`.
Mais tarde, `syncGuiToConf` também usa `m_instrumentLabel` e
`m_headRotationButton` sem verificar. Isso cria dois pontos adicionais de
falha. Os botões de voz, atalhos e gráficos também deixaram de ser definidos no
patch; seus submenus continuam a ser construídos, mas ficam inacessíveis pelo
layout normal se outro asset não fornecer os botões.

Há ainda um segundo caminho de criação do Options em
`StarMainInterface.cpp`: ele permanece imediato quando a interface de jogo é
construída. O adiamento em `TitleScreen` protege a abertura do menu principal,
mas não resolve a regressão do layout nem protege a entrada no mundo.

## Hipóteses e limites

Há configurações de interface fornecidas por mods na instalação que participou
do crash. Elas são uma variável real, pois assets podem ser mesclados por ordem
de fonte. Não há evidência suficiente para identificar um mod como causador.
Também não há evidência de que Tracy ou o painel de diagnósticos sejam a causa:
o painel de diagnósticos é criado somente depois de pressionar o botão, enquanto
o crash ocorreu antes do menu principal.

O *stack overflow* difere de um `WidgetParserException` normal. Um JSON de
widget incompleto normalmente produz a exceção legível mostrada anteriormente.
A ausência dos widgets obrigatórios tende a produzir violação de acesso; o
tratamento dessa violação pode ter obscurecido a falha inicial. A relação causal
exata entre o patch reduzido e o dump ainda precisa da configuração final dos
assets ou de uma pilha simbolizada da mesma build.

## Próximo teste e decisões

1. Restaurar no patch as definições completas dos controles obrigatórios do
   OpenStarbound, acomodando seus botões no layout com espaço para Diagnostics.
2. Validar `fetchChild` antes de qualquer desreferência para produzir erro com
   nome do controle, em vez de violação de acesso.
3. Considerar o caminho de `StarMainInterface` ao avaliar a inicialização.
4. Usar uma build posterior a essas correções. Confirmar menu principal, Options
   e entrada no mundo.
5. Abrir **Options** uma vez e enviar o `starbound.log` resultante se houver
   falha. A última marca `[OptionsMenu]` identifica a etapa responsável.
6. Se a falha ocorrer após `controls resolved`, adiar também a criação de voz,
   bindings, keybindings e gráficos até o botão correspondente. Isso mantém o
   comportamento, mas impede que um submenu não usado bloqueie o Options.
7. Se ocorrer antes de `layout constructed`, registrar de forma segura a origem
   e a forma do ativo resolvido, sem despejar configurações inteiras no log.
8. Reproduzir com o mesmo executável e sem patches de interface de terceiros
   apenas como teste de isolamento. O resultado separa um defeito do núcleo de
   uma combinação de assets, sem culpar um mod por suposição.

Nenhuma dessas decisões altera o modelo de threading, a instrumentação de mundo
ou os dados de jogo.
