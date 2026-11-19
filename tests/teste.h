/*
 * teste.h — Mini-arcabouço de testes automatizados (sem dependências).
 *
 * Cada caso de teste é uma função void. VERIFICA() registra uma verificação;
 * se ela falhar, o caso é marcado como reprovado e a mensagem indica arquivo,
 * linha e expressão. Ao final, a suíte devolve código de saída 0 apenas se
 * todos os casos forem aprovados — é isso que o "make test" e a integração
 * contínua usam para decidir se o código está correto.
 */
#ifndef TESTE_H
#define TESTE_H

#include <stdio.h>
#include <string.h>

static int g_verificacoes = 0;     /* total de verificações executadas   */
static int g_falhas_caso = 0;      /* falhas no caso corrente            */
static int g_casos = 0, g_aprovados = 0;

#define VERIFICA(cond)                                                     \
    do {                                                                   \
        g_verificacoes++;                                                  \
        if (!(cond)) {                                                     \
            g_falhas_caso++;                                               \
            fprintf(stderr, "    FALHA %s:%d: %s\n", __FILE__, __LINE__,   \
                    #cond);                                                \
        }                                                                  \
    } while (0)

#define VERIFICA_IGUAL(obtido, esperado)                                   \
    do {                                                                   \
        long long o_ = (long long)(obtido), e_ = (long long)(esperado);    \
        g_verificacoes++;                                                  \
        if (o_ != e_) {                                                    \
            g_falhas_caso++;                                               \
            fprintf(stderr, "    FALHA %s:%d: %s == %lld, esperado %lld\n",\
                    __FILE__, __LINE__, #obtido, o_, e_);                  \
        }                                                                  \
    } while (0)

static inline void executar_caso(const char *id, const char *descricao,
                          void (*caso)(void))
{
    int antes = g_verificacoes;
    g_falhas_caso = 0;
    g_casos++;
    caso();
    if (g_falhas_caso == 0) {
        g_aprovados++;
        printf("[ OK ]  %-6s %-62s (%d verificacoes)\n", id, descricao,
               g_verificacoes - antes);
    } else {
        printf("[FALHOU] %-6s %-61s (%d falhas)\n", id, descricao,
               g_falhas_caso);
    }
}

static inline int resumo(const char *suite)
{
    printf("---------------------------------------------------------------"
           "-------------------\n");
    printf("%s: %d casos, %d aprovados, %d reprovados, %d verificacoes\n",
           suite, g_casos, g_aprovados, g_casos - g_aprovados, g_verificacoes);
    return g_aprovados == g_casos ? 0 : 1;
}

#endif /* TESTE_H */
