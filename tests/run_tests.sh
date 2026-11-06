#!/bin/sh
# SPDX-License-Identifier: MIT
# GRAINE's tests: the simulator through every script in app/tests (every "expect" must hold).
set -e
cd "$(dirname "$0")/.."
./build.sh --host
out=$(mktemp -d)
fail=0
for t in app/tests/*.txt; do
    log=$(app/build/host/graine_host "$t" "$out" 2>&1) || fail=1
    ok=$(printf '%s\n' "$log" | grep -c '  ok' || true)
    if printf '%s\n' "$log" | grep -q 'FAIL'; then
        printf '%s\n' "$log" | grep 'FAIL'
        fail=1
    fi
    echo "$t: $ok checks"
done
exit $fail
