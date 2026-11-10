#!/bin/sh
set -eu

CONFIG=${1:-}
INPUT=${2:-}
OUTDIR=${3:-}

MLIR_OPT=${MLIR_OPT:-mlir-opt-18}
MLIR_TRANSLATE=${MLIR_TRANSLATE:-mlir-translate-18}

if [ -z "$CONFIG" ] || [ -z "$INPUT" ] || [ -z "$OUTDIR" ]; then
    echo "Uso: scripts/benchmark_variant.sh C0|C1|C1F|C2|C3 entrada.mlir outdir" >&2
    exit 1
fi

mkdir -p "$OUTDIR"

lower_standard() {
    in="$1"
    llvm_mlir="$2"
    "$MLIR_OPT" "$in"         --lower-affine         --convert-vector-to-scf         --convert-scf-to-cf         --convert-vector-to-llvm         --convert-arith-to-llvm         --finalize-memref-to-llvm         --convert-func-to-llvm         --convert-cf-to-llvm         --reconcile-unrealized-casts         -o "$llvm_mlir"
}

case "$CONFIG" in
    C0|C1|C1F)
        "$MLIR_OPT" "$INPUT" --canonicalize --cse             -o "$OUTDIR/01_prepared.mlir"
        lower_standard "$OUTDIR/01_prepared.mlir" "$OUTDIR/03_llvm.mlir"
        "$MLIR_TRANSLATE" --mlir-to-llvmir             "$OUTDIR/03_llvm.mlir" -o "$OUTDIR/kernel.base.ll"

        if [ "$CONFIG" = "C1F" ]; then
            # Experimento de reassociacao: permite fast-math no LLVM IR
            # gerado a partir do programa Macaronica flut.
            sed -E                 -e 's/fadd (float|double)/fadd fast \1/g'                 -e 's/fmul (float|double)/fmul fast \1/g'                 "$OUTDIR/kernel.base.ll" > "$OUTDIR/kernel.ll"
        else
            cp "$OUTDIR/kernel.base.ll" "$OUTDIR/kernel.ll"
        fi
        ;;
    C2)
        "$MLIR_OPT" "$INPUT"             --affine-super-vectorize="virtual-vector-size=8 vectorize-reductions=true"             --canonicalize --cse             -o "$OUTDIR/01_vector.mlir"
        lower_standard "$OUTDIR/01_vector.mlir" "$OUTDIR/03_llvm.mlir"
        "$MLIR_TRANSLATE" --mlir-to-llvmir             "$OUTDIR/03_llvm.mlir" -o "$OUTDIR/kernel.ll"
        ;;
    C3)
        # Paraleliza somente loops affine sem dependencias. Reducoes SCF
        # permanecem sequenciais, o que e registrado como evidencia.
        "$MLIR_OPT" "$INPUT"             --affine-parallelize="max-nested=1"             -o "$OUTDIR/01_parallel.mlir"
        "$MLIR_OPT" "$OUTDIR/01_parallel.mlir"             --lower-affine             --convert-scf-to-openmp             --canonicalize             --convert-vector-to-scf             --convert-scf-to-cf             --convert-openmp-to-llvm             --convert-vector-to-llvm             --convert-arith-to-llvm             --finalize-memref-to-llvm             --convert-func-to-llvm             --convert-cf-to-llvm             --reconcile-unrealized-casts             -o "$OUTDIR/03_llvm.mlir"
        "$MLIR_TRANSLATE" --mlir-to-llvmir             "$OUTDIR/03_llvm.mlir" -o "$OUTDIR/kernel.ll"
        ;;
    *)
        echo "Configuracao desconhecida: $CONFIG" >&2
        exit 1
        ;;
esac
