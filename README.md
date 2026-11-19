# Máquina de Estados — Ciclo de vida de um processo

Trabalho prático de **Análise de Algoritmos** (UCB, 2026) — Prof. Zoe Roberto Magalhães Junior.

| Integrante | Matrícula |
|---|---|
| Caroline Zaiatz | UC24101689 |
| Beatriz Nascimento Costa Medina | UC24101412 |
| Eloisa Sousa de Jesus | UC23100484 |

Implementação em C de uma máquina de estados finita (Mealy) que modela o ciclo de vida de
um processo em um sistema operacional com escalonamento preemptivo (modelo de cinco estados),
validada por testes **100% automatizados** de estados, transições e caminhos independentes
(Complexidade Ciclomática de McCabe).

## Modelo

* **Estados (N = 5):** `NOVO` (inicial), `PRONTO`, `EXECUTANDO`, `BLOQUEADO`, `TERMINADO` (final).
* **Eventos (7):** `ADMITIR`, `DESPACHAR`, `FIM_QUANTUM`, `SOLICITAR_ES`, `ES_CONCLUIDA`, `FINALIZAR`, `SINAL_KILL`.
* **Transições (E = 10):**

| Id | Origem | Evento [guarda] | Destino | Ação (saída de Mealy) |
|---|---|---|---|---|
| T1 | NOVO | ADMITIR [memória suficiente] | PRONTO | alocar memória e enfileirar |
| T2 | NOVO | ADMITIR [memória insuficiente] | NOVO | adiar admissão |
| T3 | PRONTO | DESPACHAR | EXECUTANDO | carregar contexto |
| T4 | EXECUTANDO | FIM_QUANTUM | PRONTO | salvar contexto e enfileirar |
| T5 | EXECUTANDO | SOLICITAR_ES | BLOQUEADO | salvar contexto e bloquear |
| T6 | BLOQUEADO | ES_CONCLUIDA | PRONTO | mover para a fila de prontos |
| T7 | EXECUTANDO | FINALIZAR | TERMINADO | liberar recursos (código 0) |
| T8 | PRONTO | SINAL_KILL | TERMINADO | abortar (código 137) |
| T9 | BLOQUEADO | SINAL_KILL | TERMINADO | abortar (código 137) |
| T10 | EXECUTANDO | SINAL_KILL | TERMINADO | abortar (código 137) |

Os outros 26 pares (estado, evento) são ignorados sem alterar o processo.
V(G) = E − N + 2 = 10 − 5 + 2 = **7** caminhos independentes.

## Estrutura

```
include/processo.h        tipos (estados, eventos, transições, ações) e interface
src/processo.c            função de transição (switch estado × switch evento)
src/simulador.c           executa uma sequência de eventos e imprime o rastro
tests/teste.h             mini-arcabouço de testes (sem dependências externas)
tests/especificacao.h     ORÁCULO: tabelas do relatório transcritas como dados
tests/teste_estados.c     CE-01..CE-06  cobertura de estados
tests/teste_transicoes.c  CT-01..CT-13  cobertura de transições + robustez
tests/teste_caminhos.c    CC-01..CC-09  caminhos independentes + teste por modelo
tests/mutacao.sh          análise de mutantes (18 defeitos injetados)
resultados/               saídas geradas por `make resultados`
relatorio/                fonte LaTeX e PDF do relatório
.github/workflows/        integração contínua (GCC e Clang)
```

## Como compilar e testar

Requisitos: GCC ou Clang (C11), `make`; opcionalmente `pip install gcovr lizard`.

```sh
make              # compila (avisos tratados como erros)
make test         # executa as três suítes; código de saída 0 = tudo aprovado
make cobertura    # cobertura de linhas e desvios de src/processo.c (gcovr)
make complexidade # V(G) de cada função (lizard)
make mutacao      # análise de mutantes
make sanitizers   # AddressSanitizer + UBSan
make resultados   # regenera todos os arquivos de resultados/

./bin/simulador ADMITIR DESPACHAR SOLICITAR_ES ES_CONCLUIDA DESPACHAR FINALIZAR
./bin/simulador -l 100 ADMITIR mem=1024 ADMITIR DESPACHAR FINALIZAR   # caminho C2
```

## Resultados (última execução)

* 28 casos de teste, 937 verificações, 28 aprovados.
* Cobertura de `src/processo.c`: 100% das linhas (126/126) e 100% dos desvios (33/33).
* V(G) do grafo = 7; V(G) de `processo_tratar` = 16 (lizard).
* Mutantes: 18/18 mortos.
