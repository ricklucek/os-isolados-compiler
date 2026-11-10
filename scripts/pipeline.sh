#!/bin/sh
set -eu

INPUT=${1:-}
OUTDIR=${2:-build/mlir}
MLIR_OPT=${MLIR_OPT:-mlir-opt-18}
MLIR_TRANSLATE=${MLIR_TRANSLATE:-mlir-translate-18}

if [ -z "$INPUT" ] || [ ! -f "$INPUT" ]; then
    echo "Uso: scripts/pipeline.sh entrada.mlir [diretorio_saida]" >&2
    exit 1
fi

mkdir -p "$OUTDIR"

# Nível 1: preserva affine/scf de alto nível, removendo redundâncias locais.
"$MLIR_OPT" "$INPUT"     --canonicalize     --cse     -o "$OUTDIR/01_canonical.mlir"

# Nível 2: transforma affine.for/load/store em operações SCF/memref.
"$MLIR_OPT" "$OUTDIR/01_canonical.mlir"     --lower-affine     --canonicalize     -o "$OUTDIR/02_scf.mlir"

# Nível 3: rebaixa SCF/arith/memref/func para o dialeto LLVM do MLIR.
"$MLIR_OPT" "$OUTDIR/02_scf.mlir"     --convert-scf-to-cf     --convert-arith-to-llvm     --finalize-memref-to-llvm     --convert-func-to-llvm     --convert-cf-to-llvm     --reconcile-unrealized-casts     -o "$OUTDIR/03_llvm.mlir"

# Saída final: traduz o dialeto LLVM do MLIR para LLVM IR textual.
"$MLIR_TRANSLATE" --mlir-to-llvmir     "$OUTDIR/03_llvm.mlir"     -o "$OUTDIR/04_kernel.ll"

echo "Pipeline MLIR concluido:"
echo "  $OUTDIR/01_canonical.mlir"
echo "  $OUTDIR/02_scf.mlir"
echo "  $OUTDIR/03_llvm.mlir"
echo "  $OUTDIR/04_kernel.ll"
