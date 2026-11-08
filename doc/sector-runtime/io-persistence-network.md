# IO, persistência, carga e entrega ao cliente

[Índice](README.md) · [Memória](memory-and-prefetch.md) · [Regiões](regional-threads.md)

## Propósito e melhoria esperada

Reduzir espera em revisitas, evitar decode duplicado e impedir que geração
pronta aguarde instalação/transmissão desnecessariamente. Separar trabalho
privado de IO/decode de ativação, callbacks e transações confirmadas.

## Estado e fonte

`WorldStorage::loadSectorToLevel` consulta B-tree, decodifica, instala e
inicializa entidades conforme o nível. `savedTileDecodeAsyncPreview` recebe
cópias de bytes da dona em chamadas Tiles-only; ele não torna a leitura do
banco paralela. Uma promoção para Loaded ainda usa caminho síncrono.
Ver a [auditoria de IO](../sector-io-ready-queue-audit-2026-11-07.md).

## IO-01 — Pipeline único de leitura e promoção

Representar pedido de dados salvos com mundo/storage epoch, setor, ticket,
versão do conteúdo e carga alvo. Reutilizar a mesma leitura/decode quando um
pedido de prefetch vira visível. Aumentar carga alvo não deve criar outro
produtor para os mesmos bytes enquanto o anterior está em voo.

O worker decodifica bytes próprios e produz tiles/JSON sem efeitos. A dona
valida encarnação, dados atuais e carga antes de normalizar IDs, instalar e
executar callbacks. Revalidar antes de qualquer criação de entidade ou índice
único. Ausência no banco é um resultado explícito, não um erro suprimido.

Aceitação: Tiles→Loaded, unload/reload, cancelamento e write posterior não
duplicam decode nem instalam um resultado de uma solicitação velha.

## IO-02 — Serviço de banco e transações

O objetivo futuro é retirar espera bloqueante da dona através de um serviço
que serialize operações do mesmo banco e devolva bytes/snapshots. A instância
mutável de `m_db` não vai diretamente para workers.

Documentar ordem de read/write, visibilidade de writes ainda não commitados,
sync, rollback, close e erro. Um handle de leitura independente que veja
somente o último checkpoint pode devolver estado antigo frente a uma escrita
recente do mundo. Definir versões de conteúdo separadas do cache de páginas
ou do mero ticket da chamada.

Aceitação: histórias de operações concorrentes equivalem a uma sequência
autorizada, sem leitura obsoleta aceita nem deadlock entre serviço e dona.

## IO-03 — Checkpoint e recuperação

Salvar apenas estado confirmado de uma fronteira consistente. Tiles,
entidades, índices únicos, metadata e estado Lua exigem definição de alcance
transacional. Sync periódico não pode confirmar metade de uma estrutura ou
uma transferência de entidade ainda não aceita pelo destino.

Se a transação do B-tree não cobrir a unidade lógica inteira, definir journal
de intenção e replay idempotente antes de permitir aplicação interrompível.
Após falha, recuperar ou rejeitar a intenção antes da ativação de clientes.
Durabilidade e visibilidade são propriedades distintas.

No runtime regional, coletar snapshots por passo/epoch e impedir que uma dona
avance o checkpoint enquanto outra ainda fornece estado de um passo
incompatível. O serviço de armazenamento serializa o banco por mundo.

Aceitação: crash/restart nos pontos críticos não produz perda, duplicação,
estrutura parcial ou geração repetida de setor salvo.

## IO-04 — Unload, drain e retorno ao legado

Unload retira autorização de publicar, preserva reservas usadas e aguarda ou
descarta conclusões pelo protocolo do produtor. Entidades `keepAlive`, índices
únicos e referências de scripts conservam seus contratos.

Falha de dona regional não autoriza uma transferência imediata de seu estado
parcial à WorldThread central. Trocar modo exige checkpoint válido/reload.
Buffers privados tardios podem ser liberados após conclusão sem alcançar o
mundo destruído. GPU precisa de conclusão física antes da reciclagem.

## IO-05 — Pacotes e aplicação no cliente

O protocolo atual pressupõe um `TileArrayUpdate` inicial antes de outros
updates daquele setor. `sectorActive` exige carga e geração completas. O
primeiro rework preserva essa ordem e o formato físico 32×32.

Reduzir custo de construir o pacote com dados confirmados e buffers próprios;
compressão/serialização puras podem ser avaliadas em workers após capturar
regras de compatibilidade e ordem dos pacotes. Não emitir estado provisório
como se fosse o pacote inicial final.

Registrar marcos de interesse, preparação, publicação, packet enqueue, envio,
recepção e aplicação. Pacote pronto no servidor não prova que o setor já é
visível no cliente. Correlacionar por identidade segura e distinguir relógios
de processos distintos; subtrair relógios não sincronizados é inválido.

## Escopo e aceitação conjunta

Run 29 mostrou consultas de tiles baratas; esta frente é especialmente
relevante para revisitas/cargas salvas, enquanto o [grafo](dependency-scheduler.md)
é a prioridade de geração inédita. Medir read, decode, normalização,
instalação, entidade/callback, sync, packet e client apply separadamente.

Testar save modificado, default ausente, metadados antigos, erro de bytes,
IDs inválidos, multiplayer e cliente compatível. Nenhuma otimização pode
regenerar terreno salvo por confundir cache miss com setor inexistente.
Regras de [compatibilidade](compatibility.md) e cenários de
[validação](validation-and-telemetry.md) bloqueiam adoção.
