#!/usr/bin/env bash
# Build and run every PC test once for each board (DEVICE_ID 1 to 4).
# Prints one line per test and exits non-zero if any build or test fails.
cd "$(dirname "$0")" || exit 1
out=$(mktemp -d)
status=0
for id in 1 2 3 4; do
  for t in test test_main test_comm test_ui test_screen; do
    if ! g++ -std=c++17 -DDEVICE_ID=$id -Istubs -Isrc -o "$out/$t" "$t.cpp"; then
      echo "BUILD FAIL  $t (DEVICE_ID=$id)"
      status=1
      continue
    fi
    if "$out/$t" > "$out/log" 2>&1; then
      echo "ok          $t (DEVICE_ID=$id): $(tail -1 "$out/log")"
    else
      echo "TEST FAIL   $t (DEVICE_ID=$id)"
      grep -v '^PASS' "$out/log"
      status=1
    fi
  done
done
rm -rf "$out"
exit $status
