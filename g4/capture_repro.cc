// Minimal Geant4 program: thermal neutrons in a block of one material, QGSP_BERT_HP,
// NuDEX capture model on or off. Writes one line per radiative capture:
//   event  Z  A(compound)  x y z (mm)  model  n  type:Ekin_keV ...
// Usage: capture_repro <run-seed> <n-events> <nudex 0|1> <out-file> [material]
#include "G4RunManagerFactory.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4VUserActionInitialization.hh"
#include "G4UserSteppingAction.hh"
#include "G4ParticleGun.hh"
#include "G4Neutron.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4PhysListFactory.hh"
#include "G4HadronicParameters.hh"
#include "G4PhysicsModelCatalog.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4VProcess.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include "Randomize.hh"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

std::FILE* gOut = nullptr;

class Detector : public G4VUserDetectorConstruction {
 public:
  explicit Detector(G4String m) : fMaterial(std::move(m)) {}
  G4VPhysicalVolume* Construct() override {
    auto* mat = G4NistManager::Instance()->FindOrBuildMaterial(fMaterial);
    if (!mat) G4Exception("Detector", "repro01", FatalException, "unknown material");
    auto* box = new G4Box("World", 50 * cm, 50 * cm, 50 * cm);
    auto* lv = new G4LogicalVolume(box, mat, "World");
    return new G4PVPlacement(nullptr, {}, lv, "World", nullptr, false, 0);
  }
 private:
  G4String fMaterial;
};

class Primary : public G4VUserPrimaryGeneratorAction {
 public:
  Primary() : fGun(1) {
    fGun.SetParticleDefinition(G4Neutron::Definition());
    fGun.SetParticleEnergy(0.0253 * eV);
    fGun.SetParticlePosition({});
  }
  void GeneratePrimaries(G4Event* e) override {
    const G4double c = 2 * G4UniformRand() - 1, s = std::sqrt(1 - c * c), p = twopi * G4UniformRand();
    fGun.SetParticleMomentumDirection({s * std::cos(p), s * std::sin(p), c});
    fGun.GeneratePrimaryVertex(e);
  }
 private:
  G4ParticleGun fGun;
};

class Stepping : public G4UserSteppingAction {
 public:
  void UserSteppingAction(const G4Step* step) override {
    const G4VProcess* proc = step->GetPostStepPoint()->GetProcessDefinedStep();
    if (!proc || step->GetTrack()->GetDefinition() != G4Neutron::Definition()) return;
    if (proc->GetProcessName().rfind("nCapture", 0) != 0) return;
    const auto* sec = step->GetSecondaryInCurrentStep();
    G4int Z = 0, A = 0;
    std::string model = "none", parts;
    G4int n = 0;
    for (const G4Track* t : *sec) {
      const G4ParticleDefinition* d = t->GetDefinition();
      if (model == "none") model = G4PhysicsModelCatalog::GetModelNameFromID(t->GetCreatorModelID());
      if (d->GetAtomicNumber() > 0 && d->GetAtomicMass() > 1) {
        Z = d->GetAtomicNumber(); A = d->GetAtomicMass();
        continue;
      }
      char type = d->GetParticleName() == "gamma" ? 'g' : d->GetParticleName() == "e-" ? 'e'
                : d->GetParticleName() == "e+" ? 'p' : '?';
      char buf[64];
      std::snprintf(buf, sizeof buf, " %c:%.3f", type, t->GetKineticEnergy() / keV);
      parts += buf;
      ++n;
    }
    const G4ThreeVector& x = step->GetPostStepPoint()->GetPosition();
    const G4int ev = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();
    std::fprintf(gOut, "%d %d %d %.3f %.3f %.3f %s %d%s\n", ev, Z, A, x.x() / mm, x.y() / mm, x.z() / mm,
                 model.c_str(), n, parts.c_str());
  }
};

class Actions : public G4VUserActionInitialization {
 public:
  void Build() const override {
    SetUserAction(new Primary);
    SetUserAction(new Stepping);
  }
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5) {
    std::fprintf(stderr, "usage: %s <run-seed> <n-events> <nudex 0|1> <out-file> [material]\n", argv[0]);
    return 2;
  }
  const long seed = std::atol(argv[1]);
  const int nev = std::atoi(argv[2]);
  const bool nudex = std::atoi(argv[3]) != 0;
  const G4String material = argc > 5 ? argv[5] : "G4_Fe";
  gOut = std::fopen(argv[4], "w");
  if (!gOut) return 3;

  G4Random::setTheSeed(seed);
  auto* rm = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial);
  // Must be set before the physics list is constructed.
  G4HadronicParameters::Instance()->SetEnableNUDEX(nudex);
  rm->SetUserInitialization(new Detector(material));
  rm->SetUserInitialization(G4PhysListFactory().GetReferencePhysList("QGSP_BERT_HP"));
  rm->SetUserInitialization(new Actions);
  rm->Initialize();
  rm->BeamOn(nev);
  std::fclose(gOut);
  delete rm;
  return 0;
}
