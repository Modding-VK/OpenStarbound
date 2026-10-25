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
