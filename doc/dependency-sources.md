# Procedência de código e dependências externas

## Regra do projeto

Baixar código, bibliotecas e ferramentas de build somente dos canais oficiais
controlados pelo projeto que os mantém, ou de um registro oficial que identifique
e verifique a origem do projeto. Usar HTTPS, fixar uma versão/revisão imutável e
verificar a integridade do conteúdo antes de compilar ou executar. Um mirror,
fork, link de terceiro ou referência móvel não substitui a fonte oficial.

Para cada nova dependência ou atualização, registrar: projeto e mantenedor,
URL oficial, revisão/versão exata, hash do arquivo ou commit, licença, motivo do
uso e onde a dependência entra na build. Conferir também os downloads que a
própria dependência faz, inclusive via vcpkg ou GitHub Actions. Se a fonte,
versão ou integridade não puder ser confirmada, interromper essa integração e
escolher uma fonte oficial verificável.

Um hash comprova que os bytes recebidos correspondem aos revisados; ele não
comprova sozinho que a fonte é legítima. Por isso, procedência e integridade
devem ser verificadas separadamente. Manter bibliotecas opcionais fora da build
normal quando não são necessárias, para não aumentar downloads e superfície de
dependências em cada execução do Actions.

## Autoria e reutilização de código

Referências externas podem orientar hipóteses, interfaces e escolhas de
arquitetura. Implementar as alterações de primeira parte a partir dos
requisitos e contratos deste projeto, sem copiar ou parafrasear de perto a
implementação de outro repositório, inclusive por meio de código gerado por
agentes. Para uma referência técnica relevante, registrar projeto, URL e
revisão consultada, além das decisões que foram adotadas ou descartadas.
Semelhança de algoritmo ou uso de uma API pública não prova, por si só, cópia;
uma revisão humana deve examinar trechos com estrutura ou expressão
suspeitamente semelhantes antes do merge.

Se for necessário incorporar código de terceiros, tratá-lo como uma importação
explícita, não como código próprio: verificar titulares e licença aplicável,
compatibilidade com a distribuição pretendida, preservar avisos e atribuições,
isolar a origem e registrar exatamente os arquivos/trechos importados e suas
alterações. Revisar também os requisitos de distribuição binária. Ausência
de licença identificável não concede permissão de reutilização. Não remover
créditos de arquivos herdados.

O uso de xStarbound como referência para o Tracy limita-se à seleção de
questões a medir e pontos conceituais de instrumentação. A implementação de
zonas no OpenStarbound deve ser escrita a partir dos próprios fluxos da
engine. Tracy é uma dependência separada, obtida do mantenedor oficial e
distribuída no pacote diagnóstico com sua licença; não deve ser apresentada
como código autoral do projeto.

### Procedência herdada a esclarecer

Este repositório é um fork de OpenStarbound, e o `README.md` do upstream
declara derivação do código de Starbound. Não foi localizado um arquivo de
licença geral na raiz deste checkout em 25/10/2026; `doc/OPENSOURCE.md`
contém avisos de bibliotecas de terceiros, não resolve sozinho os direitos
do código principal. A procedência e os direitos de redistribuição dessa base
herdada requerem análise própria antes de qualquer afirmação de licença
geral ou de autoria integral do fork. Não adicionar uma licença para a base
herdada sem confirmação dos direitos correspondentes.

## Aplicação ao Tracy

Usar apenas o repositório oficial `wolfpld/tracy` e uma revisão fixa. Na opção
diagnóstica do CMake, baixar um arquivo da revisão oficial com SHA-256 fixado
no projeto; a opção desligada não deve fazer esse download. Registrar a licença
BSD de três cláusulas. O visualizador deve vir dos releases oficiais do mesmo
projeto ou ser compilado da revisão oficial. Ver
[tracy-integration-plan.md](tracy-integration-plan.md).

## Pendências identificadas na configuração atual

- `source/vcpkg-configuration.json` fixa o baseline do registro oficial
  `microsoft/vcpkg`. Ao atualizar o baseline ou incluir bibliotecas, conferir
  URLs e hashes dos ports efetivamente usados; o baseline por si só não prova
  a procedência de cada arquivo baixado.
- `.github/workflows/build.yml` fixa as actions por commit, inclusive a
  revisão antes alcançada por `lukka/get-cmake@latest`. Atualizações exigem
  confirmar a origem do repositório e resolver uma nova revisão imutável.
- O workflow baixa o Inno Setup do release oficial `jrsoftware/issrc` e verifica
  o SHA-256 `5ad54ca3def786f8f4212552e54cc6d8d61329e2d24a1cfee0571d42c2684ff1`
  publicado nos metadados do release `is-7_0_2` antes de executá-lo.

Esses itens são um inventário inicial, não uma afirmação de que todas as
dependências transitivas e downloads do CI já foram auditados.
