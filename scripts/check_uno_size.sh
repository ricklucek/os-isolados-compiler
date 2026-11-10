#!/bin/sh
set -eu

ELF=${1:-}
AVR_SIZE=${2:-avr-size}

if [ -z "$ELF" ] || [ ! -f "$ELF" ]; then
    echo "Uso: scripts/check_uno_size.sh programa.elf [avr-size]" >&2
    exit 1
fi

LINE=$("$AVR_SIZE" --format=berkeley "$ELF" | awk 'NR==2 {print $1 " " $2 " " $3}')
TEXT=$(echo "$LINE" | awk '{print $1}')
DATA=$(echo "$LINE" | awk '{print $2}')
BSS=$(echo "$LINE" | awk '{print $3}')

FLASH=$((TEXT + DATA))
RAM=$((DATA + BSS))
FLASH_LIMIT=32256
RAM_LIMIT=1792

echo "Memoria Uno: flash=$FLASH/$FLASH_LIMIT B, estatica_RAM=$RAM/$RAM_LIMIT B (+ frame verificado pelo compilador)"

if [ "$FLASH" -gt "$FLASH_LIMIT" ]; then
    echo "ERRO: programa excede a flash disponivel do Uno." >&2
    exit 1
fi

if [ "$RAM" -gt "$RAM_LIMIT" ]; then
    echo "ERRO: dados estaticos excedem o orcamento conservador de RAM do Uno." >&2
    exit 1
fi
