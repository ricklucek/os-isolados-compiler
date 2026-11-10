#!/bin/sh
set -eu

SOURCE=${1:-}
OUTDIR=${2:-placa}
BIN=${3:-./macaronica}

LLVM_AS=${LLVM_AS:-llvm-as-18}
LLVM_OPT=${LLVM_OPT:-opt-18}
LLVM_LLC=${LLVM_LLC:-llc-18}
AVR_GCC=${AVR_GCC:-avr-gcc}
AVR_OBJCOPY=${AVR_OBJCOPY:-avr-objcopy}
AVR_SIZE=${AVR_SIZE:-avr-size}

if [ -z "$SOURCE" ] || [ ! -f "$SOURCE" ]; then
    echo "Uso: scripts/build_uno.sh programa.mcn [outdir] [compilador]" >&2
    exit 1
fi

mkdir -p "$OUTDIR"
BASE=$(basename "$SOURCE")
BASE=${BASE%.*}
LL="$OUTDIR/$BASE.ll"
BC="$OUTDIR/$BASE.bc"
OBJ="$OUTDIR/$BASE.o"
RUNTIME_OBJ="$OUTDIR/zl_runtime_avr.o"
ELF="$OUTDIR/$BASE.elf"
HEX="$OUTDIR/$BASE.hex"

"$BIN" "$SOURCE" --alvo=uno -o "$LL" >/dev/null
"$LLVM_AS" "$LL" -o "$BC"
"$LLVM_OPT" -passes=verify "$LL" -disable-output

"$LLVM_LLC" -O2 -mtriple=avr -mcpu=atmega328p     -filetype=obj "$BC" -o "$OBJ"

"$AVR_GCC" -std=gnu99 -Os -mmcu=atmega328p     -DF_CPU=16000000UL -ffunction-sections -fdata-sections     -Iruntime -c runtime/zl_runtime_avr.c -o "$RUNTIME_OBJ"

"$AVR_GCC" -mmcu=atmega328p -Wl,--gc-sections     "$OBJ" "$RUNTIME_OBJ" -lm -o "$ELF"

"$AVR_OBJCOPY" -O ihex -R .eeprom "$ELF" "$HEX"
sh scripts/check_uno_size.sh "$ELF" "$AVR_SIZE"

echo "Arduino Uno: artefatos gerados"
echo "  LLVM: $LL"
echo "  ELF:  $ELF"
echo "  HEX:  $HEX"
