# NuDEX capture cascades ignore the Geant4 run seed

Two tests:

1. **Geant4 (11.3.2 and 11.4.2).** Thermal neutrons in a block of `G4_Fe`, `QGSP_BERT_HP` with
   `G4HadronicParameters::SetEnableNUDEX(true)`, two runs with different run seeds. The k-th capture on each
   nucleus gets the same cascade in both runs, even though every capture happens at a different position.
   With NuDEX off, the runs agree only as often as chance predicts. A patch that draws the cascade seed
   from `G4Random` (`patches/geant4-nudex-cascade-seed.patch`) brings NuDEX down to the chance rate too.
2. **Standalone NuDEX (UIN-CIEMAT/NuDEX 71d5d4d).** Same seeds, different order: `SetRandom3Seed(s)` before
   cascade `s`, run once for s = 1..N and once for s = N..1 on a fresh nucleus. Cascades that contain a
   conversion electron change with the order, because `SetRandom3Seed` does not reseed the
   internal-conversion generator. `patches/nudex-reseed-icc.patch` (one line) removes the dependence.

## Requirements

- Geant4 11.3.x or 11.4.x installed with its standard datasets (G4NDL etc.); `cmake` ≥ 3.16; a C++17 compiler; Python 3.
- To test the proposed Geant4 fix: the source tree matching the install (`GEANT4_SOURCE`).
- For test 2: ROOT (upstream NuDEX builds against ROOT's `TRandom2`; `include/NuDEXRandom.hh` sets
  `COMPILATIONTYPE 1`).
- Network access on the first run: G4NUDEXLIB 1.0 from cern.ch (MD5 checked), NuDEX and NuDEXlib from GitHub.

Tested with Geant4 11.3.2 and 11.4.2, G4NDL4.7.1, G4NUDEXLIB1.0, NuDEX 71d5d4d, NuDEXlib 17319c0, ROOT 6.34.02,
GCC 13.3, Ubuntu 24.04.

## Steps

```bash
git clone https://github.com/BlaineHeffron/geant4-nudex-seeding-repro.git
cd geant4-nudex-seeding-repro
export GEANT4_PREFIX=/path/to/geant4-11.3.2-install    # contains bin/geant4.sh
export GEANT4_SOURCE=/path/to/geant4-11.3.2-source     # optional: tests the fix
export ROOTSYS=/path/to/root                           # optional: runs test 2
scripts/run_all.sh
```

`DATA` (default `~/.cache/nudex-repro`) holds the downloaded libraries. Keep that path short: in 11.3.2 and
11.4.0, `G4NuDEXLevelDensity::ReadLDParameters` builds the file name in a `char fname[100]`
(`G4NuDEXLevelDensity.cc:68-70`), and a longer path aborts the run with `G4Exception ... Error in NuDEX`.
11.4.2 uses `std::string` there.

Running the pieces by hand:

```bash
source $GEANT4_PREFIX/bin/geant4.sh
export G4NUDEXLIBDATA=~/.cache/nudex-repro/G4NUDEXLIB1.0
cmake -S g4 -B build/g4 -DGeant4_DIR=$GEANT4_PREFIX/lib/cmake/Geant4 && cmake --build build/g4
build/g4/capture_repro 1 2000 1 seed1.txt     # <run-seed> <events> <nudex 0|1> <out> [material=G4_Fe]
build/g4/capture_repro 2 2000 1 seed2.txt
python3 scripts/compare_runs.py seed1.txt seed2.txt
```

Each line of the output file is one capture: `event Z A(compound) x y z [mm] creator-model n type:Ekin[keV] ...`.

## Expected output (Geant4 11.3.2; about 1 minute)

```
Geant4 11.3.2, G4NUDEXLIB1.0, QGSP_BERT_HP, G4_Fe, 2000 thermal neutrons per run

== NuDEX ON: run seed 1 vs run seed 2 (k-th capture on each nucleus)
nucleus       pairs  same cascade  chance  same position  model
Z=26 A=55        99            99      35              0  model_nuDEX_neutronCapture
Z=26 A=57      1849          1849     181              0  model_nuDEX_neutronCapture
Z=26 A=58        39            39       2              0  model_nuDEX_neutronCapture
Z=26 A=59         3             3       2              0  model_nuDEX_neutronCapture
all            1990          1990     220

== NuDEX OFF: run seed 1 vs run seed 2 (k-th capture on each nucleus)
nucleus       pairs  same cascade  chance  same position  model
Z=26 A=55       100             0       0              0  Undefined
Z=26 A=57      1841             2       2              0  Undefined
Z=26 A=58        36             0       0              0  Undefined
Z=26 A=59         2             0       0              0  Undefined
all            1979             2       2

== NuDEX ON with patches/geant4-nudex-cascade-seed.patch (LD_PRELOAD): run seed 1 vs run seed 2
nucleus       pairs  same cascade  chance  same position  model
Z=26 A=55        98            26      27              0  model_nuDEX_neutronCapture
Z=26 A=57      1850           203     195              0  model_nuDEX_neutronCapture
Z=26 A=58        42             1       1              0  model_nuDEX_neutronCapture
Z=26 A=59         4             1       2              0  model_nuDEX_neutronCapture
all            1994           231     225

NuDEX 71d5d4d, NuDEXlib 17319c0, 20000 thermal-capture cascades per nucleus

== upstream: SetRandom3Seed(s) before cascade s; s = 1..N vs s = N..1
compound   cascades    with e- (fwd)     differ fwd/rev      differing with e-
Z=26 A=57     20000            11081               7728                   7728
Z=82 A=208    20000              176                263                    263
Z=29 A=64     20000              103                199                    199
Z=13 A=28     20000                0                  2                      2

== fixed: SetRandom3Seed(s) before cascade s; s = 1..N vs s = N..1
compound   cascades    with e- (fwd)     differ fwd/rev      differing with e-
Z=26 A=57     20000            11142                  0                      0
Z=82 A=208    20000               48                  0                      0
Z=29 A=64     20000              170                  0                      0
Z=13 A=28     20000                0                  0                      0
```

How to read it:

- **same cascade**: the k-th capture on that nucleus emitted the same particles, in the same order, with the
  same energies (within 0.05 keV; the lab boost of a thermal capture moves a 7 MeV gamma by about 1 eV).
- **chance**: identical pairs expected if the two runs sampled independently from their own cascade
  frequencies. Fe-57 has a few dominant paths, so about 10% of independent pairs match.
- **same position**: no capture happens at the same place in both runs, so the neutron histories differ.
- `Undefined` is what `G4PhysicsModelCatalog` returns for the default `G4NeutronRadCaptureHP` secondaries.
- In test 2, **with e- (fwd)** counts cascades with a conversion electron in the forward pass, and
  **differing with e-** counts changed cascades that have one in either pass. Every changed cascade has one.

With Geant4 11.4.2 (`GEANT4_PREFIX`/`GEANT4_SOURCE` pointing at 11.4.2) the NuDEX-ON table shows 1989 of 1989
identical (chance 214) and the patched run 213 of 1981 (chance 219).

## Where it comes from

Line numbers are for Geant4 11.3.2 (the same in 11.4.0); 11.4.2 numbers are given where they differ. The code is
unchanged in 11.4.3 and 11.5.0.beta.
Paths are under `source/processes/hadronic/models/nudex/`.

- `src/G4NuDEXRandom.cc:81-87`: each `G4NuDEXRandom` owns a private `CLHEP::HepJamesRandom` seeded in its
  constructor. Nothing in the model reads `G4Random`.
- `src/G4NuDEXStatisticalNucleus.cc:90-95` (11.4.2: 93-98): the level-scheme (`theRandom1`), branching (`theRandom2`) and
  cascade (`theRandom3`) generators start from 1234567. `:1860-1862` (11.4.2: 1882-1884) replaces them with the seeds in
  `G4NUDEXLIB1.0/GeneralStatNuclParameters.dat`, whose default row (Z = A = 0) is also 1234567.
- `src/G4NuDEXStatisticalNucleus.cc:274` (11.4.2: 285) seeds the internal-conversion generator with `theRandom3`'s seed;
  `src/G4NuDEXInternalConversion.cc:224` creates it with 1234567 (line 223 in 11.4.2).
- `src/G4NuDEXNeutronCaptureModel.cc:239-254`: `GenerateNeutronCaptureCascade` calls `GenerateCascade` (line 249)
  with no reseeding, so every run replays the same sequence. Each worker thread builds its own model, so by the
  same code each thread should replay it too (read from the source; this repro runs serial only).
- `src/G4NuDEXNeutronCaptureModel.cc:267-269`: `Init` passes seed2 and seed3 to `SetRandom1Seed`. It is
  latent, because the model always calls `Init(theZA)` with the default zero seeds.
- `include/private/G4NuDEXStatisticalNucleus.hh:129`: `SetRandom3Seed` reseeds `theRandom3` but not the
  internal-conversion generator. Upstream NuDEX has the same line (`include/NuDEXStatisticalNucleus.hh:88`).
  This is what test 2 measures.
