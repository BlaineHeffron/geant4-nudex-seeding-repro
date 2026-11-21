# geant4-nudex-seeding-repro

Minimal reproduction: the Geant4 NuDEX neutron-capture model (`G4NuDEXNeutronCaptureModel`) repeats the same
capture cascades whatever the run seed, and standalone NuDEX cascades depend on the order they are sampled in.
See [REPRO.md](REPRO.md) for the steps and the expected output.
