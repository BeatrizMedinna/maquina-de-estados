# Máquina de estados — ciclo de vida de um processo
# Uso:
#   make              compila a biblioteca, o simulador e as três suítes
#   make test         executa as suítes de estados, transições e caminhos
#   make cobertura    mede cobertura de linhas e desvios (gcov/gcovr)
#   make complexidade calcula V(G) das funções com lizard
#   make mutacao      análise de mutantes (eficácia dos testes)
#   make sanitizers   executa as suítes com AddressSanitizer e UBSan
#   make resultados   regenera todos os arquivos de resultados/
#   make clean

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
           -Wno-sign-conversion -Werror
CPPFLAGS = -Iinclude
BIN      = bin
SUITES   = teste_estados teste_transicoes teste_caminhos

.PHONY: all test cobertura complexidade mutacao sanitizers resultados clean

all: $(BIN)/simulador $(addprefix $(BIN)/,$(SUITES))

$(BIN):
	@mkdir -p $(BIN)

$(BIN)/simulador: src/simulador.c src/processo.c include/processo.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/simulador.c src/processo.c -o $@

$(BIN)/teste_%: tests/teste_%.c src/processo.c include/processo.h \
                tests/especificacao.h tests/teste.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/processo.c -o $@

test: $(addprefix $(BIN)/,$(SUITES))
	@status=0; for s in $(SUITES); do ./$(BIN)/$$s || status=1; echo; done; \
	if [ $$status -eq 0 ]; then echo "TODAS AS SUITES APROVADAS"; \
	else echo "HA SUITES REPROVADAS"; fi; exit $$status

# Cobertura: recompila sem otimização e com instrumentação do gcov.
cobertura: | $(BIN)
	@rm -rf $(BIN)/cov && mkdir -p $(BIN)/cov
	@for s in $(SUITES); do \
	  $(CC) $(CPPFLAGS) -std=c11 -O0 --coverage tests/$$s.c src/processo.c \
	        -o $(BIN)/cov/$$s || exit 1; done
	@cd $(BIN)/cov && for s in $(SUITES); do ./$$s > /dev/null || exit 1; done
	gcovr --root . --filter src/processo.c --txt-metric branch --object-directory $(BIN)/cov
	gcovr --root . --filter src/processo.c --object-directory $(BIN)/cov

sanitizers: | $(BIN)
	@for s in $(SUITES); do \
	  $(CC) $(CPPFLAGS) -std=c11 -g -O1 -fsanitize=address,undefined \
	        -fno-sanitize-recover=all tests/$$s.c src/processo.c -o $(BIN)/$$s-san \
	  && ./$(BIN)/$$s-san > /dev/null && echo "$$s: sem erros de memoria/UB" || exit 1; done

complexidade:
	lizard -C 20 src/processo.c

mutacao:
	bash tests/mutacao.sh

resultados: all
	@mkdir -p resultados
	$(MAKE) --no-print-directory test         > resultados/testes.txt 2>&1
	$(MAKE) --no-print-directory cobertura    > resultados/cobertura.txt 2>&1
	$(MAKE) --no-print-directory complexidade > resultados/complexidade.txt 2>&1
	$(MAKE) --no-print-directory mutacao      > resultados/mutacao.txt 2>&1
	$(MAKE) --no-print-directory sanitizers   > resultados/sanitizers.txt 2>&1
	./$(BIN)/simulador ADMITIR DESPACHAR SOLICITAR_ES ES_CONCLUIDA DESPACHAR FINALIZAR \
	    > resultados/simulacao_C5.txt
	@echo "resultados/ atualizado"

clean:
	rm -rf $(BIN) *.gcda *.gcno
