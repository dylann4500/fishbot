// Where does v0.5's time go?  Times the two shipped entry points separately.
#include "factory.hpp"
#include "game.hpp"
#include <chrono>
#include <cstdio>
using namespace fish;
using clk = std::chrono::steady_clock;
struct T : V05Agent {
  double tAsk = 0, tDecl = 0, tForced = 0; long long nAsk = 0, nDecl = 0, nForced = 0;
  AskMove chooseAsk(const PublicState& pub) override {
    auto a = clk::now(); auto r = V05Agent::chooseAsk(pub);
    tAsk += std::chrono::duration<double>(clk::now() - a).count(); nAsk++; return r; }
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& c) override {
    auto a = clk::now(); bool r = V05Agent::proposeDeclaration(pub, d, c);
    tDecl += std::chrono::duration<double>(clk::now() - a).count(); nDecl++; return r; }
  void bestGuess(const PublicState& pub, int s, Declaration& d, double& c) override {
    auto a = clk::now(); V05Agent::bestGuess(pub, s, d, c);
    tForced += std::chrono::duration<double>(clk::now() - a).count(); nForced++; }
  bool willingForced(const PublicState& pub, int s, Declaration& d, double& c, double th) override {
    auto a = clk::now(); bool r = V05Agent::willingForced(pub, s, d, c, th);
    tForced += std::chrono::duration<double>(clk::now() - a).count(); nForced++; return r; }
};
int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 200;
  int topk = argc > 2 ? atoi(argv[2]) : -1;
  int chain = argc > 3 ? atoi(argv[3]) : -1;
  Rules r; Game g; T ts[NPLAY]; Agent* ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) { if (topk >= 0) ts[p].cfg.searchTopK = topk;
    if (chain == 0) { ts[p].cfg.chainWeight = 0; ts[p].cfg.threatWeight = 0; } ag[p] = &ts[p]; }
  auto t0 = clk::now();
  long long asks = 0;
  for (int i = 0; i < games; i++) asks += g.run(mixSeed(1234567, i), r, ag).asks;
  double dt = std::chrono::duration<double>(clk::now() - t0).count();
  double ta = 0, td = 0, tf = 0; long long na = 0, nd = 0, nf = 0;
  for (int p = 0; p < NPLAY; p++) { ta += ts[p].tAsk; td += ts[p].tDecl; tf += ts[p].tForced;
    na += ts[p].nAsk; nd += ts[p].nDecl; nf += ts[p].nForced; }
  printf("topk=%d chain/threat=%s  total %.3fs | chooseAsk %.3fs (%.1f%%, n=%lld, %.0f us) | "
         "proposeDeclaration %.3fs (%.1f%%, n=%lld, %.0f us) | forced %.3fs (n=%lld)\n",
         topk < 0 ? 6 : topk, chain == 0 ? "off" : "on", dt, ta, 100*ta/dt, na, 1e6*ta/na,
         td, 100*td/dt, nd, 1e6*td/nd, tf, nf);
  return 0;
}
