/*
 * teste_estados.c — TESTES DE ESTADOS (critério: cobertura de todos os estados)
 *
 * Para cada estado S da especificação:
 *   1. o processo é conduzido de NOVO até S pelo preâmbulo mais curto (BFS);
 *   2. verifica-se que a máquina está em S e que a configuração observável
 *      (memória, filas, CPU, código de saída) é a saída de Moore prevista;
 *   3. verifica-se a contabilidade de memória do sistema.
 * O caso CE-06 confirma que os cinco estados são independentes, isto é,
 * distinguíveis entre si pela configuração observada (nenhum é redundante).
 */
#include "especificacao.h"

static Processo observado[NUM_ESTADOS];

static void imprime_preambulo(Estado alvo)
{
    Evento seq[NUM_ESTADOS];
    int n = esp_preambulo(alvo, seq, NUM_ESTADOS);
    printf("    preambulo (BFS, %d evento%s):", n, n == 1 ? "" : "s");
    if (n == 0) printf(" nenhum (estado inicial)");
    for (int i = 0; i < n; i++) printf("%s %s", i ? " ->" : "", nome_evento(seq[i]));
    printf("\n");
}

/* Corpo comum aos casos CE-01..CE-05. */
static void testa_estado(Estado alvo)
{
    Processo p;
    Sistema s;
    levar_ao_estado(&p, &s, alvo);

    verifica_config(&p, alvo);

    int esperado_livre = MEM_INICIAL_KB -
                         (MOORE_ESP[alvo].memoria_alocada ? MEM_PROCESSO_KB : 0);
    VERIFICA_IGUAL(s.memoria_livre_kb, esperado_livre);
    VERIFICA_IGUAL(p.pid, 42);
    VERIFICA_IGUAL(p.memoria_kb, MEM_PROCESSO_KB);
    observado[alvo] = p;
}

static void ce01(void)
{
    /* estado inicial: processo_criar deve deixar o processo em NOVO */
    Processo p;
    processo_criar(&p, 42, MEM_PROCESSO_KB);
    VERIFICA_IGUAL(p.estado, EST_NOVO);
    VERIFICA_IGUAL(p.despachos, 0);
    VERIFICA_IGUAL(p.contextos_salvos, 0);
    VERIFICA_IGUAL(p.admissoes_adiadas, 0);
    testa_estado(EST_NOVO);
}
static void ce02(void) { testa_estado(EST_PRONTO); }
static void ce03(void) { testa_estado(EST_EXECUTANDO); }
static void ce04(void) { testa_estado(EST_BLOQUEADO); }
static void ce05(void)
{
    testa_estado(EST_TERMINADO);
    VERIFICA(observado[EST_TERMINADO].codigo_saida != CODIGO_SAIDA_VIVO);
}

/* Assinatura observável de um estado (saída de Moore medida). */
static int assinatura(const Processo *p)
{
    return (p->memoria_alocada << 4) | (p->na_fila_prontos << 3) |
           (p->na_cpu << 2) | (p->na_fila_es << 1) |
           (p->codigo_saida == CODIGO_SAIDA_VIVO);
}

static void ce06(void)
{
    int pares = 0;
    for (int a = 0; a < NUM_ESTADOS; a++) {
        for (int b = a + 1; b < NUM_ESTADOS; b++) {
            VERIFICA(assinatura(&observado[a]) != assinatura(&observado[b]));
            pares++;
        }
    }
    VERIFICA_IGUAL(pares, NUM_ESTADOS * (NUM_ESTADOS - 1) / 2);
    printf("    assinaturas observadas (mem,prontos,cpu,e/s,vivo):\n   ");
    for (int a = 0; a < NUM_ESTADOS; a++) {
        int x = assinatura(&observado[a]);
        printf(" %s=%d%d%d%d%d", nome_estado((Estado)a), (x >> 4) & 1,
               (x >> 3) & 1, (x >> 2) & 1, (x >> 1) & 1, x & 1);
    }
    printf("\n");
}

int main(void)
{
    printf("=== TESTES DE ESTADOS ===\n");
    executar_caso("CE-01", "Estado NOVO (inicial) e sua configuracao", ce01);
    imprime_preambulo(EST_NOVO);
    executar_caso("CE-02", "Estado PRONTO e sua configuracao", ce02);
    imprime_preambulo(EST_PRONTO);
    executar_caso("CE-03", "Estado EXECUTANDO e sua configuracao", ce03);
    imprime_preambulo(EST_EXECUTANDO);
    executar_caso("CE-04", "Estado BLOQUEADO e sua configuracao", ce04);
    imprime_preambulo(EST_BLOQUEADO);
    executar_caso("CE-05", "Estado TERMINADO (final) e sua configuracao", ce05);
    imprime_preambulo(EST_TERMINADO);
    executar_caso("CE-06", "Estados independentes: 10 pares distinguiveis", ce06);
    return resumo("Suite de estados");
}
