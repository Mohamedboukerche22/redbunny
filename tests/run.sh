#!/usr/bin/env bash
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REDB="${REDB:-$ROOT/build/redb}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

if [ ! -x "$REDB" ]; then
    echo "error: compiler not found at $REDB (build it first)" >&2
    exit 1
fi

for src in "$ROOT"/tests/cases/*.redb; do
    name="$(basename "$src" .redb)"
    expected="${src%.redb}.expected"

    if ! "$REDB" "$src" -o "$TMP/$name" >"$TMP/$name.build" 2>&1; then
        echo "FAIL $name (compile)"
        cat "$TMP/$name.build"
        fail=$((fail + 1))
        continue
    fi

    input="${src%.redb}.in"
    if [ -f "$input" ]; then
        "$TMP/$name" <"$input" >"$TMP/$name.out" 2>&1
    else
        "$TMP/$name" >"$TMP/$name.out" 2>&1
    fi
    if diff -u "$expected" "$TMP/$name.out" >"$TMP/$name.diff"; then
        echo "PASS $name"
        pass=$((pass + 1))
    else
        echo "FAIL $name (output)"
        cat "$TMP/$name.diff"
        fail=$((fail + 1))
    fi
done

for src in "$ROOT"/tests/errors/*.redb; do
    name="$(basename "$src" .redb)"

    if "$REDB" "$src" -o "$TMP/err_$name" >"$TMP/err_$name.log" 2>&1; then
        echo "FAIL $name (expected compile error)"
        fail=$((fail + 1))
    else
        echo "PASS $name (rejected)"
        pass=$((pass + 1))
    fi
done

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
