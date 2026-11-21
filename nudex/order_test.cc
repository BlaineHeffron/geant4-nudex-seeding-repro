// History-order test for standalone NuDEX.
// For each target, two fresh NuDEXStatisticalNucleus instances (same level/branching seeds from
// the library) generate N thermal-capture cascades with SetRandom3Seed(s) before each cascade,
// s = 1..N. Instance A runs s = 1..N, instance B runs s = N..1. With complete per-cascade
// seeding, cascade s is the same in both. Prints, per compound nucleus, how many differ.
// Usage: order_test <NuDEXlib dir> <N> <target ZA> [<target ZA> ...]
#include "NuDEXStatisticalNucleus.hh"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

struct Cascade {
  std::vector<char> type;
  std::vector<double> energy, time;
  bool HasElectron() const {
    for (char t : type) if (t == 'e') return true;
    return false;
  }
  bool operator==(const Cascade& o) const { return type == o.type && energy == o.energy && time == o.time; }
};

std::vector<Cascade> Run(const char* lib, int Z, int A, int n, bool reverse) {
  NuDEXStatisticalNucleus nucleus(Z, A);
  if (nucleus.Init(lib) < 0 || !nucleus.HasThermalLevel()) {
    std::fprintf(stderr, "no thermal capture data for Z=%d A=%d\n", Z, A);
    std::exit(1);
  }
  double sn = 0, i0 = 0;
  nucleus.GetSnAndI0(sn, i0);
  std::vector<Cascade> out(n);
  for (int k = 0; k < n; ++k) {
    const int s = reverse ? n - k : k + 1;  // seed of this cascade
    Cascade& c = out[s - 1];
    nucleus.SetRandom3Seed(s);
    if (nucleus.GenerateCascade(-1, sn, c.type, c.energy, c.time) <= 0) {
      std::fprintf(stderr, "GenerateCascade failed, seed %d\n", s);
      std::exit(1);
    }
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <NuDEXlib dir> <N> <target ZA>...\n", argv[0]);
    return 2;
  }
  const char* lib = argv[1];
  const int n = std::atoi(argv[2]);
  std::printf("%-10s %8s %16s %18s %22s\n", "compound", "cascades", "with e- (fwd)", "differ fwd/rev", "differing with e-");
  for (int i = 3; i < argc; ++i) {
    const int za = std::atoi(argv[i]), Z = za / 1000, A = za % 1000 + 1;
    const auto fwd = Run(lib, Z, A, n, false);
    const auto rev = Run(lib, Z, A, n, true);
    int with_e = 0, differ = 0, differ_e = 0;
    for (int s = 0; s < n; ++s) {
      with_e += fwd[s].HasElectron();
      if (!(fwd[s] == rev[s])) {
        ++differ;
        differ_e += fwd[s].HasElectron() || rev[s].HasElectron();
      }
    }
    std::printf("Z=%-2d A=%-3d %8d %16d %18d %22d\n", Z, A, n, with_e, differ, differ_e);
  }
  return 0;
}
