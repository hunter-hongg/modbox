#!/usr/bin/env bash
#
# run_tests.sh — Front-end for the modbox test suite.
#
# Default behaviour delegates to tests/run_tests.py, a Python orchestrator
# that runs the existing tests/test_*.sh files unchanged but in parallel
# across all CPU cores (~77s -> ~28s on an 8-core box).
#
# Fallbacks (kept for zero-dependency / debugging use):
#   SERIAL=1   bash tests/run_tests.sh   -> Python orchestrator, serial
#   USE_BASH=1 bash tests/run_tests.sh   -> original pure-bash runner below
#
# Any extra args (e.g. "ls", "--workers 4") are forwarded to run_tests.py.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PY="${PYTHON:-python3}"

if [[ "${USE_BASH:-0}" == "1" ]]; then
  # ── Pure-bash fallback: original serial runner (zero-dep use) ──
  exec 0</dev/null
  source "$SCRIPT_DIR/framework.sh"
  echo "============================================"
  echo "  modbox Test Suite (pure bash, serial)"
  echo "  Binary: $MODBOX"
  echo "============================================"
  echo ""
  ALL_OUTPUT=""
  for test_file in "$SCRIPT_DIR"/test_*.sh; do
    TEST_OUT=$(
      source "$SCRIPT_DIR/framework.sh"
      source "$test_file"
      printf "__PASS__=%s\n" "$PASS_COUNT"
      printf "__FAIL__=%s\n" "$FAIL_COUNT"
    )
    ALL_OUTPUT="${ALL_OUTPUT}${TEST_OUT}
"
  done
  echo "$ALL_OUTPUT" | grep -v "^__PASS__\|^__FAIL__"
  PASS_COUNT=0
  FAIL_COUNT=0
  while IFS= read -r line; do
    if [[ "$line" =~ ^__PASS__=([0-9]+) ]]; then
      PASS_COUNT=$((PASS_COUNT + BASH_REMATCH[1]))
    elif [[ "$line" =~ ^__FAIL__=([0-9]+) ]]; then
      FAIL_COUNT=$((FAIL_COUNT + BASH_REMATCH[1]))
    fi
  done <<< "$ALL_OUTPUT"
  echo ""
  echo "════════════════════════════════════════════"
  echo "  Results: $PASS_COUNT passed, $FAIL_COUNT failed"
  echo "════════════════════════════════════════════"
  [[ $FAIL_COUNT -gt 0 ]] && exit 1
  exit 0
fi

# ── Default: delegate to the Python orchestrator ──
if [[ "${SERIAL:-0}" == "1" ]]; then
  exec "$PY" "$SCRIPT_DIR/run_tests.py" --serial "$@"
else
  exec "$PY" "$SCRIPT_DIR/run_tests.py" "$@"
fi
