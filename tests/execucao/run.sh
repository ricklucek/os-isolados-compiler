#!/bin/sh
set -u

BIN=${1:-./macaronica}
LLVM_AS=${LLVM_AS:-llvm-as-18}
LLVM_OPT=${LLVM_OPT:-opt-18}
LLVM_LLI=${LLVM_LLI:-lli-18}
CLANG_LLVM=${CLANG_LLVM:-clang-18}

IR=.llvm-test.ll
BC=.llvm-test.bc
PROG=.llvm-test-prog
OUT=.llvm-test.out
ERR=.llvm-test.err
EXPECTED=.llvm-test.expected
INVALID=.llvm-invalid.ll
UNSUPPORTED=.llvm-unsupported.ll
M2R=.llvm-mem2reg.ll
AVR_OBJ=.llvm-avr.o

cleanup() {
    rm -f "$IR" "$BC" "$PROG" "$OUT" "$ERR" "$EXPECTED" "$INVALID" "$UNSUPPORTED" "$M2R" "$AVR_OBJ"
}
trap cleanup EXIT INT TERM
cleanup

fail() {
    echo "FALHA LLVM: $1" >&2
    exit 1
}

"$BIN" tests/execucao/01_retorno_basico.mcn -o "$IR" >/dev/null ||
    fail "nao gerou retorno basico"

if grep -Eq '^target[[:space:]]+triple' "$IR"; then
    fail "IR contem target triple"
fi
if grep -Eq '^target[[:space:]]+datalayout' "$IR"; then
    fail "IR contem target datalayout"
fi
grep -q 'define i32 @zl_principal() nounwind' "$IR" ||
    fail "principal nao virou zl_principal nounwind"

"$LLVM_AS" "$IR" -o "$BC" || fail "llvm-as rejeitou a IR"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "opt -passes=verify rejeitou a IR"

status=0
"$LLVM_LLI" --entry-function=zl_principal "$IR" >/dev/null || status=$?
[ "$status" -eq 42 ] ||
    fail "lli esperava codigo 42 e recebeu $status"

"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou IR + runtime POSIX"

status=0
"./$PROG" >/dev/null || status=$?
[ "$status" -eq 42 ] ||
    fail "executavel POSIX esperava codigo 42 e recebeu $status"

"$BIN" tests/execucao/02_variaveis_expressoes.mcn -o "$IR" >/dev/null ||
    fail "nao gerou programa de expressoes"
"$LLVM_AS" "$IR" -o /dev/null ||
    fail "llvm-as rejeitou programa de expressoes"

status=0
"$LLVM_LLI" --entry-function=zl_principal "$IR" >/dev/null || status=$?
[ "$status" -eq 42 ] ||
    fail "programa de expressoes esperava codigo 42 e recebeu $status"

"$BIN" tests/execucao/03_saida_tipos.mcn -o "$IR" >/dev/null ||
    fail "nao gerou programa de saida"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou programa de saida"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou programa de saida"
"./$PROG" > "$OUT" || fail "programa de saida retornou erro"
tr -d '\r' < tests/execucao/03_saida_tipos.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida do runtime POSIX divergiu do esperado"

if "$BIN" tests/invalidos/semantic_01_nao_declarada.mac     -o "$INVALID" >/dev/null 2>&1; then
    fail "fonte semanticamente invalido foi aceito com -o"
fi
[ ! -e "$INVALID" ] ||
    fail "fonte invalido deixou arquivo LLVM"

"$BIN" tests/execucao/04_condicional.mcn -o "$IR" >/dev/null ||
    fail "nao gerou condicional"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou condicional"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou condicional"
status=0
"./$PROG" > "$OUT" || status=$?
[ "$status" -eq 2 ] ||
    fail "condicional esperava codigo 2 e recebeu $status"
tr -d '\r' < tests/execucao/04_condicional.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida do condicional divergiu"

"$BIN" tests/execucao/05_lacos.mcn -o "$IR" >/dev/null ||
    fail "nao gerou lacos"
"$LLVM_AS" "$IR" -o /dev/null ||
    fail "llvm-as rejeitou lacos"
grep -q 'while.cond' "$IR" || fail "IR nao contem while"
grep -q 'for.cond' "$IR" || fail "IR nao contem for"
"$LLVM_OPT" -S -passes=mem2reg "$IR" -o "$M2R" ||
    fail "mem2reg falhou no programa com lacos"
grep -q 'phi i32' "$M2R" ||
    fail "mem2reg nao produziu phi i32 para variaveis do laco"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou lacos"
status=0
"./$PROG" > "$OUT" || status=$?
[ "$status" -eq 10 ] ||
    fail "lacos esperavam codigo 10 e receberam $status"
tr -d '\r' < tests/execucao/05_lacos.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida dos lacos divergiu"

"$BIN" tests/execucao/06_curto_circuito.mcn -o "$IR" >/dev/null ||
    fail "nao gerou curto-circuito"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou curto-circuito"
grep -q 'phi i1' "$IR" ||
    fail "E/OU nao produziram merge SSA com phi i1"
grep -q 'and.rhs' "$IR" ||
    fail "IR nao contem bloco do lado direito de E"
grep -q 'or.rhs' "$IR" ||
    fail "IR nao contem bloco do lado direito de OU"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou curto-circuito"
"./$PROG" > "$OUT" || fail "programa de curto-circuito retornou erro"
tr -d '\r' < tests/execucao/06_curto_circuito.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida do curto-circuito divergiu"

"$BIN" tests/execucao/07_escopo_sombreamento.mcn -o "$IR" >/dev/null ||
    fail "nao gerou sombreamento em bloco interno"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou sombreamento"
grep -q '%x.addr = alloca i32' "$IR" ||
    fail "armazenamento externo de x ausente"
grep -q '%x.1.addr = alloca i32' "$IR" ||
    fail "armazenamento sombreado de x nao recebeu nome unico"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou sombreamento"
"./$PROG" > "$OUT" || fail "programa de sombreamento retornou erro"
tr -d '\r' < tests/execucao/07_escopo_sombreamento.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida do sombreamento divergiu"

"$BIN" tests/execucao/08_funcoes_chamadas.mcn -o "$IR" >/dev/null ||
    fail "nao gerou funcoes e chamadas"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou funcoes e chamadas"
grep -q 'define i32 @quadrado(i32 %arg0) nounwind' "$IR" ||
    fail "assinatura LLVM de quadrado ausente"
grep -q 'call i32 @quadrado(i32 5) nounwind' "$IR" ||
    fail "call de quadrado ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou funcoes e chamadas"
"./$PROG" > "$OUT" || fail "programa de funcoes retornou erro"
tr -d '\r' < tests/execucao/08_funcoes_chamadas.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de funcoes divergiu"

"$BIN" tests/execucao/09_recursao_forward.mcn -o "$IR" >/dev/null ||
    fail "nao gerou recursao/forward call"
"$LLVM_AS" "$IR" -o /dev/null ||
    fail "llvm-as rejeitou recursao"
grep -q 'call i32 @fatorial' "$IR" ||
    fail "chamada recursiva de fatorial ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou recursao"
"./$PROG" > "$OUT" || fail "programa recursivo retornou erro"
tr -d '\r' < tests/execucao/09_recursao_forward.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida da recursao divergiu"

"$BIN" tests/execucao/10_funcao_vazio.mcn -o "$IR" >/dev/null ||
    fail "nao gerou funcao vazio"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou funcao vazio"
grep -q 'define void @mostrar(i32 %arg0) nounwind' "$IR" ||
    fail "assinatura void de mostrar ausente"
grep -q 'call void @mostrar(i32 7) nounwind' "$IR" ||
    fail "call void de mostrar ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou funcao vazio"
"./$PROG" > "$OUT" || fail "funcao vazio retornou erro"
tr -d '\r' < tests/execucao/10_funcao_vazio.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de funcao vazio divergiu"

"$BIN" tests/execucao/11_potencia_raiz.mcn -o "$IR" >/dev/null ||
    fail "nao gerou potencia/raiz"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou potencia/raiz"
grep -q '@zl_pow_i32' "$IR" || fail "helper de potencia inteira ausente"
grep -q '@zl_pow_flut' "$IR" || fail "helper de potencia real ausente"
grep -q '@zl_raiz_flut' "$IR" || fail "helper de raiz ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou potencia/raiz"
"./$PROG" > "$OUT" || fail "programa de potencia/raiz retornou erro"
tr -d '\r' < tests/execucao/11_potencia_raiz.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de potencia/raiz divergiu"

"$BIN" tests/execucao/12_curto_circuito_chamada.mcn -o "$IR" >/dev/null ||
    fail "nao gerou curto-circuito com chamada"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou curto-circuito com chamada"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou curto-circuito com chamada"
"./$PROG" > "$OUT" || fail "curto-circuito com chamada retornou erro"
tr -d '\r' < tests/execucao/12_curto_circuito_chamada.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "lado direito do curto-circuito foi avaliado indevidamente"

"$BIN" tests/execucao/14_forward_entre_funcoes.mcn -o "$IR" >/dev/null ||
    fail "nao gerou chamada entre funcoes com definicao posterior"
"$LLVM_AS" "$IR" -o /dev/null ||
    fail "llvm-as rejeitou forward call entre funcoes"
grep -q 'call i32 @dobrar' "$IR" ||
    fail "forward call para dobrar ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou forward call entre funcoes"
"./$PROG" > "$OUT" || fail "forward call entre funcoes retornou erro"
tr -d '\r' < tests/execucao/14_forward_entre_funcoes.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de forward call divergiu"

"$BIN" tests/execucao/13_erro_expoente_negativo.mcn -o "$IR" >/dev/null ||
    fail "nao gerou caso de erro de expoente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou erro de expoente"
status=0
"./$PROG" >/dev/null 2>&1 || status=$?
[ "$status" -eq 104 ] ||
    fail "expoente inteiro negativo esperava exit 104 e recebeu $status"

"$BIN" tests/execucao/15_vetores.mcn -o "$IR" >/dev/null ||
    fail "nao gerou vetores"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou vetores"
grep -Fq 'getelementptr [4 x i32]' "$IR" ||
    fail "GEP de vetor inteiro ausente"
grep -Fq 'getelementptr [2 x ptr]' "$IR" ||
    fail "GEP de vetor palavra ausente"
grep -Fq 'call void @zl_erro_execucao(i32 1' "$IR" ||
    fail "bounds check de vetor nao chama erro de execucao"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou vetores"
status=0
"./$PROG" > "$OUT" || status=$?
[ "$status" -eq 12 ] ||
    fail "programa de vetores esperava codigo 12 e recebeu $status"
tr -d '\r' < tests/execucao/15_vetores.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de vetores divergiu"

"$BIN" tests/execucao/16_registro.mcn -o "$IR" >/dev/null ||
    fail "nao gerou registro"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou registro"
grep -Fq '%struct.Pessoa = type { ptr, i32, float }' "$IR" ||
    fail "layout LLVM de Pessoa ausente/incorreto"
grep -Fq 'getelementptr %struct.Pessoa' "$IR" ||
    fail "GEP de campo de registro ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou registro"
status=0
"./$PROG" > "$OUT" || status=$?
[ "$status" -eq 30 ] ||
    fail "registro esperava codigo 30 e recebeu $status"
tr -d '\r' < tests/execucao/16_registro.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de registro divergiu"

"$BIN" tests/execucao/17_agregados_globais.mcn -o "$IR" >/dev/null ||
    fail "nao gerou agregados globais"
"$LLVM_AS" "$IR" -o /dev/null ||
    fail "llvm-as rejeitou agregados globais"
grep -Fq '@valores = global [3 x i32] zeroinitializer' "$IR" ||
    fail "vetor global ausente"
grep -Fq '@config = global %struct.Config zeroinitializer' "$IR" ||
    fail "registro global ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou agregados globais"
status=0
"./$PROG" > "$OUT" || status=$?
[ "$status" -eq 13 ] ||
    fail "agregados globais esperavam codigo 13 e receberam $status"
tr -d '\r' < tests/execucao/17_agregados_globais.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de agregados globais divergiu"

"$BIN" tests/execucao/18_erro_indice_dinamico.mcn -o "$IR" >/dev/null ||
    fail "nao gerou caso de bounds dinamico"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou bounds dinamico"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou bounds dinamico"
status=0
"./$PROG" > "$OUT" 2> "$ERR" || status=$?
[ "$status" -eq 101 ] ||
    fail "indice fora dos limites esperava exit 101 e recebeu $status"
grep -Fq 'codigo=1, linha=7' "$ERR" ||
    fail "erro de indice nao informou codigo/linha esperados"

"$BIN" tests/execucao/19_aritmetica_precedencia.mcn -o "$IR" >/dev/null ||
    fail "nao gerou aritmetica de precedencia"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou aritmetica de precedencia"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou aritmetica de precedencia"
"./$PROG" > "$OUT" || fail "aritmetica de precedencia retornou erro"
tr -d '\r' < tests/execucao/19_aritmetica_precedencia.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida da precedencia divergiu"

"$BIN" tests/execucao/20_condicional_aninhada.mcn -o "$IR" >/dev/null ||
    fail "nao gerou condicional aninhada"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou condicional aninhada"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou condicional aninhada"
"./$PROG" > "$OUT" || fail "condicional aninhada retornou erro"
tr -d '\r' < tests/execucao/20_condicional_aninhada.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida da condicional aninhada divergiu"

"$BIN" tests/execucao/21_entrada_saida.mcn -o "$IR" >/dev/null ||
    fail "nao gerou entrada/saida"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou entrada/saida"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou entrada/saida"
"./$PROG" < tests/execucao/21_entrada_saida.entrada > "$OUT" ||
    fail "programa de entrada/saida retornou erro"
tr -d '\r' < tests/execucao/21_entrada_saida.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de entrada/saida divergiu"

"$BIN" tests/execucao/22_divisao_real_inteira.mcn -o "$IR" >/dev/null ||
    fail "nao gerou diferenca entre / e //"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou / e //"
grep -q 'sdiv i32' "$IR" || fail "IR nao contem sdiv para //"
grep -q 'fdiv float' "$IR" || fail "IR nao contem fdiv para /"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou / e //"
"./$PROG" > "$OUT" || fail "programa de divisoes retornou erro"
tr -d '\r' < tests/execucao/22_divisao_real_inteira.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de / e // divergiu"

"$BIN" tests/execucao/23_tipos_mistos.mcn -o "$IR" >/dev/null ||
    fail "nao gerou tipos mistos"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou tipos mistos"
grep -q 'sitofp i32' "$IR" || fail "promocao inteira -> real ausente"
grep -q 'fpext float' "$IR" || fail "promocao flut -> duplocarpado ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou tipos mistos"
"./$PROG" > "$OUT" || fail "tipos mistos retornaram erro"
tr -d '\r' < tests/execucao/23_tipos_mistos.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de tipos mistos divergiu"

"$BIN" tests/execucao/24_div_zero_dinamica_inteira.mcn -o "$IR" >/dev/null ||
    fail "nao gerou divisao inteira dinamica"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou guarda de divisao inteira"
grep -q 'div.zero' "$IR" || fail "bloco div.zero ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou divisao inteira dinamica"
status=0
"./$PROG" > "$OUT" 2> "$ERR" || status=$?
[ "$status" -eq 102 ] ||
    fail "divisao inteira por zero esperava exit 102 e recebeu $status"
grep -Fq 'codigo=2, linha=7' "$ERR" ||
    fail "divisao inteira por zero nao informou codigo/linha esperados"

"$BIN" tests/execucao/25_div_zero_dinamica_real.mcn -o "$IR" >/dev/null ||
    fail "nao gerou divisao real dinamica"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou guarda de divisao real"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou divisao real dinamica"
status=0
"./$PROG" > "$OUT" 2> "$ERR" || status=$?
[ "$status" -eq 102 ] ||
    fail "divisao real por zero esperava exit 102 e recebeu $status"
grep -Fq 'codigo=2, linha=7' "$ERR" ||
    fail "divisao real por zero nao informou codigo/linha esperados"

"$BIN" tests/execucao/26_sdiv_overflow.mcn -o "$IR" >/dev/null ||
    fail "nao gerou protecao de sdiv extremo"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou protecao de sdiv extremo"
grep -q 'sdiv.overflow' "$IR" || fail "bloco sdiv.overflow ausente"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou sdiv extremo"
status=0
"./$PROG" > "$OUT" 2> "$ERR" || status=$?
[ "$status" -eq 106 ] ||
    fail "INT32_MIN // -1 esperava exit 106 e recebeu $status"
grep -Fq 'codigo=6, linha=7' "$ERR" ||
    fail "overflow de sdiv nao informou codigo/linha esperados"

"$BIN" tests/execucao/27_int32_minimo.mcn -o "$IR" >/dev/null ||
    fail "nao aceitou INT32_MIN valido"
"$LLVM_AS" "$IR" -o /dev/null || fail "llvm-as rejeitou INT32_MIN"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module \
    "$IR" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROG" ||
    fail "clang nao ligou INT32_MIN"
"./$PROG" > "$OUT" || fail "programa INT32_MIN retornou erro"
tr -d '\r' < tests/execucao/27_int32_minimo.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" || fail "saida de INT32_MIN divergiu"

status=0
"$BIN" tests/execucao/28_inteira_fora_i32.mcn -o "$UNSUPPORTED"     >/dev/null 2> "$ERR" || status=$?
[ "$status" -eq 8 ] ||
    fail "literal fora de i32 deveria retornar exit 8, recebeu $status"
[ ! -e "$UNSUPPORTED" ] ||
    fail "literal fora de i32 deixou LLVM parcial"
grep -Fq 'nao cabe em i32' "$ERR" ||
    fail "diagnostico de literal fora de i32 ausente"

status=0
"$BIN" tests/execucao/29_div_zero_constante.mcn -o "$UNSUPPORTED"     >/dev/null 2> "$ERR" || status=$?
[ "$status" -eq 8 ] ||
    fail "divisao por zero estatica deveria retornar exit 8, recebeu $status"
[ ! -e "$UNSUPPORTED" ] ||
    fail "divisao por zero estatica deixou LLVM parcial"
grep -Fq 'divisao por zero comprovada estaticamente' "$ERR" ||
    fail "diagnostico estatico de divisao por zero ausente"

status=0
"$BIN" tests/execucao/99_ainda_nao_suportado.mcn     -o "$UNSUPPORTED" >/dev/null 2>&1 || status=$?
[ "$status" -eq 8 ] ||
    fail "principal vazio fora do contrato deveria retornar exit 8, recebeu $status"
[ ! -e "$UNSUPPORTED" ] ||
    fail "falha controlada de codegen deixou LLVM parcial"

echo "PjBL 3: IR valida, bateria obrigatoria e casos de borda aprovados."
