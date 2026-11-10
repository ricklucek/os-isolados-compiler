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

"$BIN" tests/placa/08_pinole_compila.mcn --alvo=uno -o "$OUT/pinole.ll" >/dev/null ||
    fail "pinole deveria compilar para Uno"
grep -q '@zl_pino_le' "$OUT/pinole.ll" ||
    fail "builtin pinole nao chegou ao LLVM"

for file in     tests/placa/09_imprime_constante.mcn     tests/placa/10_laco_escalar.mcn; do
    LLVM_AS="$LLVM_AS" LLVM_OPT="$LLVM_OPT" LLVM_LLC="$LLVM_LLC"     AVR_GCC="$AVR_GCC" AVR_OBJCOPY="$AVR_OBJCOPY" AVR_SIZE="$AVR_SIZE"         sh scripts/build_uno.sh "$file" "$OUT" "$BIN" >/dev/null ||
        fail "build diagnostico AVR falhou para $file"
done

run_simavr() {
    hex="$1"
    output="$2"
    status=0
    timeout 2s simavr -m atmega328p -f 16000000 "$hex"         > "$output" 2>&1 || status=$?
    [ "$status" -eq 0 ] || [ "$status" -eq 124 ] ||
        fail "simavr falhou para $hex (exit $status)"
}

run_simavr "$OUT/09_imprime_constante.hex" "$OUT/constante.sim"
grep -Eq '(^|[^0-9])15([^0-9]|$)' "$OUT/constante.sim" ||
    fail "simavr: saida(15) nao produziu 15"

run_simavr "$OUT/10_laco_escalar.hex" "$OUT/laco.sim"
grep -Eq '(^|[^0-9])15([^0-9]|$)' "$OUT/laco.sim" ||
    fail "simavr: laco escalar nao produziu 15"

for file in     tests/placa/11_vetor_constante.mcn     tests/placa/12_vetor_leitura_dinamica.mcn     tests/placa/13_vetor_escrita_dinamica.mcn; do
    LLVM_AS="$LLVM_AS" LLVM_OPT="$LLVM_OPT" LLVM_LLC="$LLVM_LLC"     AVR_GCC="$AVR_GCC" AVR_OBJCOPY="$AVR_OBJCOPY" AVR_SIZE="$AVR_SIZE"         sh scripts/build_uno.sh "$file" "$OUT" "$BIN" >/dev/null ||
        fail "build diagnostico vetorial AVR falhou para $file"
done

run_simavr "$OUT/11_vetor_constante.hex" "$OUT/vetor_constante.sim"
grep -Eq '(^|[^0-9])15([^0-9]|$)' "$OUT/vetor_constante.sim" ||
    fail "simavr: vetor com indices constantes nao produziu 15"

run_simavr "$OUT/12_vetor_leitura_dinamica.hex" "$OUT/vetor_leitura.sim"
grep -Eq '(^|[^0-9])15([^0-9]|$)' "$OUT/vetor_leitura.sim" ||
    fail "simavr: leitura vetorial dinamica nao produziu 15"

run_simavr "$OUT/13_vetor_escrita_dinamica.hex" "$OUT/vetor_escrita.sim"
grep -Eq '(^|[^0-9])15([^0-9]|$)' "$OUT/vetor_escrita.sim" ||
    fail "simavr: escrita vetorial dinamica nao produziu 15"

run_simavr "$OUT/01_laco_vetor.hex" "$OUT/vetor.sim"
grep -Eq '(^|[^0-9])15([^0-9]|$)' "$OUT/vetor.sim" ||
    fail "simavr: laco+vetor nao produziu 15"

echo "Arduino Uno: alvo, memoria, builtins, ELF/HEX e simulacao serial OK."
