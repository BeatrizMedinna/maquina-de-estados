/*
 * processo.c — Implementação da máquina de estados do ciclo de vida de um
 * processo (modelo de cinco estados).
 *
 * Organização (estrutura clássica de FSM em C):
 *   - um switch externo sobre o ESTADO atual;
 *   - em cada estado, um switch interno sobre o EVENTO;
 *   - cada aresta do diagrama corresponde a exatamente um "return" que
 *     executa a ação (saída de Mealy) e muda o estado na mesma operação;
 *   - todo par (estado, evento) não especificado cai no "default" interno e
 *     é ignorado: o processo não é alterado (robustez).
 */
#include "processo.h"

#include <stddef.h>

/* ======================================================================= */
/*  Ações elementares (efeitos colaterais das transições)                  */
/* ======================================================================= */

static void alocar_e_enfileirar(Processo *p, Sistema *s)
{
    s->memoria_livre_kb -= p->memoria_kb;
    p->memoria_alocada = true;
    p->na_fila_prontos = true;
}

static void carregar_contexto(Processo *p)
{
    p->na_fila_prontos = false;
    p->na_cpu = true;
    p->despachos++;
}

static void salvar_e_enfileirar(Processo *p)
{
    p->contextos_salvos++;
    p->na_cpu = false;
    p->na_fila_prontos = true;
}

static void salvar_e_bloquear(Processo *p)
{
    p->contextos_salvos++;
    p->na_cpu = false;
    p->na_fila_es = true;
}

static void mover_para_prontos(Processo *p)
{
    p->na_fila_es = false;
    p->na_fila_prontos = true;
}

/* Remove o processo de qualquer fila/CPU e devolve a memória, se houver. */
static void liberar(Processo *p, Sistema *s, int codigo)
{
    if (p->memoria_alocada) {
        s->memoria_livre_kb += p->memoria_kb;
    }
    p->memoria_alocada = false;
    p->na_fila_prontos = false;
    p->na_cpu = false;
    p->na_fila_es = false;
    p->codigo_saida = codigo;
}

/* Registra a mudança de estado e monta o resultado do passo. */
static Resultado mudar(Processo *p, Resultado r, Estado destino,
                       Transicao id, Acao acao)
{
    p->estado = destino;
    r.destino = destino;
    r.id = id;
    r.acao = acao;
    return r;
}

/* ======================================================================= */
/*  Interface pública                                                      */
/* ======================================================================= */

void processo_criar(Processo *p, int pid, int memoria_kb)
{
    p->pid = pid;
    p->memoria_kb = memoria_kb;
    p->estado = EST_NOVO;
    p->memoria_alocada = false;
    p->na_fila_prontos = false;
    p->na_cpu = false;
    p->na_fila_es = false;
    p->codigo_saida = CODIGO_SAIDA_VIVO;
    p->despachos = 0;
    p->contextos_salvos = 0;
    p->admissoes_adiadas = 0;
}

/*
 * Função de transição δ e função de saída λ da máquina de Mealy.
 * Complexidade ciclomática (McCabe, contagem de nós predicativos):
 *   5 "case" externos + 9 "case" internos + 1 "if" (guarda) = 15  =>  V(G) = 16.
 */
Resultado processo_tratar(Processo *p, Sistema *s, Evento e)
{
    Resultado r = { p->estado, p->estado, T_IGNORADA, ACAO_NENHUMA };

    switch (p->estado) {
    case EST_NOVO:
        switch (e) {
        case EV_ADMITIR:
            if (p->memoria_kb <= s->memoria_livre_kb) {          /* guarda */
                alocar_e_enfileirar(p, s);
                return mudar(p, r, EST_PRONTO, T1_ADMITIR, ACAO_ALOCAR_E_ENFILEIRAR);
            }
            p->admissoes_adiadas++;
            return mudar(p, r, EST_NOVO, T2_ADIAR, ACAO_ADIAR_ADMISSAO);
        default:
            break;
        }
        break;

    case EST_PRONTO:
        switch (e) {
        case EV_DESPACHAR:
            carregar_contexto(p);
            return mudar(p, r, EST_EXECUTANDO, T3_DESPACHAR, ACAO_CARREGAR_CONTEXTO);
        case EV_SINAL_KILL:
            liberar(p, s, CODIGO_SAIDA_SIGKILL);
            return mudar(p, r, EST_TERMINADO, T8_MATAR_PRONTO, ACAO_ABORTAR);
        default:
            break;
        }
        break;

    case EST_EXECUTANDO:
        switch (e) {
        case EV_FIM_QUANTUM:
            salvar_e_enfileirar(p);
            return mudar(p, r, EST_PRONTO, T4_PREEMPTAR, ACAO_SALVAR_E_ENFILEIRAR);
        case EV_SOLICITAR_ES:
            salvar_e_bloquear(p);
            return mudar(p, r, EST_BLOQUEADO, T5_BLOQUEAR, ACAO_SALVAR_E_BLOQUEAR);
        case EV_FINALIZAR:
            liberar(p, s, CODIGO_SAIDA_NORMAL);
            return mudar(p, r, EST_TERMINADO, T7_FINALIZAR, ACAO_LIBERAR_RECURSOS);
        case EV_SINAL_KILL:
            liberar(p, s, CODIGO_SAIDA_SIGKILL);
            return mudar(p, r, EST_TERMINADO, T10_MATAR_EXECUTANDO, ACAO_ABORTAR);
        default:
            break;
        }
        break;

    case EST_BLOQUEADO:
        switch (e) {
        case EV_ES_CONCLUIDA:
            mover_para_prontos(p);
            return mudar(p, r, EST_PRONTO, T6_DESBLOQUEAR, ACAO_MOVER_PARA_PRONTOS);
        case EV_SINAL_KILL:
            liberar(p, s, CODIGO_SAIDA_SIGKILL);
            return mudar(p, r, EST_TERMINADO, T9_MATAR_BLOQUEADO, ACAO_ABORTAR);
        default:
            break;
        }
        break;

    case EST_TERMINADO:
        break;                        /* estado final: nenhum evento tem efeito */

    default:                          /* valor fora do enum: memória corrompida */
        liberar(p, s, CODIGO_SAIDA_SIGKILL);
        return mudar(p, r, EST_TERMINADO, T_RECUPERACAO, ACAO_ABORTAR);
    }

    return r;                         /* evento ignorado: nada foi alterado */
}

/*
 * Saída de Moore: configuração que o processo DEVE ter em cada estado.
 * Linhas: estado; colunas: memória alocada, fila de prontos, CPU, fila de E/S,
 * vivo (código de saída = -1).
 */
static const bool SAIDA_MOORE[NUM_ESTADOS][5] = {
    /*                 mem    prontos cpu    e/s    vivo  */
    [EST_NOVO]       = {false, false, false, false, true },
    [EST_PRONTO]     = {true,  true,  false, false, true },
    [EST_EXECUTANDO] = {true,  false, true,  false, true },
    [EST_BLOQUEADO]  = {true,  false, false, true,  true },
    [EST_TERMINADO]  = {false, false, false, false, false},
};

bool processo_invariante(const Processo *p)
{
    if ((unsigned)p->estado >= NUM_ESTADOS) {
        return false;
    }
    const bool *m = SAIDA_MOORE[p->estado];
    bool vivo = (p->codigo_saida == CODIGO_SAIDA_VIVO);
    /* operadores & (sem curto-circuito): a verificação não cria desvios */
    return (p->memoria_alocada == m[0]) & (p->na_fila_prontos == m[1]) &
           (p->na_cpu == m[2]) & (p->na_fila_es == m[3]) & (vivo == m[4]);
}

/* ======================================================================= */
/*  Nomes (para mensagens e rastreamento)                                  */
/* ======================================================================= */

static const char *const NOMES_ESTADO[NUM_ESTADOS] = {
    "NOVO", "PRONTO", "EXECUTANDO", "BLOQUEADO", "TERMINADO"
};
static const char *const NOMES_EVENTO[NUM_EVENTOS] = {
    "ADMITIR", "DESPACHAR", "FIM_QUANTUM", "SOLICITAR_ES",
    "ES_CONCLUIDA", "FINALIZAR", "SINAL_KILL"
};
static const char *const NOMES_TRANSICAO[NUM_TRANSICOES] = {
    "ignorada", "T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9", "T10",
    "recuperacao"
};
static const char *const NOMES_ACAO[NUM_ACOES] = {
    "nenhuma", "alocar_e_enfileirar", "adiar_admissao", "carregar_contexto",
    "salvar_e_enfileirar", "salvar_e_bloquear", "mover_para_prontos",
    "liberar_recursos", "abortar"
};

const char *nome_estado(Estado e)
{
    return (unsigned)e < NUM_ESTADOS ? NOMES_ESTADO[e] : "?";
}
const char *nome_evento(Evento e)
{
    return (unsigned)e < NUM_EVENTOS ? NOMES_EVENTO[e] : "?";
}
const char *nome_transicao(Transicao t)
{
    return (unsigned)t < NUM_TRANSICOES ? NOMES_TRANSICAO[t] : "?";
}
const char *nome_acao(Acao a)
{
    return (unsigned)a < NUM_ACOES ? NOMES_ACAO[a] : "?";
}
