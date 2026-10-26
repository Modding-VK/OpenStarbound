# Auditoria das execuções recentes do GitHub Actions — 26/10/2026

Escopo: execuções manuais do workflow [Build](../.github/workflows/build.yml)
no repositório `Modding-VK/OpenStarbound`, branch `perf/threading-audit`, commit
`6bc48199`. Esta auditoria consultou os estados, jobs, artefatos e o log de
configuração disponibilizados pelo GitHub. Não iniciou novas execuções.

| Execução | Resultado observado | Interpretação |
| --- | --- | --- |
| [#23](https://github.com/Modding-VK/OpenStarbound/actions/runs/40247252659) | `skipped`; os oito jobs foram ignorados | Nenhuma condição de seleção de plataforma passou. A API consultada devolveu `inputs: null`, então não é possível afirmar quais opções foram enviadas ou por que ficaram falsas. |
| [#24, Tracy](https://github.com/Modding-VK/OpenStarbound/actions/runs/40247563736) | Job Windows falhou em `Run CMake with Tracy`, antes de compilar | O código de integração mistura as assinaturas `plain` e `keyword` de `target_link_libraries` para os mesmos executáveis. |
| [#25, Windows normal](https://github.com/Modding-VK/OpenStarbound/actions/runs/40249959323) | Build, criação do instalador e upload concluídos; publicação da Release ignorada | O instalador existe como artifact. O job de publicação requer a opção explícita `publish_windows_release: true`, cujo padrão é `false`. |

## Tracy: falha de configuração

O log do job Windows da #24 aponta `source/CMakeLists.txt:650` e `:681`.
Essas chamadas usam `target_link_libraries(... PRIVATE Tracy::TracyClient)`
para `starbound_server` e `starbound`. Os mesmos alvos já recebem a chamada
sem `PRIVATE` em `source/server/CMakeLists.txt:43` e
`source/client/CMakeLists.txt:49`. O CMake proíbe misturar as duas assinaturas
para um mesmo alvo. A configuração chegou a obter o Tracy oficial fixado pela
revisão e a definir `TRACY_ENABLE=ON`; **não chegou à compilação** nem produziu
artifacts. Portanto essa execução não informa se instrumentação, link,
instalador ou execução em jogo funcionariam depois de corrigida a configuração.

Correção necessária antes de outro diagnóstico: uniformizar a assinatura de
`target_link_libraries` dos dois alvos; manter o link de Tracy restrito ao
preset diagnóstico. Depois, uma execução manual de Windows com `tracy: true`
deverá comprovar configuração, compilação e instalador diagnóstico; conexão e
captura Tracy exigem execução local do jogo instrumentado.
O workflow atual deliberadamente não publica Release para Tracy.

## Instalador e Release da build normal

A #25 publicou quatro artifacts ainda disponíveis no momento da consulta:

| Artifact | Tamanho informado pelo GitHub |
| --- | ---: |
| `OpenStarbound-Windows-Installer` | 94.853.928 bytes |
| `OpenStarbound-Windows-Client` | 137.566.047 bytes |
| `OpenStarbound-Windows-Server` | 88.950.860 bytes |
| `OpenStarbound-Windows-All-DevOnly` | 219.125.403 bytes |

O job `Publish Windows Installer Release` só é admitido se a build foi
despachada manualmente com `publish_windows_release: true` e `tracy: false`
([condição do workflow](../.github/workflows/build.yml)). Na #25 ele foi
`skipped`; isso explica a ausência de uma **nova GitHub Release**, embora o
instalador esteja disponível na seção **Artifacts** da execução. A #22 também
gerou instalador e ignorou esse job. Existe uma Release `windows-build-21`,
mas a execução #21 consultada não contém o job atual de publicação. Portanto
a existência dessa Release anterior não valida o caminho atual de publicação;
ela pode ter vindo de outro fluxo ou de publicação manual.

Publicar uma Release requer solicitação explícita e uso da opção dedicada.
Quando isso for solicitado, verificar também permissões efetivas de
`contents: write`, download do artifact e arquivo anexado, já que o caminho
atual de publicação ainda não tem execução bem-sucedida comprovada.

## Leitura operacional

- Para build Windows normal com instalador, selecionar `windows: true`.
- Para diagnóstico Tracy, selecionar também `tracy: true` **após** corrigir o
  CMake; o instalador diagnóstico continua previsto, separado do normal.
- O padrão `publish_windows_release: false` evita publicar uma Release por
  consequência de uma build. `Success` no build Windows não significa que o
  job de publicação foi executado.
- A #23 não fornece base para atribuir o `skipped` a uma falha de compilação
  nem para afirmar um erro específico de seleção; conferir as opções no
  formulário e o resumo do run se o caso se repetir.

Estado: diagnóstico documentado; correção CMake, novo build Tracy e prova do
job de publicação seguem pendentes. O workflow permanece manual.
