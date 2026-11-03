#!/bin/sh
set -u

SOURCE=${1:-}
BIN=${2:-./macaronica}
LLVM_AS=${LLVM_AS:-llvm-as-18}
LLVM_OPT=${LLVM_OPT:-opt-18}
CLANG_LLVM=${CLANG_LLVM:-clang-18}

if [ -z "$SOURCE" ]; then
    echo "Uso: scripts/run_llvm.sh programa.mcn [compilador]" >&2
    exit 1
fi

IR=.llvm-run.ll
PROG=.llvm-run

cleanup() {
    rm -f "$IR" "$PROG"
}
trap cleanup EXIT INT TERM
cleanup

"$BIN" "$SOURCE" -o "$IR" || exit $?
"$LLVM_AS" "$IR" -o /dev/null || exit $?
"$LLVM_OPT" -passes=verify "$IR" -disable-output || exit $?
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic     "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" || exit $?

status=0
"./$PROG" || status=$?
echo "codigo de saida: $status"
exit 0
