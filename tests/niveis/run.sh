#!/bin/sh
set -u
BIN=${1:-./macaronica}
PASS=0
FAIL=0
for file in tests/niveis/nivel*.mcn; do
    [ -f "$file" ] || continue
    status=0
    "$BIN" "$file" >/dev/null 2>&1 || status=$?
    if [ "$status" -eq 0 ]; then
        echo "[OK] $file"
        PASS=$((PASS + 1))
    else
        echo "[FALHA] $file retornou exit $status" >&2
        FAIL=$((FAIL + 1))
    fi
done
printf "\nNiveis: %d OK, %d falha(s).\n" "$PASS" "$FAIL"
[ "$PASS" -eq 5 ] && [ "$FAIL" -eq 0 ]
