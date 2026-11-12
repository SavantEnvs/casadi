#!/usr/bin/env bash
#
# mayhem/build.sh — build casadi's fuzz harnesses (sanitized), their -standalone reproducers, and the
# known-answer oracle program (normal flags) that mayhem/test.sh runs. Runs in the commit image as
# `mayhem` in /mayhem. Offline: casadi builds Sundials, CSparse and TinyXML from the sources it ships.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

TARGETS="function_deserialize fmi_description dm_txt dm_mtx sparsity_mtx conic_qp"

# UBSan relaxations (each is benign by construction in casadi, and would halt every input):
#   vptr       — CRTP static_cast to the derived class inside base-class constructors
#                (generic_shared_internal.hpp), so every object construction trips it.
#   shift-base — serializing_stream.cpp packs signed chars with a left shift.
UBSAN_RELAX="-fno-sanitize=vptr,shift-base"

# 1) casadi itself, sanitized, with DWARF 3. WITH_DEEPBIND=OFF: the sanitizer runtimes refuse
#    dlopen(RTLD_DEEPBIND), which casadi uses to load its plugins (libcasadi_conic_qrqp.so, ...).
F="-O1 $SANITIZER_FLAGS $UBSAN_RELAX -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
L="$SANITIZER_FLAGS $UBSAN_RELAX -fsanitize=fuzzer-no-link"
cmake -G Ninja -S "$SRC" -B "$SRC/build-fuzz" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_C_FLAGS_RELEASE="$F" -DCMAKE_CXX_FLAGS_RELEASE="$F" \
  -DCMAKE_SHARED_LINKER_FLAGS="$L" -DCMAKE_EXE_LINKER_FLAGS="$L" \
  -DWITH_PYTHON=OFF -DWITH_DEEPBIND=OFF
cmake --build "$SRC/build-fuzz" -j"$MAYHEM_JOBS"

# 2) Each harness twice: against libFuzzer, and against the run-once standalone driver.
HF="-std=c++17 -O1 $SANITIZER_FLAGS $UBSAN_RELAX $DEBUG_FLAGS"
INC="-I$SRC -I$SRC/build-fuzz"
LNK="-L$SRC/build-fuzz/lib -lcasadi -Wl,-rpath,$SRC/build-fuzz/lib"
$CXX $HF -c "$SRC/mayhem/lsan_off.cc" -o "$SRC/build-fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$SRC/build-fuzz/standalone_main.o"
for t in $TARGETS; do
  $CXX $HF $LIB_FUZZING_ENGINE "$SRC/mayhem/fuzz_$t.cc" "$SRC/build-fuzz/lsan_off.o" $INC $LNK \
    -o "/mayhem/fuzz_$t"
  $CXX $HF "$SRC/mayhem/fuzz_$t.cc" "$SRC/build-fuzz/standalone_main.o" "$SRC/build-fuzz/lsan_off.o" $INC $LNK \
    -o "/mayhem/fuzz_$t-standalone"
done

# 3) The oracle: a clean, normally-flagged casadi build plus the known-answer program.
cmake -G Ninja -S "$SRC" -B "$SRC/build-tests" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DCMAKE_SHARED_LINKER_FLAGS="$COVERAGE_FLAGS" -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS" \
  -DWITH_PYTHON=OFF -DWITH_DEEPBIND=OFF
cmake --build "$SRC/build-tests" -j"$MAYHEM_JOBS"
$CXX -std=c++17 -O1 $COVERAGE_FLAGS "$SRC/mayhem/kat.cc" -I"$SRC" -I"$SRC/build-tests" \
  -L"$SRC/build-tests/lib" -lcasadi -Wl,-rpath,"$SRC/build-tests/lib" -o "$SRC/build-tests/kat"
