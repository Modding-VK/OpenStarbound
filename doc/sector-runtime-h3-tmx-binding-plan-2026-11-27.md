# H3 / MD-03: plano de ligação TMX em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [TMX static](sector-runtime-h3-tmx-static-product-2026-11-27.md).

## Objetivo

Primeiro recorte de MD-03: registrar escolhas RandomBrush/NpcBrush/conector
na ordem autoritativa sobre um produto estático, sem consumir RNG vivo nem
escrever na facade.

## Escopo

`WorldGenerationTmxBindingPlan` + `planWorldGenerationTmxBindingChoices`.
Requests trazem optionCount/selectedIndex já resolvidos pelo dono.

## Validação

NoAssets: Binding **5/5** (+ Static **6/6**).

## Limitações

Sem stream Random real, sem spawn de NPC/objeto, sem MD-04 comandos.
Publicação autoritativa continua fora (sem live-publish em H0/H1 apis_ready).

## Próximo

MD-04 footprint/comandos em sombra, ou WT-04 staging invisível sem install.
