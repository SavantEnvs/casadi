#!/usr/bin/env bash
#
# mayhem/test.sh — run casadi's known-answer oracle (built by mayhem/build.sh against a normally-built
# libcasadi): symbolic differentiation, MX evaluation, serialization round trip, the qrqp and ipqp QP
# solvers, Newton rootfinding, CVODES integration, interpolation, the Matrix Market reader and the
# FMI description parser. Each case prints "<name> <value>" and is compared here against the known answer.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

KAT="$SRC/build-tests/kat"
if [ ! -x "$KAT" ]; then
  echo "missing $KAT — mayhem/build.sh did not build it" >&2
  emit_ctrf casadi-kat 0 1 0
  exit 1
fi

export CASADI_PLUGIN_SEARCH_PATH="$SRC/build-tests/lib"
actual="$("$KAT" "$SRC/mayhem/fuzz_fmi_description/testsuite/fmi2_basic.xml" 2>/dev/null)"

expected="sx_value 18.00000
sx_dfdx 12.00000
mx_mtimes0 17.00000
mx_mtimes1 39.00000
serialize_roundtrip 9.00000
qrqp_x0 0.50000
qrqp_x1 0.50000
qrqp_cost -0.75000
ipqp_x0 0.50000
ipqp_x1 0.50000
ipqp_cost -0.75000
rootfinder_sqrt2 1.41421
cvodes_exp 0.36788
interpolant_linear 6.50000
mtx_nnz 3.00000
mtx_sum 2.75000
mtx_32 3.25000
fmi_nx 1.00000
fmi_nu 1.00000"

passed=0; failed=0
while read -r name want; do
  got="$(printf '%s\n' "$actual" | awk -v n="$name" '$1==n {print $2}')"
  if [ "$got" = "$want" ]; then
    passed=$((passed + 1))
  else
    echo "FAIL $name: expected $want, got '${got:-<missing>}'" >&2
    failed=$((failed + 1))
  fi
done <<< "$expected"

emit_ctrf casadi-kat "$passed" "$failed" 0
