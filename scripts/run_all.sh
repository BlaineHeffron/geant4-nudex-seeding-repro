#!/bin/bash
# Build and run both tests; print the evidence.
# Environment:
#   GEANT4_PREFIX   Geant4 install prefix (contains bin/geant4.sh)            [required]
#   GEANT4_SOURCE   matching Geant4 source tree, to test the proposed fix    [optional]
#   ROOTSYS         ROOT install prefix, for the standalone NuDEX test        [optional; skipped if unset]
#   DATA            where to put G4NUDEXLIB1.0 and NuDEXlib [default ~/.cache/nudex-repro]
#                   Keep it short: Geant4 11.3.2/11.4.0 read the level-density file through a
#                   char[100] path buffer (G4NuDEXLevelDensity.cc:68-70) and abort on longer paths.
#   EVENTS          neutrons per Geant4 run [2000];  CASCADES  cascades per nucleus [20000]
#   BUILD, OUT      build and output directories [build/, out/]
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
: "${GEANT4_PREFIX:?set GEANT4_PREFIX to the Geant4 install prefix}"
DATA=${DATA:-$HOME/.cache/nudex-repro}
EVENTS=${EVENTS:-2000}
CASCADES=${CASCADES:-20000}
CMAKE=${CMAKE:-cmake}
build=${BUILD:-$here/build}
out=${OUT:-$here/out}
mkdir -p "$DATA" "$out"

# --- data ---
if [[ ! -d $DATA/G4NUDEXLIB1.0 ]]; then
  curl -sSfL -o "$DATA/G4NUDEXLIB.1.0.tar.gz" https://cern.ch/geant4-data/datasets/G4NUDEXLIB.1.0.tar.gz
  echo "09a85f907d2282dbf234d1784f436db3  $DATA/G4NUDEXLIB.1.0.tar.gz" | md5sum -c --quiet
  tar -xzf "$DATA/G4NUDEXLIB.1.0.tar.gz" -C "$DATA"
fi

# --- Geant4 test: two run seeds ---
set +u; source "$GEANT4_PREFIX/bin/geant4.sh"; set -u
export G4NUDEXLIBDATA=$DATA/G4NUDEXLIB1.0
fixopt=()
[[ -n ${GEANT4_SOURCE:-} ]] && fixopt=(-DGEANT4_SOURCE_DIR="$GEANT4_SOURCE")
"$CMAKE" -S "$here/g4" -B "$build/g4" -DCMAKE_BUILD_TYPE=Release -DGeant4_DIR="$GEANT4_PREFIX/lib/cmake/Geant4" "${fixopt[@]}" >/dev/null
"$CMAKE" --build "$build/g4" -j"$(nproc)" >/dev/null
echo "Geant4 $(geant4-config --version), G4NUDEXLIB1.0, QGSP_BERT_HP, G4_Fe, $EVENTS thermal neutrons per run"
for nudex in 1 0; do
  for seed in 1 2; do
    "$build/g4/capture_repro" $seed "$EVENTS" $nudex "$out/g4_nudex${nudex}_seed$seed.txt" > "$out/g4_nudex${nudex}_seed$seed.log" 2>&1 &
  done
  wait
  echo; echo "== NuDEX $([[ $nudex == 1 ]] && echo ON || echo OFF): run seed 1 vs run seed 2 (k-th capture on each nucleus)"
  python3 "$here/scripts/compare_runs.py" "$out/g4_nudex${nudex}_seed1.txt" "$out/g4_nudex${nudex}_seed2.txt"
done
if [[ -n ${GEANT4_SOURCE:-} ]]; then
  for seed in 1 2; do
    LD_PRELOAD=$build/g4/libnudex_patched.so "$build/g4/capture_repro" $seed "$EVENTS" 1 "$out/g4_fixed_seed$seed.txt" > "$out/g4_fixed_seed$seed.log" 2>&1 &
  done
  wait
  echo; echo "== NuDEX ON with patches/geant4-nudex-cascade-seed.patch (LD_PRELOAD): run seed 1 vs run seed 2"
  python3 "$here/scripts/compare_runs.py" "$out/g4_fixed_seed1.txt" "$out/g4_fixed_seed2.txt"
fi

# --- standalone NuDEX test: history order ---
if [[ -z ${ROOTSYS:-} ]]; then echo; echo "ROOTSYS unset: skipping the standalone NuDEX order test"; exit 0; fi
if [[ ! -d $DATA/NuDEXlib ]]; then
  git clone -q https://github.com/UIN-CIEMAT/NuDEXlib.git "$DATA/NuDEXlib"
  git -C "$DATA/NuDEXlib" checkout -q 17319c0
fi
"$CMAKE" -S "$here/nudex" -B "$build/nudex" -DCMAKE_BUILD_TYPE=Release -DROOT_DIR="$ROOTSYS/cmake" >/dev/null
"$CMAKE" --build "$build/nudex" -j"$(nproc)" >/dev/null
echo; echo "NuDEX $(git -C "$build/nudex/nudex-upstream" rev-parse --short=7 HEAD), NuDEXlib $(git -C "$DATA/NuDEXlib" rev-parse --short=7 HEAD), $CASCADES thermal-capture cascades per nucleus"
for v in upstream fixed; do
  "$build/nudex/order_test_$v" "$DATA/NuDEXlib" "$CASCADES" 26056 82207 29063 13027 > "$out/order_$v.log" 2>&1 &
done
wait
for v in upstream fixed; do
  echo; echo "== $v: SetRandom3Seed(s) before cascade s; s = 1..N vs s = N..1"
  grep -E '^(compound|Z=)' "$out/order_$v.log"
done
