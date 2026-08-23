#include "probe_vwill.hpp"
#include <iostream>
#include <cstring>

int main(int argc, char** argv) {
  using namespace fish;
  vwill::WCfg c;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&](const char* k) -> const char* {
      size_t n = strlen(k);
      return (a.size() > n && a.compare(0, n, k) == 0) ? a.c_str() + n : nullptr;
    };
    if (auto v = val("--a=")) c.specA = v;
    else if (auto v = val("--b=")) c.specB = v;
    else if (auto v = val("--games=")) c.games = atoi(v);
    else if (auto v = val("--rotations=")) c.rotations = atoi(v);
    else if (auto v = val("--seed=")) c.seed = strtoull(v, nullptr, 10);
    else if (auto v = val("--threads=")) c.threads = atoi(v);
  }
  vwill::WStats s = vwill::runVWill(c);
  std::cout << "vwill  " << c.specA << " vs " << c.specB
            << "  seed=" << c.seed << "  games=" << c.games << "x" << c.rotations << "\n";
  vwill::printVWill(c, s, std::cout);
  return 0;
}
