/*
 * teste_caminhos.c — TESTES DE CAMINHOS (critério: cobertura dos caminhos
 * linearmente independentes de McCabe).
 *
 * CC-01..CC-07: cada caminho do conjunto-base é executado de NOVO até
 *   TERMINADO; em cada passo confere-se a transição disparada, o estado de
 *   destino e a configuração observável; ao final, o código de saída, os
 *   contadores e a conservação da memória do sistema.
 * CC-08: verifica automaticamente as propriedades do conjunto-base:
 *   V(G) = E - N + 2 = P + 1, pré-condições de McCabe (entrada e saída
 *   únicas), encadeamento, cobertura de todas as arestas, contribuição de
 *   aresta nova por caminho e posto (rank) da matriz caminho x aresta.
 * CC-09: teste baseado em modelo — 10 000 sequências aleatórias (semente
 *   fixa) comparadas, passo a passo, com o oráculo.
 */
#include "especificacao.h"

static int ultimo_caminho;

static void executa_caminho(void)
{
    const CaminhoEsp *c = &CAMINHOS[ultimo_caminho];
    Processo p;
    Sistema s = { .memoria_livre_kb = MEM_INICIAL_KB };
    int memoria_de_referencia = MEM_INICIAL_KB;
    int n_t2 = 0, n_t3 = 0, n_salvos = 0;

    processo_criar(&p, 7, MEM_PROCESSO_KB);
    for (int i = 0; i < c->n; i++) {
        if (c->passos[i].memoria_livre != MEM_NAO_ALTERA) {
            s.memoria_livre_kb = c->passos[i].memoria_livre;
            memoria_de_referencia = s.memoria_livre_kb;
        }
        const ArestaEsp *a = esp_por_id(c->esperado[i]);
        Estado origem = p.estado;
        Resultado r = processo_tratar(&p, &s, c->passos[i].evento);

        VERIFICA_IGUAL(origem, a->origem);
        VERIFICA_IGUAL(r.id, a->id);
        VERIFICA_IGUAL(r.acao, a->acao);
        VERIFICA_IGUAL(r.destino, a->destino);
        verifica_config(&p, a->destino);

        n_t2 += (r.id == T2_ADIAR);
        n_t3 += (r.id == T3_DESPACHAR);
        n_salvos += (r.id == T4_PREEMPTAR || r.id == T5_BLOQUEAR);
    }
    VERIFICA_IGUAL(p.estado, EST_TERMINADO);
    VERIFICA_IGUAL(p.codigo_saida, c->codigo_saida);
    VERIFICA_IGUAL(p.admissoes_adiadas, n_t2);
    VERIFICA_IGUAL(p.despachos, n_t3);
    VERIFICA_IGUAL(p.contextos_salvos, n_salvos);
    VERIFICA_IGUAL(s.memoria_livre_kb, memoria_de_referencia); /* conservação */
}

static void imprime_rastro(int k)
{
    const CaminhoEsp *c = &CAMINHOS[k];
    printf("    NOVO");
    for (int i = 0; i < c->n; i++) {
        const ArestaEsp *a = esp_por_id(c->esperado[i]);
        printf(" -%s-> %s", nome_transicao(a->id), nome_estado(a->destino));
    }
    printf("  (saida %d)\n", c->codigo_saida);
}

/* ---------------------------------------------------------------------- */
/*  CC-08: propriedades do conjunto-base                                  */
/* ---------------------------------------------------------------------- */
static int indice_aresta(Transicao id)
{
    for (int i = 0; i < NUM_ARESTAS; i++)
        if (ESPEC[i].id == id) return i;
    return -1;
}

/* Posto de uma matriz (eliminação de Gauss com pivoteamento parcial). */
static int posto(double m[NUM_CAMINHOS][NUM_ARESTAS], int lin, int col)
{
    int r = 0;
    for (int c = 0; c < col && r < lin; c++) {
        int piv = r;
        for (int i = r + 1; i < lin; i++)
            if ((m[i][c] < 0 ? -m[i][c] : m[i][c]) > (m[piv][c] < 0 ? -m[piv][c] : m[piv][c]))
                piv = i;
        if ((m[piv][c] < 0 ? -m[piv][c] : m[piv][c]) < 1e-9) continue;
        for (int j = 0; j < col; j++) { double t = m[r][j]; m[r][j] = m[piv][j]; m[piv][j] = t; }
        for (int i = 0; i < lin; i++) {
            if (i == r) continue;
            double f = m[i][c] / m[r][c];
            for (int j = 0; j < col; j++) m[i][j] -= f * m[r][j];
        }
        r++;
    }
    return r;
}

/* BFS genérica sobre a especificação (sentido direto ou reverso). */
static int alcancaveis(Estado raiz, bool reverso)
{
    bool vis[NUM_ESTADOS] = { false };
    Estado fila[NUM_ESTADOS];
    int ini = 0, fim = 0, total = 1;
    vis[raiz] = true;
    fila[fim++] = raiz;
    while (ini < fim) {
        Estado u = fila[ini++];
        for (int i = 0; i < NUM_ARESTAS; i++) {
            Estado de = reverso ? ESPEC[i].destino : ESPEC[i].origem;
            Estado para = reverso ? ESPEC[i].origem : ESPEC[i].destino;
            if (de == u && !vis[para]) { vis[para] = true; fila[fim++] = para; total++; }
        }
    }
    return total;
}

static double matriz[NUM_CAMINHOS][NUM_ARESTAS];
static int vg_formula, vg_predicados, rank_obtido;

static void cc08(void)
{
    const int N = NUM_ESTADOS, E = NUM_ARESTAS;

    /* (a) V(G) pela fórmula de McCabe e pelos nós predicativos */
    int saida[NUM_ESTADOS] = { 0 }, P = 0;
    for (int i = 0; i < E; i++) saida[ESPEC[i].origem]++;
    for (int v = 0; v < N; v++) if (saida[v] > 1) P += saida[v] - 1;
    vg_formula = E - N + 2;
    vg_predicados = P + 1;
    VERIFICA_IGUAL(vg_formula, 7);
    VERIFICA_IGUAL(vg_predicados, vg_formula);

    /* (b) pré-condições: tudo alcançável a partir de NOVO; TERMINADO
     *     alcançável a partir de tudo; TERMINADO sem arestas de saída */
    VERIFICA_IGUAL(alcancaveis(EST_NOVO, false), N);
    VERIFICA_IGUAL(alcancaveis(EST_TERMINADO, true), N);
    VERIFICA_IGUAL(saida[EST_TERMINADO], 0);

    /* (c) número de caminhos = V(G) */
    VERIFICA_IGUAL(NUM_CAMINHOS, vg_formula);

    bool coberta[NUM_ARESTAS] = { false };
    for (int k = 0; k < NUM_CAMINHOS; k++) {
        const CaminhoEsp *c = &CAMINHOS[k];
        Estado atual = EST_NOVO;
        int novas = 0;
        for (int j = 0; j < E; j++) matriz[k][j] = 0;
        for (int i = 0; i < c->n; i++) {
            int j = indice_aresta(c->esperado[i]);
            VERIFICA(j >= 0);
            /* (d) encadeamento: cada aresta parte de onde a anterior chegou */
            VERIFICA_IGUAL(ESPEC[j].origem, atual);
            atual = ESPEC[j].destino;
            matriz[k][j] += 1;
            if (!coberta[j]) { coberta[j] = true; novas++; }
        }
        VERIFICA_IGUAL(atual, EST_TERMINADO);     /* termina na saída */
        VERIFICA(novas >= 1);                     /* (e) aresta nova */
    }
    /* (f) cobertura de todas as arestas */
    for (int j = 0; j < E; j++) VERIFICA(coberta[j]);

    /* (g) independência linear: posto da matriz caminho x aresta = V(G) */
    double copia[NUM_CAMINHOS][NUM_ARESTAS];
    memcpy(copia, matriz, sizeof copia);
    rank_obtido = posto(copia, NUM_CAMINHOS, E);
    VERIFICA_IGUAL(rank_obtido, vg_formula);
}

static void imprime_cc08(void)
{
    printf("    V(G) = E - N + 2 = %d ; V(G) = P + 1 = %d ; posto da matriz = %d\n",
           vg_formula, vg_predicados, rank_obtido);
    printf("    matriz caminho x aresta:   ");
    for (int j = 0; j < NUM_ARESTAS; j++) printf("%4s", nome_transicao(ESPEC[j].id));
    printf("\n");
    for (int k = 0; k < NUM_CAMINHOS; k++) {
        printf("    C%d %-24s", k + 1, "");
        for (int j = 0; j < NUM_ARESTAS; j++) printf("%4.0f", matriz[k][j]);
        printf("\n");
    }
}

/* ---------------------------------------------------------------------- */
/*  CC-09: teste baseado em modelo                                        */
/* ---------------------------------------------------------------------- */
static unsigned long long semente = 20261119ULL;
static unsigned aleatorio(unsigned n)
{
    semente ^= semente << 13;
    semente ^= semente >> 7;
    semente ^= semente << 17;
    return (unsigned)(semente % n);
}

#define NUM_SEQUENCIAS 10000
#define MAX_COMPRIMENTO 30
static long passos_total, contagem[NUM_TRANSICOES], terminaram, divergencias;

static void cc09(void)
{
    static const int memorias[] = { 0, 128, 255, 256, 512, 1024 };
    for (int q = 0; q < NUM_SEQUENCIAS; q++) {
        Processo p;
        Sistema s = { .memoria_livre_kb = MEM_INICIAL_KB };
        processo_criar(&p, q, MEM_PROCESSO_KB);
        int comprimento = 1 + (int)aleatorio(MAX_COMPRIMENTO);
        for (int i = 0; i < comprimento; i++) {
            if (aleatorio(4) == 0) s.memoria_livre_kb = memorias[aleatorio(6)];
            Evento e = (Evento)aleatorio(NUM_EVENTOS);
            Estado origem = p.estado;
            int livre = s.memoria_livre_kb;
            const ArestaEsp *a = esp_aresta(origem, e, p.memoria_kb <= livre);

            Resultado r = processo_tratar(&p, &s, e);
            passos_total++;
            contagem[r.id]++;

            Estado dest_esp  = a ? a->destino : origem;
            Transicao id_esp = a ? a->id : T_IGNORADA;
            int livre_esp    = livre + (a ? a->d_memoria * MEM_PROCESSO_KB : 0);
            bool ok = (r.id == id_esp) && (p.estado == dest_esp) &&
                      (s.memoria_livre_kb == livre_esp) && processo_invariante(&p);
            if (!ok) divergencias++;
        }
        terminaram += (p.estado == EST_TERMINADO);
    }
    VERIFICA_IGUAL(divergencias, 0);
    for (int t = T1_ADMITIR; t <= T10_MATAR_EXECUTANDO; t++) VERIFICA(contagem[t] > 0);
    VERIFICA_IGUAL(contagem[T_RECUPERACAO], 0);
}

static void imprime_cc09(void)
{
    printf("    %d sequencias, %ld passos, %ld divergencias, %ld terminaram\n",
           NUM_SEQUENCIAS, passos_total, divergencias, terminaram);
    printf("    disparos: ignorada=%ld\n    ", contagem[T_IGNORADA]);
    for (int t = T1_ADMITIR; t <= T10_MATAR_EXECUTANDO; t++)
        printf(" %s=%ld", nome_transicao((Transicao)t), contagem[t]);
    printf("\n");
}

int main(void)
{
    printf("=== TESTES DE CAMINHOS ===\n");
    for (int k = 0; k < NUM_CAMINHOS; k++) {
        ultimo_caminho = k;
        executar_caso(CAMINHOS[k].id, CAMINHOS[k].descricao, executa_caminho);
        imprime_rastro(k);
    }
    executar_caso("CC-08", "Conjunto-base: V(G), cobertura e independencia", cc08);
    imprime_cc08();
    executar_caso("CC-09", "Modelo x implementacao: 10 000 sequencias", cc09);
    imprime_cc09();
    return resumo("Suite de caminhos");
}
