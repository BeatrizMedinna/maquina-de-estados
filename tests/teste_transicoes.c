/*
 * teste_transicoes.c — TESTES DE TRANSIÇÕES (critério: cobertura de todas as
 * transições).
 *
 * CT-01..CT-10: uma transição especificada por caso. O processo é levado ao
 *   estado de origem (preâmbulo BFS), o evento é disparado e confere-se:
 *   estado de destino, identificador da transição, ação (saída de Mealy),
 *   configuração do destino (saída de Moore), contadores, memória e código
 *   de saída. A guarda de admissão é testada nos dois valores-limite:
 *   memória livre = exigida (admite) e memória livre = exigida - 1 (adia).
 * CT-11: os 26 pares (estado, evento) NÃO especificados devem ser ignorados
 *   sem alterar nenhum byte do processo nem a memória do sistema.
 * CT-12: valores de evento fora do domínio são ignorados em todos os estados.
 * CT-13: um valor de estado corrompido leva à recuperação segura.
 */
#include "especificacao.h"

static void testa_aresta(const ArestaEsp *a)
{
    Processo p;
    Sistema s;
    levar_ao_estado(&p, &s, a->origem);
    VERIFICA_IGUAL(p.estado, a->origem);           /* pré-condição */

    if (a->guarda == G_MEM_SUFICIENTE)   s.memoria_livre_kb = MEM_PROCESSO_KB;
    if (a->guarda == G_MEM_INSUFICIENTE) s.memoria_livre_kb = MEM_PROCESSO_KB - 1;

    Processo antes = p;
    int livre_antes = s.memoria_livre_kb;

    Resultado r = processo_tratar(&p, &s, a->evento);

    VERIFICA_IGUAL(r.origem, a->origem);
    VERIFICA_IGUAL(r.destino, a->destino);
    VERIFICA_IGUAL(r.id, a->id);
    VERIFICA_IGUAL(r.acao, a->acao);
    verifica_config(&p, a->destino);
    VERIFICA_IGUAL(p.despachos - antes.despachos, a->d_despachos);
    VERIFICA_IGUAL(p.contextos_salvos - antes.contextos_salvos, a->d_contextos);
    VERIFICA_IGUAL(p.admissoes_adiadas - antes.admissoes_adiadas, a->d_adiadas);
    VERIFICA_IGUAL(s.memoria_livre_kb, livre_antes + a->d_memoria * MEM_PROCESSO_KB);
    VERIFICA_IGUAL(p.codigo_saida, a->codigo_saida);
    VERIFICA_IGUAL(p.pid, antes.pid);
    VERIFICA_IGUAL(p.memoria_kb, antes.memoria_kb);
}

static void ct01(void) { testa_aresta(esp_por_id(T1_ADMITIR)); }
static void ct02(void) { testa_aresta(esp_por_id(T2_ADIAR)); }
static void ct03(void) { testa_aresta(esp_por_id(T3_DESPACHAR)); }
static void ct04(void) { testa_aresta(esp_por_id(T4_PREEMPTAR)); }
static void ct05(void) { testa_aresta(esp_por_id(T5_BLOQUEAR)); }
static void ct06(void) { testa_aresta(esp_por_id(T6_DESBLOQUEAR)); }
static void ct07(void) { testa_aresta(esp_por_id(T7_FINALIZAR)); }
static void ct08(void) { testa_aresta(esp_por_id(T8_MATAR_PRONTO)); }
static void ct09(void) { testa_aresta(esp_por_id(T9_MATAR_BLOQUEADO)); }
static void ct10(void) { testa_aresta(esp_por_id(T10_MATAR_EXECUTANDO)); }

/* Dispara e em s e exige que nada mude (evento ignorado). */
static void exige_ignorado(Estado st, Evento e)
{
    Processo p;
    Sistema s;
    levar_ao_estado(&p, &s, st);
    Processo antes = p;
    int livre_antes = s.memoria_livre_kb;

    Resultado r = processo_tratar(&p, &s, e);

    VERIFICA_IGUAL(r.id, T_IGNORADA);
    VERIFICA_IGUAL(r.acao, ACAO_NENHUMA);
    VERIFICA_IGUAL(r.origem, st);
    VERIFICA_IGUAL(r.destino, st);
    VERIFICA(memcmp(&p, &antes, sizeof p) == 0);   /* nenhum byte alterado */
    VERIFICA_IGUAL(s.memoria_livre_kb, livre_antes);
}

static char matriz[NUM_ESTADOS][NUM_EVENTOS][8];

static void ct11(void)
{
    int ignorados = 0;
    for (int st = 0; st < NUM_ESTADOS; st++) {
        for (int e = 0; e < NUM_EVENTOS; e++) {
            if (esp_par_especificado((Estado)st, (Evento)e)) {
                /* par especificado: registra o que o código faz (com memória
                 * suficiente e, se houver guarda, também insuficiente) */
                Processo p; Sistema s;
                levar_ao_estado(&p, &s, (Estado)st);
                Resultado r = processo_tratar(&p, &s, (Evento)e);
                strcpy(matriz[st][e], nome_transicao(r.id));
                if (esp_aresta((Estado)st, (Evento)e, false) !=
                    esp_aresta((Estado)st, (Evento)e, true)) {   /* há guarda */
                    levar_ao_estado(&p, &s, (Estado)st);
                    s.memoria_livre_kb = 0;
                    r = processo_tratar(&p, &s, (Evento)e);
                    strcat(matriz[st][e], "/");
                    strcat(matriz[st][e], nome_transicao(r.id));
                }
            } else {
                exige_ignorado((Estado)st, (Evento)e);
                ignorados++;
                strcpy(matriz[st][e], ".");
            }
        }
    }
    VERIFICA_IGUAL(ignorados, NUM_ESTADOS * NUM_EVENTOS - 9);  /* 35 - 9 = 26 */
}

static void imprime_matriz(void)
{
    printf("    matriz estado x evento observada (Tn = transicao, . = ignorado):\n");
    printf("    %-10s", "");
    for (int e = 0; e < NUM_EVENTOS; e++) printf(" %s", nome_evento((Evento)e));
    printf("\n");
    for (int st = 0; st < NUM_ESTADOS; st++) {
        printf("    %-10s", nome_estado((Estado)st));
        for (int e = 0; e < NUM_EVENTOS; e++)
            printf(" %-*s", (int)strlen(nome_evento((Evento)e)), matriz[st][e]);
        printf("\n");
    }
}

static void ct12(void)
{
    const int invalidos[] = { NUM_EVENTOS, 99 };
    for (int st = 0; st < NUM_ESTADOS; st++)
        for (int k = 0; k < 2; k++)
            exige_ignorado((Estado)st, (Evento)invalidos[k]);
    /* rótulos de valores fora do domínio */
    VERIFICA(strcmp(nome_evento((Evento)99), "?") == 0);
    VERIFICA(strcmp(nome_transicao((Transicao)99), "?") == 0);
    VERIFICA(strcmp(nome_acao((Acao)99), "?") == 0);
    VERIFICA(strcmp(nome_acao(ACAO_ABORTAR), "abortar") == 0);
}

static void ct13(void)
{
    /* (a) processo com memória alocada cujo estado foi corrompido */
    Processo p; Sistema s;
    levar_ao_estado(&p, &s, EST_PRONTO);
    p.estado = (Estado)42;
    VERIFICA(!processo_invariante(&p));
    VERIFICA(strcmp(nome_estado(p.estado), "?") == 0);
    Resultado r = processo_tratar(&p, &s, EV_DESPACHAR);
    VERIFICA_IGUAL(r.origem, 42);
    VERIFICA_IGUAL(r.id, T_RECUPERACAO);
    VERIFICA_IGUAL(r.acao, ACAO_ABORTAR);
    VERIFICA_IGUAL(r.destino, EST_TERMINADO);
    verifica_config(&p, EST_TERMINADO);
    VERIFICA_IGUAL(p.codigo_saida, CODIGO_SAIDA_SIGKILL);
    VERIFICA_IGUAL(s.memoria_livre_kb, MEM_INICIAL_KB);   /* memória devolvida */

    /* (b) processo ainda sem memória alocada: nada a devolver */
    levar_ao_estado(&p, &s, EST_NOVO);
    p.estado = (Estado)42;
    r = processo_tratar(&p, &s, EV_ADMITIR);
    VERIFICA_IGUAL(r.id, T_RECUPERACAO);
    verifica_config(&p, EST_TERMINADO);
    VERIFICA_IGUAL(s.memoria_livre_kb, MEM_INICIAL_KB);
}

int main(void)
{
    printf("=== TESTES DE TRANSICOES ===\n");
    executar_caso("CT-01", "T1  NOVO -ADMITIR[mem=256 KB, limite]-> PRONTO", ct01);
    executar_caso("CT-02", "T2  NOVO -ADMITIR[mem=255 KB, limite]-> NOVO", ct02);
    executar_caso("CT-03", "T3  PRONTO -DESPACHAR-> EXECUTANDO", ct03);
    executar_caso("CT-04", "T4  EXECUTANDO -FIM_QUANTUM-> PRONTO", ct04);
    executar_caso("CT-05", "T5  EXECUTANDO -SOLICITAR_ES-> BLOQUEADO", ct05);
    executar_caso("CT-06", "T6  BLOQUEADO -ES_CONCLUIDA-> PRONTO", ct06);
    executar_caso("CT-07", "T7  EXECUTANDO -FINALIZAR-> TERMINADO", ct07);
    executar_caso("CT-08", "T8  PRONTO -SINAL_KILL-> TERMINADO", ct08);
    executar_caso("CT-09", "T9  BLOQUEADO -SINAL_KILL-> TERMINADO", ct09);
    executar_caso("CT-10", "T10 EXECUTANDO -SINAL_KILL-> TERMINADO", ct10);
    executar_caso("CT-11", "26 pares nao especificados sao ignorados", ct11);
    imprime_matriz();
    executar_caso("CT-12", "Eventos fora do dominio ignorados nos 5 estados", ct12);
    executar_caso("CT-13", "Estado corrompido -> recuperacao segura", ct13);
    return resumo("Suite de transicoes");
}
