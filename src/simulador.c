/*
 * simulador.c — Executa uma sequência de eventos sobre um processo e imprime
 * o rastro (estado de origem, evento, transição, ação, estado de destino).
 *
 * Uso:
 *   ./bin/simulador [-l memoria_livre_kb] [-m memoria_processo_kb] TOKEN...
 *   TOKEN = nome de evento (ADMITIR, DESPACHAR, FIM_QUANTUM, SOLICITAR_ES,
 *           ES_CONCLUIDA, FINALIZAR, SINAL_KILL) ou "mem=N", que altera a
 *           memória livre do sistema (evento do ambiente, não da máquina).
 *
 * Exemplo:
 *   ./bin/simulador ADMITIR DESPACHAR SOLICITAR_ES ES_CONCLUIDA DESPACHAR FINALIZAR
 */
#include "processo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int evento_por_nome(const char *nome)
{
    for (int e = 0; e < NUM_EVENTOS; e++) {
        if (strcmp(nome, nome_evento((Evento)e)) == 0) {
            return e;
        }
    }
    return -1;
}

int main(int argc, char **argv)
{
    Sistema s = { .memoria_livre_kb = 1024 };
    int memoria_processo = 256;
    int i = 1;

    for (; i + 1 < argc && argv[i][0] == '-'; i += 2) {
        if (strcmp(argv[i], "-l") == 0) {
            s.memoria_livre_kb = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "-m") == 0) {
            memoria_processo = atoi(argv[i + 1]);
        } else {
            fprintf(stderr, "opcao desconhecida: %s\n", argv[i]);
            return 2;
        }
    }

    Processo p;
    processo_criar(&p, 1, memoria_processo);
    printf("passo  origem       evento         trans.  acao                   destino      mem.livre\n");
    printf("-----  -----------  -------------  ------  ---------------------  -----------  ---------\n");

    int passo = 0;
    for (; i < argc; i++) {
        if (strncmp(argv[i], "mem=", 4) == 0) {
            s.memoria_livre_kb = atoi(argv[i] + 4);
            printf("  --   (ambiente: memoria livre passa a %d KB)\n", s.memoria_livre_kb);
            continue;
        }
        int e = evento_por_nome(argv[i]);
        if (e < 0) {
            fprintf(stderr, "evento desconhecido: %s\n", argv[i]);
            return 2;
        }
        Resultado r = processo_tratar(&p, &s, (Evento)e);
        printf("%5d  %-11s  %-13s  %-6s  %-21s  %-11s  %6d KB\n", ++passo,
               nome_estado(r.origem), nome_evento((Evento)e), nome_transicao(r.id),
               nome_acao(r.acao), nome_estado(r.destino), s.memoria_livre_kb);
        if (!processo_invariante(&p)) {
            fprintf(stderr, "INVARIANTE VIOLADA no passo %d\n", passo);
            return 1;
        }
    }
    printf("estado final: %s | codigo de saida: %d | despachos: %d | "
           "contextos salvos: %d | admissoes adiadas: %d\n",
           nome_estado(p.estado), p.codigo_saida, p.despachos,
           p.contextos_salvos, p.admissoes_adiadas);
    return 0;
}
