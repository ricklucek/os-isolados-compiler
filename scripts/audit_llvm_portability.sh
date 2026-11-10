#!/bin/sh
set -eu

IR=${1:-}
if [ -z "$IR" ] || [ ! -f "$IR" ]; then
    echo "Uso: scripts/audit_llvm_portability.sh arquivo.ll" >&2
    exit 1
fi

fail() {
    echo "FALHA DE PORTABILIDADE: $1" >&2
    exit 1
}

if grep -Eq '^target[[:space:]]+(triple|datalayout)' "$IR"; then
    fail "IR contem target triple/datalayout"
fi

if grep -Eq '@(printf|scanf|fprintf|fscanf|puts|gets)\b' "$IR"; then
    fail "IR chama libc diretamente em vez da ABI zl_*"
fi

if grep -Eq '^declare .*\.\.\.' "$IR"; then
    fail "IR contem declaracao variadica"
fi

if grep -Eq 'getelementptr .*i64 ' "$IR"; then
    fail "GEP usa indice i64; o contrato portavel exige i32"
fi

if grep -E '^(define|declare) ' "$IR" | grep -vq ' nounwind'; then
    fail "funcao gerada/declarada sem nounwind"
fi

echo "Auditoria de portabilidade LLVM: OK."
