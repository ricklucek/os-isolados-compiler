#!/bin/sh
# POSIX sh; .gitattributes forca LF para evitar CRLF em bind mounts do Windows.

set -u

BIN=${1:-./macaronica}
ROOT=$(mktemp -d 2>/dev/null || mktemp -d -t macaronica-robustez)
OUT="$ROOT/stdout.txt"
ERR="$ROOT/stderr.txt"
PASS=0
FAIL=0

cleanup() {
    rm -rf "$ROOT"
}
trap cleanup EXIT HUP INT TERM

ok() {
    echo "[OK] $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "[FALHA] $1" >&2
    if [ -s "$OUT" ]; then
        echo "--- stdout ---" >&2
        cat "$OUT" >&2
    fi
    if [ -s "$ERR" ]; then
        echo "--- stderr ---" >&2
        cat "$ERR" >&2
    fi
    FAIL=$((FAIL + 1))
}

run_expect() {
    expected=$1
    description=$2
    shift 2
    : > "$OUT"
    : > "$ERR"
    status=0
    "$@" >"$OUT" 2>"$ERR" || status=$?
    if [ "$status" -eq "$expected" ]; then
        ok "$description (exit $status)"
    else
        fail "$description: esperado exit $expected, recebido $status"
    fi
}

run_not_signal() {
    description=$1
    shift
    : > "$OUT"
    : > "$ERR"
    status=0
    "$@" >"$OUT" 2>"$ERR" || status=$?
    if [ "$status" -lt 128 ]; then
        ok "$description (sem encerramento por sinal; exit $status)"
    else
        fail "$description: processo terminou por sinal/exit anormal $status"
    fi
}

assert_error_count() {
    pattern=$1
    minimum=$2
    description=$3
    count=$(grep -c "$pattern" "$ERR" 2>/dev/null || true)
    if [ "$count" -ge "$minimum" ]; then
        ok "$description ($count diagnosticos)"
    else
        fail "$description: esperados ao menos $minimum diagnosticos, encontrados $count"
    fi
}

VALID="$ROOT/valid.mac"
cat > "$VALID" <<'EOF'
inteira principal() @
[
    inteira x;
    x receba 1;
    respost x;
]
EOF

run_expect 0 "ajuda da CLI" "$BIN" --help
if grep -q "Codigos de saida" "$OUT"; then
    ok "ajuda documenta codigos de saida"
else
    fail "ajuda deveria documentar os codigos de saida"
fi

run_expect 1 "linha de comando sem arquivo" "$BIN"
run_expect 1 "opcao desconhecida" "$BIN" "$VALID" --opcao-inexistente
run_expect 1 "combinacao de modos incompatíveis" "$BIN" "$VALID" --lexer-only --ast
run_expect 3 "arquivo inexistente" "$BIN" "$ROOT/nao-existe.mac"

LEXICAL="$ROOT/multiplos-lexicos.mac"
cat > "$LEXICAL" <<'EOF'
inteira principal() @
[
    inteira x;
    x receba 1;
    $ % ?
    respost x;
]
EOF
run_expect 2 "multiplos erros lexicos" "$BIN" "$LEXICAL"
assert_error_count "\[ERRO LEXICO\]" 3 "lexer continua apos caracteres invalidos"

SYNTACTIC="$ROOT/multiplos-sintaticos.mac"
cat > "$SYNTACTIC" <<'EOF'
inteira principal() @
[
    inteira a
    inteira b
    a receba 1;
    b receba 2;
    respost 0;
]
EOF
run_expect 4 "entrada com erros sintaticos recuperaveis" "$BIN" "$SYNTACTIC" --parser-only
assert_error_count "\[ERRO SINTATICO\]" 2 "parser reporta multiplos erros na mesma execucao"

SEMANTIC="$ROOT/multiplos-semanticos.mac"
cat > "$SEMANTIC" <<'EOF'
outrafuncao inteira soma(inteira a, inteira b) @
[
    respost a + b;
]

inteira principal() @
[
    inteira x;
    inteira x;
    y receba 1;
    bool ativo;
    x receba ativo;
    soma(1);
    respost ativo;
]
EOF
run_expect 6 "entrada com multiplos erros semanticos" "$BIN" "$SEMANTIC"
assert_error_count "\[ERRO SEMANTICO\]" 4 "semantico acumula diagnosticos independentes"

LONG="$ROOT/identificador-longo.mac"
long_name=$(awk 'BEGIN { printf "v"; for (i = 0; i < 4095; ++i) printf "a" }')
{
    echo "inteira principal() @"
    echo "["
    printf "    inteira %s;\n" "$long_name"
    printf "    %s receba 1;\n" "$long_name"
    printf "    respost %s;\n" "$long_name"
    echo "]"
} > "$LONG"
run_expect 0 "identificador longo sem overflow de buffer fixo" "$BIN" "$LONG"

MANY="$ROOT/muitos-simbolos.mac"
{
    echo "inteira principal() @"
    echo "["
    i=1
    while [ "$i" -le 256 ]; do
        echo "    inteira v$i;"
        i=$((i + 1))
    done
    echo "    respost 0;"
    echo "]"
} > "$MANY"
run_expect 0 "crescimento dinamico da tabela com 256 simbolos" "$BIN" "$MANY"

DEEP="$ROOT/aninhamento.mac"
{
    echo "inteira principal() @"
    echo "["
    echo "    inteira x;"
    echo "    x receba 1;"
    i=1
    while [ "$i" -le 40 ]; do
        echo "    cond ( x > 0 ) edai"
        echo "    ["
        i=$((i + 1))
    done
    echo "        x receba x + 1;"
    i=1
    while [ "$i" -le 40 ]; do
        echo "    ]"
        i=$((i + 1))
    done
    echo "    respost x;"
    echo "]"
} > "$DEEP"
run_expect 0 "quarenta niveis de blocos aninhados" "$BIN" "$DEEP"

BYTES="$ROOT/bytes-invalidos.mac"
printf '\001\002\003' > "$BYTES"
run_expect 2 "bytes de controle sao rejeitados sem crash" "$BIN" "$BYTES"

EMPTY="$ROOT/vazio.mac"
: > "$EMPTY"
run_not_signal "arquivo vazio nao causa crash ou loop" "$BIN" "$EMPTY"

TRUNCATED="$ROOT/truncado.mac"
cat > "$TRUNCATED" <<'EOF'
inteira principal() @
[
    inteira x;
    cond ( x > 0 ) edai
    [
        x receba 1;
EOF
run_not_signal "arquivo truncado finaliza de forma controlada" "$BIN" "$TRUNCATED"

printf "\nRobustez: %d teste(s) OK, %d falha(s).\n" "$PASS" "$FAIL"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
