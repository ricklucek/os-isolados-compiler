#!/bin/sh
set -u

BIN=${1:-./macaronica}
OUT=.uno-test
IR=.uno-test.ll

cleanup() {
    rm -rf "$OUT" "$IR"
}
trap cleanup EXIT INT TERM
cleanup
mkdir -p "$OUT"

fail() {
    echo "FALHA UNO: $1" >&2
    exit 1
}

for file in     tests/placa/01_laco_vetor.mcn     tests/placa/02_recursao.mcn     tests/placa/03_erro_execucao.mcn     tests/placa/04_pisca_led.mcn     tests/placa/05_memoria_ok.mcn; do
    "$BIN" "$file" --alvo=uno -o "$IR" >/dev/null ||
        fail "$file deveria passar em --alvo=uno"
done

status=0
"$BIN" tests/placa/06_memoria_excede.mcn --alvo=uno -o "$IR"     >/dev/null 2> "$OUT/mem.err" || status=$?
[ "$status" -eq 6 ] || fail "memoria excedida deveria retornar 6, recebeu $status"
grep -q "alvo 'uno'" "$OUT/mem.err" ||
    fail "diagnostico de RAM do Uno ausente"

status=0
"$BIN" tests/placa/07_duplocarpado_rejeitado.mcn --alvo=uno -o "$IR"     >/dev/null 2> "$OUT/duplo.err" || status=$?
[ "$status" -eq 6 ] || fail "duplocarpado no Uno deveria retornar 6"
grep -q "nao suporta duplocarpado" "$OUT/duplo.err" ||
    fail "diagnostico de duplocarpado ausente"

for file in     tests/placa/01_laco_vetor.mcn     tests/placa/02_recursao.mcn     tests/placa/03_erro_execucao.mcn     tests/placa/04_pisca_led.mcn; do
    LLVM_AS="$LLVM_AS" LLVM_OPT="$LLVM_OPT" LLVM_LLC="$LLVM_LLC"     AVR_GCC="$AVR_GCC" AVR_OBJCOPY="$AVR_OBJCOPY" AVR_SIZE="$AVR_SIZE"         sh scripts/build_uno.sh "$file" "$OUT" "$BIN" >/dev/null ||
        fail "build AVR falhou para $file"
    base=$(basename "$file" .mcn)
    [ -s "$OUT/$base.elf" ] || fail "ELF ausente para $file"
    [ -s "$OUT/$base.hex" ] || fail "HEX ausente para $file"
done

grep -q '@zl_pino_modo' "$OUT/04_pisca_led.ll" ||
    fail "builtin pinomodo nao chegou ao LLVM"
grep -q '@zl_pino_escreve' "$OUT/04_pisca_led.ll" ||
    fail "builtin pinoescreve nao chegou ao LLVM"
grep -q '@zl_espera' "$OUT/04_pisca_led.ll" ||
    fail "builtin espera nao chegou ao LLVM"

echo "Arduino Uno: alvo, memoria, builtins e ELF/HEX OK."
