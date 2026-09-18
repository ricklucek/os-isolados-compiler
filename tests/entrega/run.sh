#!/bin/sh
# POSIX sh; .gitattributes forca LF.

set -u

BIN=${1:-./macaronica}
PASS=0
FAIL=0

expect_status() {
    expected=$1
    file=$2
    description=$3
    status=0
    "$BIN" "$file" >/dev/null 2>&1 || status=$?
    if [ "$status" -eq "$expected" ]; then
        echo "[OK] $description (exit $status)"
        PASS=$((PASS + 1))
    else
        echo "[FALHA] $description: esperado exit $expected, recebido $status" >&2
        FAIL=$((FAIL + 1))
    fi
}

for file in tests/entrega/validos/*.mac; do
    expect_status 0 "$file" "programa valido: $file"
done

expect_status 2 tests/entrega/invalidos/01_erro_lexico.mac "erro lexico obrigatorio"
expect_status 4 tests/entrega/invalidos/02_erro_sintatico.mac "erro sintatico obrigatorio"
expect_status 6 tests/entrega/invalidos/03_uso_sem_declaracao.mac "uso sem declaracao"
expect_status 6 tests/entrega/invalidos/04_incompatibilidade_tipo.mac "incompatibilidade de tipo"
expect_status 6 tests/entrega/invalidos/05_assinatura_funcao.mac "assinatura de funcao"

printf "\nEntrega: %d teste(s) OK, %d falha(s).\n" "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
