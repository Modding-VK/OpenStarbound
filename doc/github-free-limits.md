# GitHub Free: limites e cobrança relevantes ao projeto

Consulta das páginas oficiais em **2026-10-25**. Este documento é a fonte de referência para uma futura skill de controle do projeto; números, preços e regras podem mudar. A modalidade de assinatura e a cobrança efetiva da organização `Modding-VK` **não foram verificadas**: a API de faturamento e a de configurações de Actions retornaram HTTP 403 para a credencial desta sessão. Não inferir plano ou valor devido apenas pela visibilidade pública do repositório.

## Leitura rápida para OpenStarbound

| Recurso | GitHub Free para organizações | Consequência prática |
| --- | ---: | --- |
| Repositórios e colaboradores | Ilimitados; recursos de repositórios privados são reduzidos | Nosso repositório é público. [Planos][plans] |
| Actions em runners padrão para repositório público | Minutos de execução gratuitos | O runner `windows-latest` padrão entra nessa regra; runners maiores são cobrados. [Cobrança do Actions][actions-billing] |
| Actions em repositório privado | 2.000 minutos/mês incluídos | Cobrança adicional ou bloqueio após a franquia, conforme pagamento e orçamento. [Cobrança do Actions][actions-billing] |
| Artefatos do Actions + GitHub Packages | 500 MB de armazenamento compartilhado incluído | O uso observado via API não é a fatura; confirmar plano e uso acumulado na tela de Billing. [Cobrança do Actions][actions-billing] |
| Cache do Actions | 10 GB incluídos **por repositório** | Separado dos artefatos; pode haver expulsão de entradas quando cheio. [Cache][cache] |
| Packages privados | 1 GB/mês de transferência incluído | Pacotes públicos e upload são gratuitos; armazenamento privado compartilha a cota de artefatos. [Packages][packages] |
| Git LFS | 10 GiB de armazenamento e 10 GiB/mês de download incluídos | Conta inclusive em repositórios públicos. Um arquivo LFS no Free pode ter até 2 GB. [LFS cobrança][lfs-billing], [LFS tamanho][lfs-size] |
| Codespaces em conta pessoal Free | 120 horas de computação e 15 GB-mês | Organizações não recebem franquia gratuita de Codespaces. [Codespaces][codespaces] |

O GitHub usa unidades binárias em sua medição do Actions: **1 GB = 1.073.741.824 bytes**. Por isso, registrar sempre bytes brutos, MiB/GiB e a definição usada pela fonte; não comparar MB decimais com uma franquia sem explicitar a unidade. [Cobrança do Actions][actions-billing]

### Retrato consultado via API

Em 2026-10-25, `Modding-VK/OpenStarbound` tinha **28 artefatos ativos**, **3.437.000.568 bytes** (3.277,8 MiB; 3,20 GiB), e **47 caches**, **6.413.687.008 bytes** (6.116,6 MiB; 5,97 GiB). Esses valores são apenas o estoque atual **deste repositório**. Não incluem outros repositórios da organização, Packages, LFS, dados acumulados ao longo do mês nem cobranças. O cache atual está abaixo da franquia padrão de 10 GiB por repositório. Os artefatos superam 500 MiB de estoque, mas isso **não demonstra** cobrança ou bloqueio sem conhecer o plano, a política de cobrança aplicável e o relatório do proprietário.

## Como a cobrança funciona

- Minutos de runners padrão são gratuitos para repositórios públicos. Para repositórios privados, o tempo é atribuído ao **proprietário do repositório**, não ao usuário que disparou o workflow. Runners maiores são sempre cobrados. A franquia de minutos reinicia a cada ciclo mensal. [Cobrança do Actions][actions-billing]
- Armazenamento de artefatos e Packages usa uma franquia compartilhada. O faturamento mede **GB-horas**, ou seja, quanto foi armazenado a cada hora do ciclo; excluí-lo agora reduz o estoque e o consumo futuro, mas não apaga o consumo já acumulado. A atualização dos números de armazenamento no painel pode levar **6 a 12 horas**. Os logs e resumos de jobs não contam como armazenamento de artefatos. [Cobrança do Actions][actions-billing]
- Cache tem franquia separada de **10 GB por repositório**. O limite padrão é 10 GB; aumentar o limite mediante configuração e usar o excedente pode gerar custo. O preço documentado para excedente é **US$ 0,07/GB-mês**; o armazenamento compartilhado de artefatos e Packages é **US$ 0,25/GB-mês**. São preços publicados, não estimativas da fatura desta organização. [Cobrança do Actions][actions-billing], [Cache][cache]
- Sem meio de pagamento válido, o GitHub informa que uso além da franquia é bloqueado. Com pagamento configurado, os **budgets** podem alertar ou interromper uso ao atingir o limite. Orçamentos sobrepostos também podem causar bloqueios inesperados. Alertas podem ser configurados em 75%, 90% e 100%; a franquia incluída pode emitir avisos em 90% e 100%. [Cobrança do Actions][actions-billing], [Budgets][budgets]
- Consultar a fatura em **Organization → Settings → Billing and licensing → Usage** e **Budgets and alerts**, usando conta com acesso de owner/billing manager. A API pública de artefatos não substitui o painel de cobrança. [Consulta de uso][usage]

## Limites operacionais para a skill

| Área | Limite ou comportamento | Regra de controle sugerida |
| --- | --- | --- |
| Workflow | Até 35 dias por run; job em runner hospedado até 6 horas; até 256 jobs por matrix; 50 reexecuções por run | Alertar se a duração típica se aproximar do limite. [Actions limites][actions-limits] |
| Concorrência | Free: até 20 jobs padrão simultâneos, sendo no máximo 5 macOS | Evitar jobs de outras plataformas em pushes quando o objetivo for só Windows. [Actions limites][actions-limits] |
| Arquivo de workflow | Até 500 KB | Alertar antes de expandir demais o YAML. [Actions limites][actions-limits] |
| Cache | Entradas sem acesso por mais de 7 dias são removidas; ao atingir o máximo do repositório, entradas antigas são expulsas | Monitorar bytes, número de entradas e falhas de hit; não aumentar a cota por conta própria. [Cache][cache] |
| Artefatos e logs | Retenção padrão de 90 dias; configurável no repositório/organização e por `retention-days`, respeitando o teto superior | Usar retenção curta para builds de desenvolvimento e manter releases estáveis por outro mecanismo. Excluir um run também exclui seus artefatos. [Remoção de artefatos][artifacts] |
| Releases | Até 1.000 assets por release, cada asset abaixo de 2 GiB; sem limite de tamanho total ou de banda para a release | Installer estável pode ser distribuído como asset de release, somente após pedido explícito de publicação. [Releases][releases] |
| Git normal | Aviso para arquivo acima de 50 MiB; bloqueio acima de 100 MiB; push limitado a 2 GB | Manter binários e instaladores fora do histórico Git. [Arquivos grandes][large-files], [Limites do repositório][repo-limits] |
| Saúde do repositório | GitHub recomenda repositório abaixo de 1 GB, fortemente abaixo de 5 GB; outra página sugere `.git` abaixo de 10 GB como máximo operacional | Tratar ambos como **recomendações de desempenho**, não como uma única cota rígida. [Arquivos grandes][large-files], [Limites do repositório][repo-limits] |
| API REST | 60 requisições/h sem autenticação; em geral 5.000/h autenticadas; `GITHUB_TOKEN` 1.000/h por repositório; limites secundários também existem | Respeitar cabeçalhos de rate limit e `Retry-After`; não fazer polling agressivo. [API REST][api-rate] |
| Proteção de branch | Regras de restrição estão disponíveis em repositórios públicos de organizações GitHub Free | Manter a revisão independente planejada para `main`; confirmar configuração remota com permissão adequada. [Proteção de branch][branch-protection] |

Os limites acima não formam uma promessa de disponibilidade: alguns são recomendações, outros são limites rígidos, outros dependem do plano e da configuração. A skill deve guardar a URL e data de verificação de cada regra, e atualizar as fontes antes de decisões que gerem custos ou excluam dados.

## Contrato sugerido para a futura skill

1. Ler por API `visibility`, lista paginada de artefatos (bytes, expiração e run), lista paginada de caches (bytes e último acesso), runs recentes e status dos workflows. Registrar horário da coleta. Não somar apenas a primeira página.
2. Distinguir **estoque atual** de **consumo faturável acumulado**. Exibir “cobrança desconhecida” se o painel/API de billing não estiver acessível. Nunca deduzir o plano a partir de `public` ou do volume armazenado.
3. Comparar caches com o limite configurado, quando consultável, e artefatos com a franquia **somente após** confirmar o plano e a regra aplicável. Sinalizar alterações de retenção, crescimento por build e duplicação de pacotes.
4. Recomendar redução de `retention-days`, cancelamento de runs supersedidas, remoção de artefatos obsoletos e publicação consciente de releases. Não apagar runs, artefatos, caches ou releases automaticamente sem uma solicitação específica; relatórios e alertas são operações de leitura.
5. Não ampliar orçamento, habilitar cobrança, aumentar cache pago, criar runner maior nem publicar release como resposta automática a um alerta. Não registrar tokens ou conteúdo de artefatos no relatório.

## Fontes oficiais

[plans]: https://docs.github.com/en/get-started/learning-about-github/githubs-plans
[actions-billing]: https://docs.github.com/en/billing/concepts/product-billing/github-actions
[actions-limits]: https://docs.github.com/en/actions/reference/limits
[cache]: https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching#usage-limits-and-eviction-policy
[packages]: https://docs.github.com/en/billing/concepts/product-billing/github-packages
[lfs-billing]: https://docs.github.com/en/billing/concepts/product-billing/git-lfs
[lfs-size]: https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage
[codespaces]: https://docs.github.com/en/billing/concepts/product-billing/github-codespaces
[usage]: https://docs.github.com/en/billing/how-tos/products/view-productlicense-use
[budgets]: https://docs.github.com/en/billing/how-tos/set-up-budgets
[artifacts]: https://docs.github.com/en/actions/how-tos/manage-workflow-runs/remove-workflow-artifacts
[releases]: https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases
[large-files]: https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github
[repo-limits]: https://docs.github.com/en/repositories/creating-and-managing-repositories/repository-limits
[api-rate]: https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api
[branch-protection]: https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches
