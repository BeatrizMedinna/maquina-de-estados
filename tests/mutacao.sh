#!/usr/bin/env bash
# mutacao.sh — Análise de mutantes: mede a EFICÁCIA dos testes.
#
# Cada mutante é uma cópia de src/processo.c com UM defeito plausível
# injetado (destino errado, guarda errada, ação omitida, transição removida,
# caminho furtivo etc.). As três suítes são recompiladas contra o mutante e
# executadas separadamente. Um mutante é "morto" se pelo menos uma suíte
# falhar; um mutante "vivo" indicaria uma lacuna nos testes.
set -u
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
SUITES=(teste_estados teste_transicoes teste_caminhos)

# id | descrição | substituição perl (aplicada com -0p, texto inteiro)
MUTANTES=(
'M01|T1 leva a EXECUTANDO em vez de PRONTO|s/mudar\(p, r, EST_PRONTO, T1_ADMITIR/mudar(p, r, EST_EXECUTANDO, T1_ADMITIR/'
'M02|Guarda de admissao: <= trocado por <|s/p->memoria_kb <= s->memoria_livre_kb/p->memoria_kb < s->memoria_livre_kb/'
'M03|Guarda de admissao invertida (>)|s/p->memoria_kb <= s->memoria_livre_kb/p->memoria_kb > s->memoria_livre_kb/'
'M04|T3 leva a BLOQUEADO em vez de EXECUTANDO|s/EST_EXECUTANDO, T3_DESPACHAR/EST_BLOQUEADO, T3_DESPACHAR/'
'M05|T4 nao muda de estado (fica EXECUTANDO)|s/EST_PRONTO, T4_PREEMPTAR/EST_EXECUTANDO, T4_PREEMPTAR/'
'M06|T5 executa a acao de T4 (fila errada)|s/salvar_e_bloquear\(p\);/salvar_e_enfileirar(p);/'
'M07|T6 leva a EXECUTANDO em vez de PRONTO|s/EST_PRONTO, T6_DESBLOQUEAR/EST_EXECUTANDO, T6_DESBLOQUEAR/'
'M08|T7 grava codigo de saida 137 em vez de 0|s/liberar\(p, s, CODIGO_SAIDA_NORMAL\)/liberar(p, s, CODIGO_SAIDA_SIGKILL)/'
'M09|T8 removida (SIGKILL ignorado em PRONTO)|s/case EV_SINAL_KILL:(\s+liberar\(p, s, CODIGO_SAIDA_SIGKILL\);\s+return mudar\(p, r, EST_TERMINADO, T8_)/case NUM_EVENTOS:$1/'
'M10|T9 leva a PRONTO em vez de TERMINADO|s/EST_TERMINADO, T9_MATAR_BLOQUEADO/EST_PRONTO, T9_MATAR_BLOQUEADO/'
'M11|T10 identificada como T7|s/T10_MATAR_EXECUTANDO, ACAO_ABORTAR/T7_FINALIZAR, ACAO_ABORTAR/'
'M12|Despacho nao retira da fila de prontos|s/p->na_fila_prontos = false;\n(\s+p->na_cpu = true;)/$1/'
'M13|Termino nao devolve a memoria|s/s->memoria_livre_kb \+= p->memoria_kb;/s->memoria_livre_kb += 0;/'
'M14|Caminho furtivo: TERMINADO aceita ADMITIR|s/case EST_TERMINADO:\n(\s+)break;/case EST_TERMINADO:\n$1if (e == EV_ADMITIR) { p->codigo_saida = CODIGO_SAIDA_VIVO; return mudar(p, r, EST_NOVO, T_IGNORADA, ACAO_NENHUMA); }\n$1break;/'
'M15|Evento ignorado em EXECUTANDO altera contador|s/(case EV_SINAL_KILL:\s+liberar\(p, s, CODIGO_SAIDA_SIGKILL\);\s+return mudar\(p, r, EST_TERMINADO, T10_MATAR_EXECUTANDO, ACAO_ABORTAR\);\s+default:)/$1 p->despachos++;/'
'M16|Recuperacao de estado corrompido ausente|s/(default:\s+\/\* valor fora do enum[^\n]*\n)\s+liberar\(p, s, CODIGO_SAIDA_SIGKILL\);\s+return mudar\(p, r, EST_TERMINADO, T_RECUPERACAO, ACAO_ABORTAR\);/$1        break;/'
'M17|T2 nao registra a admissao adiada|s/\s+p->admissoes_adiadas\+\+;//'
'M18|Desbloqueio nao retira da fila de E/S|s/p->na_fila_es = false;\n(\s+p->na_fila_prontos = true;)/$1/'
)

printf '%-4s  %-46s  %-7s  %-10s  %-8s  %s\n' "Id" "Defeito injetado" "Estados" "Transicoes" "Caminhos" "Resultado"
printf '%s\n' "------------------------------------------------------------------------------------------------"
mortos=0; total=0
declare -A mata=( [teste_estados]=0 [teste_transicoes]=0 [teste_caminhos]=0 )
for m in "${MUTANTES[@]}"; do
    IFS='|' read -r id desc expr <<< "$m"
    total=$((total + 1))
    cp src/processo.c "$TMP/processo.c"
    perl -0pi -e "$expr" "$TMP/processo.c"
    if cmp -s src/processo.c "$TMP/processo.c"; then
        printf '%-4s  %-46s  %s\n' "$id" "$desc" "ERRO: substituicao nao aplicada"; exit 2
    fi
    linha=(); morto=0
    for s in "${SUITES[@]}"; do
        if ! $CC -std=c11 -O0 -w -Iinclude "tests/$s.c" "$TMP/processo.c" -o "$TMP/$s" 2>/dev/null; then
            linha+=("nao-comp"); continue
        fi
        if "$TMP/$s" > /dev/null 2>&1; then
            linha+=("passou")
        else
            linha+=("FALHOU"); morto=1; mata[$s]=$((mata[$s] + 1))
        fi
    done
    if [ $morto -eq 1 ]; then res="morto"; mortos=$((mortos + 1)); else res="VIVO"; fi
    printf '%-4s  %-46s  %-7s  %-10s  %-8s  %s\n' "$id" "$desc" "${linha[0]}" "${linha[1]}" "${linha[2]}" "$res"
done
printf '%s\n' "------------------------------------------------------------------------------------------------"
echo "Mutantes mortos por suite: estados=${mata[teste_estados]}  transicoes=${mata[teste_transicoes]}  caminhos=${mata[teste_caminhos]}  (de $total)"
echo "Escore de mutacao: $mortos/$total mortos"
[ $mortos -eq $total ]
