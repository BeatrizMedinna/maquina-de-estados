/*
 * processo.h — Máquina de estados do ciclo de vida de um processo
 *
 * Modelo de cinco estados (NOVO, PRONTO, EXECUTANDO, BLOQUEADO, TERMINADO)
 * de um sistema operacional com escalonamento preemptivo por quantum.
 *
 * A máquina é de Mealy: a saída (Acao) é produzida na transição e depende
 * do par (estado atual, evento) e, na admissão, também da guarda de memória.
 * A configuração do processo em cada estado (filas, CPU, memória) é a
 * saída de Moore e é verificada por processo_invariante().
 *
 * Disciplina: Análise de Algoritmos — UCB — 2026
 */
#ifndef PROCESSO_H
#define PROCESSO_H

#include <stdbool.h>

/* ---- Estados (vértices do grafo) -------------------------------------- */
typedef enum {
    EST_NOVO = 0,      /* criado, ainda não admitido pelo SO (inicial)     */
    EST_PRONTO,        /* na fila de prontos, aguardando a CPU             */
    EST_EXECUTANDO,    /* de posse da CPU                                  */
    EST_BLOQUEADO,     /* aguardando a conclusão de uma operação de E/S    */
    EST_TERMINADO,     /* recursos liberados; estado final (absorvente)    */
    NUM_ESTADOS
} Estado;

/* ---- Eventos (entradas da máquina) ------------------------------------ */
typedef enum {
    EV_ADMITIR = 0,    /* escalonador de longo prazo tenta admitir          */
    EV_DESPACHAR,      /* escalonador de curto prazo entrega a CPU          */
    EV_FIM_QUANTUM,    /* interrupção do temporizador (quantum esgotado)    */
    EV_SOLICITAR_ES,   /* chamada de sistema de E/S bloqueante              */
    EV_ES_CONCLUIDA,   /* interrupção do dispositivo: E/S concluída         */
    EV_FINALIZAR,      /* chamada exit() do próprio processo                */
    EV_SINAL_KILL,     /* sinal SIGKILL enviado por outro processo          */
    NUM_EVENTOS
} Evento;

/* ---- Transições (arestas do grafo) ------------------------------------ */
typedef enum {
    T_IGNORADA = 0,       /* evento sem efeito no estado atual (laço "else") */
    T1_ADMITIR,           /* NOVO       -> PRONTO     [memória suficiente]   */
    T2_ADIAR,             /* NOVO       -> NOVO       [memória insuficiente] */
    T3_DESPACHAR,         /* PRONTO     -> EXECUTANDO                        */
    T4_PREEMPTAR,         /* EXECUTANDO -> PRONTO                            */
    T5_BLOQUEAR,          /* EXECUTANDO -> BLOQUEADO                         */
    T6_DESBLOQUEAR,       /* BLOQUEADO  -> PRONTO                            */
    T7_FINALIZAR,         /* EXECUTANDO -> TERMINADO  (término normal)       */
    T8_MATAR_PRONTO,      /* PRONTO     -> TERMINADO  (SIGKILL)              */
    T9_MATAR_BLOQUEADO,   /* BLOQUEADO  -> TERMINADO  (SIGKILL)              */
    T10_MATAR_EXECUTANDO, /* EXECUTANDO -> TERMINADO  (SIGKILL)              */
    T_RECUPERACAO,        /* estado corrompido -> TERMINADO (só no código)   */
    NUM_TRANSICOES
} Transicao;

/* ---- Ações (saídas de Mealy) ------------------------------------------ */
typedef enum {
    ACAO_NENHUMA = 0,
    ACAO_ALOCAR_E_ENFILEIRAR, /* reserva memória e insere na fila de prontos */
    ACAO_ADIAR_ADMISSAO,      /* registra a tentativa e mantém em NOVO        */
    ACAO_CARREGAR_CONTEXTO,   /* retira da fila de prontos e ocupa a CPU      */
    ACAO_SALVAR_E_ENFILEIRAR, /* salva o contexto e volta à fila de prontos   */
    ACAO_SALVAR_E_BLOQUEAR,   /* salva o contexto e entra na fila de E/S      */
    ACAO_MOVER_PARA_PRONTOS,  /* sai da fila de E/S e entra na de prontos     */
    ACAO_LIBERAR_RECURSOS,    /* término normal: libera tudo, código 0        */
    ACAO_ABORTAR,             /* término forçado: libera tudo, código 137     */
    NUM_ACOES
} Acao;

#define CODIGO_SAIDA_VIVO     (-1)
#define CODIGO_SAIDA_NORMAL     0
#define CODIGO_SAIDA_SIGKILL  137   /* 128 + 9 (convenção dos shells POSIX) */

/* Recurso compartilhado consultado pela guarda de admissão. */
typedef struct {
    int memoria_livre_kb;
} Sistema;

/* Bloco de controle do processo (PCB) simplificado. */
typedef struct {
    int    pid;
    int    memoria_kb;        /* memória exigida pelo processo            */
    Estado estado;
    /* configuração observável (saída de Moore) */
    bool   memoria_alocada;
    bool   na_fila_prontos;
    bool   na_cpu;
    bool   na_fila_es;
    int    codigo_saida;      /* -1 enquanto não terminou                 */
    /* contadores (variáveis comuns: não alteram as regras) */
    int    despachos;
    int    contextos_salvos;
    int    admissoes_adiadas;
} Processo;

/* Resultado de um passo da máquina. */
typedef struct {
    Estado    origem;
    Estado    destino;
    Transicao id;
    Acao      acao;
} Resultado;

void      processo_criar(Processo *p, int pid, int memoria_kb);
Resultado processo_tratar(Processo *p, Sistema *s, Evento e);
bool      processo_invariante(const Processo *p);

const char *nome_estado(Estado e);
const char *nome_evento(Evento e);
const char *nome_transicao(Transicao t);
const char *nome_acao(Acao a);

#endif /* PROCESSO_H */
