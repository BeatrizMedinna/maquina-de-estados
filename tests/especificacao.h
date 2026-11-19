/*
 * especificacao.h — ORÁCULO dos testes.
 *
 * Esta é a especificação do relatório (Tabelas de estados e de transições e
 * os caminhos independentes) transcrita como DADOS. Ela é escrita de forma
 * independente da implementação em src/processo.c: os testes comparam o que
 * o código faz com o que a especificação diz, e nunca o código com ele mesmo.
 */
#ifndef ESPECIFICACAO_H
#define ESPECIFICACAO_H

#include "processo.h"
#include "teste.h"

#define MEM_PROCESSO_KB  256    /* memória exigida pelo processo de teste */
#define MEM_INICIAL_KB  1024    /* memória livre inicial do sistema       */

/* ---------------------------------------------------------------------- */
/*  Saída de Moore esperada em cada estado (Tabela de estados)            */
/* ---------------------------------------------------------------------- */
typedef struct {
    bool memoria_alocada, na_fila_prontos, na_cpu, na_fila_es, vivo;
} ConfigEsperada;

static const ConfigEsperada MOORE_ESP[NUM_ESTADOS] = {
    /*                  mem    prontos cpu    e/s    vivo  */
    [EST_NOVO]       = {false, false, false, false, true },
    [EST_PRONTO]     = {true,  true,  false, false, true },
    [EST_EXECUTANDO] = {true,  false, true,  false, true },
    [EST_BLOQUEADO]  = {true,  false, false, true,  true },
    [EST_TERMINADO]  = {false, false, false, false, false},
};

/* ---------------------------------------------------------------------- */
/*  Transições especificadas (Tabela de transições)                       */
/* ---------------------------------------------------------------------- */
typedef enum { G_NENHUMA, G_MEM_SUFICIENTE, G_MEM_INSUFICIENTE } Guarda;

typedef struct {
    Transicao id;
    Estado    origem;
    Evento    evento;
    Guarda    guarda;
    Estado    destino;
    Acao      acao;
    int d_despachos, d_contextos, d_adiadas;  /* variação dos contadores */
    int d_memoria;     /* -1: reserva memória; +1: devolve; 0: não mexe   */
    int codigo_saida;  /* código de saída após a transição               */
} ArestaEsp;

#define NUM_ARESTAS 10
static const ArestaEsp ESPEC[NUM_ARESTAS] = {
  /* id, origem, evento, guarda, destino,
     ação, Δdespachos, Δcontextos, Δadiadas, Δmemória, código de saída */
  {T1_ADMITIR, EST_NOVO, EV_ADMITIR, G_MEM_SUFICIENTE, EST_PRONTO,
   ACAO_ALOCAR_E_ENFILEIRAR, 0, 0, 0, -1, CODIGO_SAIDA_VIVO},
  {T2_ADIAR, EST_NOVO, EV_ADMITIR, G_MEM_INSUFICIENTE, EST_NOVO,
   ACAO_ADIAR_ADMISSAO, 0, 0, 1, 0, CODIGO_SAIDA_VIVO},
  {T3_DESPACHAR, EST_PRONTO, EV_DESPACHAR, G_NENHUMA, EST_EXECUTANDO,
   ACAO_CARREGAR_CONTEXTO, 1, 0, 0, 0, CODIGO_SAIDA_VIVO},
  {T4_PREEMPTAR, EST_EXECUTANDO, EV_FIM_QUANTUM, G_NENHUMA, EST_PRONTO,
   ACAO_SALVAR_E_ENFILEIRAR, 0, 1, 0, 0, CODIGO_SAIDA_VIVO},
  {T5_BLOQUEAR, EST_EXECUTANDO, EV_SOLICITAR_ES, G_NENHUMA, EST_BLOQUEADO,
   ACAO_SALVAR_E_BLOQUEAR, 0, 1, 0, 0, CODIGO_SAIDA_VIVO},
  {T6_DESBLOQUEAR, EST_BLOQUEADO, EV_ES_CONCLUIDA, G_NENHUMA, EST_PRONTO,
   ACAO_MOVER_PARA_PRONTOS, 0, 0, 0, 0, CODIGO_SAIDA_VIVO},
  {T7_FINALIZAR, EST_EXECUTANDO, EV_FINALIZAR, G_NENHUMA, EST_TERMINADO,
   ACAO_LIBERAR_RECURSOS, 0, 0, 0, +1, CODIGO_SAIDA_NORMAL},
  {T8_MATAR_PRONTO, EST_PRONTO, EV_SINAL_KILL, G_NENHUMA, EST_TERMINADO,
   ACAO_ABORTAR, 0, 0, 0, +1, CODIGO_SAIDA_SIGKILL},
  {T9_MATAR_BLOQUEADO, EST_BLOQUEADO, EV_SINAL_KILL, G_NENHUMA, EST_TERMINADO,
   ACAO_ABORTAR, 0, 0, 0, +1, CODIGO_SAIDA_SIGKILL},
  {T10_MATAR_EXECUTANDO, EST_EXECUTANDO, EV_SINAL_KILL, G_NENHUMA, EST_TERMINADO,
   ACAO_ABORTAR, 0, 0, 0, +1, CODIGO_SAIDA_SIGKILL},
};

/* Procura na especificação a aresta disparada por (estado, evento, guarda).
 * Devolve NULL quando o par não é especificado (evento deve ser ignorado). */
static inline const ArestaEsp *esp_aresta(Estado s, Evento e, bool memoria_suficiente)
{
    for (int i = 0; i < NUM_ARESTAS; i++) {
        const ArestaEsp *a = &ESPEC[i];
        if (a->origem != s || a->evento != e) continue;
        if (a->guarda == G_MEM_SUFICIENTE && !memoria_suficiente) continue;
        if (a->guarda == G_MEM_INSUFICIENTE && memoria_suficiente) continue;
        return a;
    }
    return NULL;
}

static inline bool esp_par_especificado(Estado s, Evento e)
{
    return esp_aresta(s, e, true) != NULL || esp_aresta(s, e, false) != NULL;
}

/* ---------------------------------------------------------------------- */
/*  Preâmbulo: sequência mais curta de eventos que leva de NOVO ao estado  */
/*  desejado. Calculada por BUSCA EM LARGURA (BFS) sobre a especificação,  */
/*  pois a BFS encontra o caminho com o menor número de arestas.           */
/* ---------------------------------------------------------------------- */
static inline int esp_preambulo(Estado alvo, Evento seq[], int max)
{
    int pai_aresta[NUM_ESTADOS];
    bool visitado[NUM_ESTADOS] = { false };
    Estado fila[NUM_ESTADOS];
    int ini = 0, fim = 0;

    for (int i = 0; i < NUM_ESTADOS; i++) pai_aresta[i] = -1;
    visitado[EST_NOVO] = true;
    fila[fim++] = EST_NOVO;
    while (ini < fim) {
        Estado u = fila[ini++];
        for (int i = 0; i < NUM_ARESTAS; i++) {
            const ArestaEsp *a = &ESPEC[i];
            if (a->origem != u || a->guarda == G_MEM_INSUFICIENTE) continue;
            if (!visitado[a->destino]) {
                visitado[a->destino] = true;
                pai_aresta[a->destino] = i;
                fila[fim++] = a->destino;
            }
        }
    }
    /* reconstrói o caminho de trás para frente */
    Evento inv[NUM_ESTADOS];
    int n = 0;
    for (Estado v = alvo; v != EST_NOVO; v = ESPEC[pai_aresta[v]].origem) {
        inv[n++] = ESPEC[pai_aresta[v]].evento;
    }
    for (int i = 0; i < n && i < max; i++) seq[i] = inv[n - 1 - i];
    return n;
}

/* Cria um processo e o conduz, pelo preâmbulo, até o estado pedido. */
static inline void levar_ao_estado(Processo *p, Sistema *s, Estado alvo)
{
    Evento seq[NUM_ESTADOS];
    int n = esp_preambulo(alvo, seq, NUM_ESTADOS);
    s->memoria_livre_kb = MEM_INICIAL_KB;
    processo_criar(p, 42, MEM_PROCESSO_KB);
    for (int i = 0; i < n; i++) processo_tratar(p, s, seq[i]);
}

/* Confere a configuração observável do processo contra a saída de Moore. */
static inline void verifica_config(const Processo *p, Estado esperado)
{
    const ConfigEsperada *c = &MOORE_ESP[esperado];
    VERIFICA_IGUAL(p->estado, esperado);
    VERIFICA_IGUAL(p->memoria_alocada, c->memoria_alocada);
    VERIFICA_IGUAL(p->na_fila_prontos, c->na_fila_prontos);
    VERIFICA_IGUAL(p->na_cpu, c->na_cpu);
    VERIFICA_IGUAL(p->na_fila_es, c->na_fila_es);
    VERIFICA_IGUAL(p->codigo_saida == CODIGO_SAIDA_VIVO, c->vivo);
    VERIFICA(processo_invariante(p));
}

/* ---------------------------------------------------------------------- */
/*  Caminhos linearmente independentes (método da linha de base)          */
/* ---------------------------------------------------------------------- */
#define MAX_PASSOS 8
#define NUM_CAMINHOS 7
#define MEM_NAO_ALTERA (-1)

typedef struct {
    Evento evento;
    int    memoria_livre;    /* ajuste do ambiente antes do evento */
} Passo;

typedef struct {
    const char *id;
    const char *descricao;
    int         n;
    Passo       passos[MAX_PASSOS];
    Transicao   esperado[MAX_PASSOS];
    int         codigo_saida;
} CaminhoEsp;

#define P(ev)       { ev, MEM_NAO_ALTERA }
#define PM(ev, m)   { ev, m }

static const CaminhoEsp CAMINHOS[NUM_CAMINHOS] = {
 {"CC-01", "C1 linha de base: admite, executa e termina", 3,
  {P(EV_ADMITIR), P(EV_DESPACHAR), P(EV_FINALIZAR)},
  {T1_ADMITIR, T3_DESPACHAR, T7_FINALIZAR}, CODIGO_SAIDA_NORMAL},
 {"CC-02", "C2 admissao adiada por falta de memoria", 4,
  {PM(EV_ADMITIR, 100), PM(EV_ADMITIR, MEM_INICIAL_KB), P(EV_DESPACHAR), P(EV_FINALIZAR)},
  {T2_ADIAR, T1_ADMITIR, T3_DESPACHAR, T7_FINALIZAR}, CODIGO_SAIDA_NORMAL},
 {"CC-03", "C3 morto por SIGKILL na fila de prontos", 2,
  {P(EV_ADMITIR), P(EV_SINAL_KILL)},
  {T1_ADMITIR, T8_MATAR_PRONTO}, CODIGO_SAIDA_SIGKILL},
 {"CC-04", "C4 preemptado por fim de quantum", 5,
  {P(EV_ADMITIR), P(EV_DESPACHAR), P(EV_FIM_QUANTUM), P(EV_DESPACHAR), P(EV_FINALIZAR)},
  {T1_ADMITIR, T3_DESPACHAR, T4_PREEMPTAR, T3_DESPACHAR, T7_FINALIZAR}, CODIGO_SAIDA_NORMAL},
 {"CC-05", "C5 bloqueia em E/S e volta a executar", 6,
  {P(EV_ADMITIR), P(EV_DESPACHAR), P(EV_SOLICITAR_ES), P(EV_ES_CONCLUIDA), P(EV_DESPACHAR), P(EV_FINALIZAR)},
  {T1_ADMITIR, T3_DESPACHAR, T5_BLOQUEAR, T6_DESBLOQUEAR, T3_DESPACHAR, T7_FINALIZAR}, CODIGO_SAIDA_NORMAL},
 {"CC-06", "C6 morto por SIGKILL durante a execucao", 3,
  {P(EV_ADMITIR), P(EV_DESPACHAR), P(EV_SINAL_KILL)},
  {T1_ADMITIR, T3_DESPACHAR, T10_MATAR_EXECUTANDO}, CODIGO_SAIDA_SIGKILL},
 {"CC-07", "C7 morto por SIGKILL enquanto bloqueado", 4,
  {P(EV_ADMITIR), P(EV_DESPACHAR), P(EV_SOLICITAR_ES), P(EV_SINAL_KILL)},
  {T1_ADMITIR, T3_DESPACHAR, T5_BLOQUEAR, T9_MATAR_BLOQUEADO}, CODIGO_SAIDA_SIGKILL},
};

static inline const ArestaEsp *esp_por_id(Transicao id)
{
    for (int i = 0; i < NUM_ARESTAS; i++)
        if (ESPEC[i].id == id) return &ESPEC[i];
    return NULL;
}

#endif /* ESPECIFICACAO_H */
