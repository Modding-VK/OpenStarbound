# Validação das fases internas de factories — Tracy 23

**Build:** `ea2945be272889087e60e4d953c4e2866f08c081`  
**Objetivo:** separar o custo serial das factories de MicroDungeons antes de
escolher uma otimização, preservando a compatibilidade de geração e mods.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `23.tracy` | 23.044.269 bytes | `18175c739cd8d4ec690a3574cd52ce2bde36820fa4ffa9ca10ef462bcac758de` |
| `exploration.zip` | 13.947.026 bytes | `32ed38134479177d73000892fe79979fe2573e391fe73685eb2ead295aa39ef7` |
| `starbound.log` | 71.500 bytes | `bcef876ac61f9a03ce276a3b6f953c47ec46265f8f096ac93dbb413560471ecc` |
| `starbound.config` | 10.797 bytes | `8ad34b0130073510a32f7188b9c04d51f226da09b66a67582f232fb71aeffdd3` |

A execução encerrou normalmente. A configuração observada usa modo `dynamic`,
workers automáticos, entrega de BaseTiles ampliada e fila pronta de
MicroDungeons; o feeder especulativo de MicroDungeons permaneceu desligado.

## Sessão e relatórios

A sessão do mundo foi isolada no arquivo com início monotônico
`749662796.672`, com 46 janelas e registro final. Isto confirma que o novo
nome de relatório impede a mistura com as runs anteriores. O perfil transitório
`Nowhere` ainda acionou um único fallback de `storage` por `BadVariantType`;
ele foi recuperado e não interrompeu a execução, mas a causa raiz continua
pendente.

Somados por janela, foram concluídos 449 setores e expiraram 108. Os workers
de BaseTiles tiveram 11,0 workers configurados e somente 0,10 worker ativo em
média nas 52 amostras: capacidade de CPU não é o limitador desse trecho.
Houve 435 planos de MicroDungeon agendados, 428 aplicados e 4 resultados stale
(0,93% dos aplicados). A preparação especulativa continua desnecessária nesta
rota.

## Resultado da instrumentação interna

Somente duas ocorrências ultrapassaram o limiar de 25 ms. A nova instrumentação
identifica causas diferentes para o mesmo hash, sem registrar nomes ou payloads
de templates.

| Hash | Setor | Resultado | Total | Fase dominante | Evidência |
| ---: | --- | --- | ---: | --- | --- |
| `-5547472491861117508` | `[326,124]` | não gerado | 148,149 ms | definição | 148,105 ms em `definition`; as 7 tentativas foram rejeitadas por limites, antes de colisão ou `canPlace` |
| `-5547472491861117508` | `[5,125]` | gerado, 2.249 tiles | 40,284 ms | construção | 38,794 ms em `build`; 1 tentativa válida, 0 rejeições |

Portanto, não há uma única otimização segura de factory: o primeiro caso exige
investigar carregamento/instanciação da definição antes de varrer posições; o
segundo é custo de aplicar uma estrutura grande pela thread dona. Em ambos, a
ordem de geração e os efeitos de mod devem permanecer inalterados.

## Tracy

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 5.636 | 7,131 ms | 15,408 ms | 35,333 ms | 310,444 ms |
| `WorldStorage generate queue` | 1.317 | 0,831 ms | 13,885 ms | 43,507 ms | 148,510 ms |
| `Generate microdungeons` | 866 | 0,557 ms | 1,062 ms | 12,002 ms | 148,191 ms |
| `MicroDungeonFactory generate` | 220 | 0,033 ms | 6,388 ms | 10,843 ms | 148,143 ms |
| `WorldServer entities` | 5.589 | 4,870 ms | 10,726 ms | 15,212 ms | 124,510 ms |

O pico de geração coincide com a factory de definição lenta. Já os picos de
entidades são independentes e não autorizam atribuir todo o stutter à geração.
As esperas de setores ainda alcançaram segundos em cadeias com dependências;
logo, otimizar esta factory melhora outliers, mas não substitui o trabalho de
admissão e entrega de setores padrão.

## Decisão e próximos passos

1. Reproduzir o hash `-5547472491861117508` na mesma seed/rota e medir se o
   custo de definição reaparece; só então localizar o subpasso de carregamento
   com uma instrumentação limitada adicional.
2. Medir separadamente a aplicação do footprint de 2.249 tiles, distinguindo
   escrita de tiles, limpeza de entidades e criação de entidades, sempre na
   thread dona. Não paralelizar a publicação.
3. Manter `generationDependencyMicroDungeonPreparationPreview: false` e
   repetir a rota de setores padrão para comparar expirações, espera e p95/p99
   sem mudar múltiplas variáveis.
4. Investigar o `BadVariantType` de `storage` em perfil `Nowhere`; o fallback
   atual protege a telemetria, mas não é uma correção da origem do variant.

Não houve validação de equivalência de saves, multiplayer ou conteúdo de mods
nesta run; qualquer otimização futura permanece preview até essa validação.
