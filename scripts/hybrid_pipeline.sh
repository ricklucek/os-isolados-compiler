#!/bin/sh
set -eu

SOURCE=${1:-}
OUTDIR=${2:-build/hybrid}
BIN=${3:-./macaronica}

LLVM_AS=${LLVM_AS:-llvm-as-18}
LLVM_OPT=${LLVM_OPT:-opt-18}
LLVM_LINK=${LLVM_LINK:-llvm-link-18}
MLIR_OPT=${MLIR_OPT:-mlir-opt-18}
MLIR_TRANSLATE=${MLIR_TRANSLATE:-mlir-translate-18}
CLANG_LLVM=${CLANG_LLVM:-clang-18}

if [ -z "$SOURCE" ] || [ ! -f "$SOURCE" ]; then
    echo "Uso: scripts/hybrid_pipeline.sh programa.mcn [outdir] [compilador]" >&2
    exit 1
fi

mkdir -p "$OUTDIR/kernels"

SCALAR="$OUTDIR/00_scalar.ll"
MLIR="$OUTDIR/00_kernels.mlir"
KERNEL_LL="$OUTDIR/kernels/04_kernel.ll"
LINKED="$OUTDIR/05_linked.ll"
PROGRAM="$OUTDIR/programa"

"$BIN" "$SOURCE" -o "$SCALAR" >/dev/null
"$BIN" "$SOURCE" --emit-mlir "$MLIR" >/dev/null

MLIR_OPT="$MLIR_OPT" MLIR_TRANSLATE="$MLIR_TRANSLATE"     sh scripts/pipeline.sh "$MLIR" "$OUTDIR/kernels" >/dev/null

"$LLVM_LINK" -S "$SCALAR" "$KERNEL_LL" -o "$LINKED"
"$LLVM_AS" "$LINKED" -o /dev/null
"$LLVM_OPT" -passes=verify "$LINKED" -disable-output

"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$LINKED" runtime/zl_runtime_posix.c -Iruntime -lm -o "$PROGRAM"

echo "Pipeline hibrido concluido:"
echo "  $SCALAR"
echo "  $MLIR"
echo "  $KERNEL_LL"
echo "  $LINKED"
echo "  $PROGRAM"
