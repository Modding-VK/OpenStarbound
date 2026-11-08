# Threads dedicadas e propriedade regional

[Índice](README.md) · [WorldThread](worldthread.md) · [Compatibilidade](compatibility.md)

## Propósito e decisão de unidade

Permitir progresso autoritativo simultâneo dentro do mesmo mundo, distribuindo
estado entre donos exclusivos. A unidade vigente é a **região lógica 2×2**:
até quatro setores físicos 32×32, com uma thread dedicada do sistema operacional.
Uma região não troca sua simulação com outra dentro de uma pool. Jobs de
terreno, decode e GPU continuam em executores auxiliares.

Essa regra consolida a decisão no início do
[plano regional anterior](../world-region-rework-advanced-concept.md).
As passagens que ainda descreviam até quatro threads para uma região foram
alinhadas a essa decisão na documentação; a formulação antiga fica no Git.

## Estado real

`WorldRegionPartition` define a geometria. O coordenador observa metadados e
revalida turnos em sombra. Os previews de lifecycle/mailbox não executam Lua,
simulação ou instalação autoritativa em paralelo. A WorldThread central ainda
é a dona do mundo no runtime atual.

## REG-01 — Lifecycle e identidade da dona

A primeira carga efetiva de um setor para Tiles/Entities dá origem à thread
de sua região; o unload do último membro encerra a thread após drain. Apenas
enfileirar um setor não cria uma dona. O preview atual instala tiles antes do
callback; a transferência autoritativa futura precisará de fronteira explícita
antes de conceder acesso local.

Manter epoch de mundo, geração da região e referência à dona. Endereço de
objeto/thread não é identidade. Falha de criação deve impedir a ativação do
modo regional afetado e preservar recuperação por uma fronteira consistente.
Não alterar interesse, TTL ou `keepAlive` para caber num limite de threads.

Dados preparados em RAM não são carga ativa de `WorldStorage`; o prefetch
privado não deve criar artificialmente milhares de donos de simulação.

## REG-02 — Estado local e visões de borda

Inventariar tiles, líquidos, colisão, entidades ancoradas, fios, dano e Lua
local que cada dona possui. Consultas externas usam snapshots confirmados,
com idade e revisão. Região ausente é indisponível, não terreno vazio.
Regiões parciais e wrap horizontal usam `WorldGeometry` canônica.

Durante um turno, somente a dona escreve seu estado. Uma área visual ou
metaboundbox atravessando a fronteira não concede escrita à região vizinha.
Índices globais e referências de entidades têm contrato próprio de publicação.

## REG-03 — Tempo e operações entre regiões

Primeiro alvo: passo lógico comum, entradas sequenciadas e publicação
confirmada na fronteira do passo. Cada thread executa no máximo um turno
local aplicável, produzindo intenções remotas versionadas. Conclusão mais
rápida não decide a ordem de conflito.

Operações multirregião reservam footprint completo numa ordem estável, sem
Lua segurando locks de outros donos e sem espera circular entre threads.
Definir commit/rejeição, timeout e recuperação. Fios, líquidos e roteadores
não são independentes só porque têm âncoras em regiões diferentes.

Passos realmente independentes são uma entrega posterior, condicionada a
protocolo de relógio, mensagens, clientes e checkpoint. Remover a barreira
global antes disso cria leituras de futuros incompatíveis.

## REG-04 — Transferência de entidades

Uma entidade tem `(EntityId, incarnation, ownerGeneration)` e no máximo uma
dona autoritativa por passo. Origem prepara transferência, destino valida e
o commit atualiza dono/índice/rede conjuntamente. Mensagens antigas são
rejeitadas; idempotência impede update, item ou dano duplicado.

Manter a autoridade atual de jogadores, entidades master do cliente,
monstros, veículos e stagehands conforme o tipo. Projeções de leitura não
executam IA. Essa tarefa não retoma externalização de NPCs, que segue suspensa.

## REG-05 — Falha, checkpoint e custo de escala

Uma falha invalida saídas ainda não confirmadas e interrompe o passo afetado.
Retorno ao legado exige checkpoint consistente ou novo carregamento. Não
reaproveitar uma região parcialmente publicada como se tivesse sido revertida.

Medir pilhas, memória por thread, criação/join, latência de acordar, threads
simultaneamente prontas, custo da barreira, mensagens e regiões lentas. Threads
sem trabalho aguardam condição. Muitas threads dedicadas não significam muitos
núcleos disponíveis; orçamento dos jobs auxiliares considera a simulação.

Se o custo das threads for inviável, manter o modo regional desligado naquele
cenário até uma decisão documentada. Trocar por pool de simulação ou alterar
regras de carga é uma mudança de arquitetura separada.

## Melhoria esperada e aceitação

Espera-se sobreposição de updates locais independentes e menor concentração
na thread central. Bases que compartilham muitas redes ou callbacks globais
podem continuar serializadas; isso precisa aparecer nas métricas.

Aceitação: exclusão de dona, zero update duplicado, equivalência nas bordas,
handoff rastreável, saves consistentes, multiplayer, falha recuperável e ganho
líquido incluindo overhead. Dependências: [compatibilidade](compatibility.md),
[persistência](io-persistence-network.md), [scheduler](dependency-scheduler.md)
e matriz de [validação](validation-and-telemetry.md). A sequência regional fica
separada de H0–H5 no [roadmap](roadmap.md).
