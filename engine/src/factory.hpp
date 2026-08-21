// Policy specification parser: "name" or "name:key=value,key=value".
#pragma once
#include "baselines.hpp"
#include "v04.hpp"
#include <memory>
#include <map>
#include <sstream>

namespace fish {

inline std::map<std::string, std::string> parseOpts(const std::string& spec, std::string& base) {
  std::map<std::string, std::string> o;
  auto colon = spec.find(':');
  base = colon == std::string::npos ? spec : spec.substr(0, colon);
  if (colon == std::string::npos) return o;
  std::string rest = spec.substr(colon + 1);
  std::stringstream ss(rest);
  std::string item;
  while (std::getline(ss, item, ',')) {
    auto eq = item.find('=');
    if (eq == std::string::npos) continue;
    o[item.substr(0, eq)] = item.substr(eq + 1);
  }
  return o;
}

inline double optD(const std::map<std::string, std::string>& o, const char* k, double dflt) {
  auto it = o.find(k); return it == o.end() ? dflt : atof(it->second.c_str());
}
inline int optI(const std::map<std::string, std::string>& o, const char* k, int dflt) {
  auto it = o.find(k); return it == o.end() ? dflt : atoi(it->second.c_str());
}

inline std::unique_ptr<Agent> makeAgent(const std::string& spec) {
  std::string base;
  auto o = parseOpts(spec, base);
  if (base == "v04" || base == "fishbot_v04") {
    auto a = std::make_unique<V04Agent>();
    auto it = o.find("belief");
    if (it != o.end()) {
      if (it->second == "exact") a->cfg.belief = BeliefMode::Exact;
      else if (it->second == "exactdisj") a->cfg.belief = BeliefMode::ExactDisj;
      else if (it->second == "sinkhorn") a->cfg.belief = BeliefMode::Sinkhorn;
      else if (it->second == "indep") a->cfg.belief = BeliefMode::Independent;
      else if (it->second == "hybrid") a->cfg.belief = BeliefMode::Hybrid;
      else if (it->second == "fast") a->cfg.belief = BeliefMode::Fast;
      else if (it->second == "block") a->cfg.belief = BeliefMode::Block;
    }
    a->cfg.particles         = optI(o, "particles", a->cfg.particles);
    a->cfg.declThreshold     = optD(o, "decl", a->cfg.declThreshold);
    a->cfg.lockedAllocThresh = optD(o, "lockthr", a->cfg.lockedAllocThresh);
    a->cfg.minTeamProb       = optD(o, "minteam", a->cfg.minTeamProb);
    a->cfg.patientLocked     = optI(o, "patient", a->cfg.patientLocked ? 1 : 0) != 0;
    a->cfg.askFloor          = optD(o, "askfloor", a->cfg.askFloor);
    a->cfg.patiencePool      = optI(o, "pool", a->cfg.patiencePool);
    a->cfg.forceDeclareEvents= optI(o, "force", a->cfg.forceDeclareEvents);
    a->cfg.oppCardFloor      = optD(o, "oppfloor", a->cfg.oppCardFloor);
    a->cfg.gateTeamProb      = optD(o, "gate", a->cfg.gateTeamProb);
    a->cfg.marginalGate      = optD(o, "mgate", a->cfg.marginalGate);
    a->cfg.sinkOuter         = optI(o, "souter", a->cfg.sinkOuter);
    a->cfg.sinkInner         = optI(o, "sinner", a->cfg.sinkInner);
    a->cfg.useValue          = optI(o, "value", a->cfg.useValue ? 1 : 0) != 0;
    a->cfg.valueWeight       = optD(o, "vweight", a->cfg.valueWeight);
    a->cfg.linearWeight      = optD(o, "lweight", a->cfg.linearWeight);
    a->cfg.valueDeclare      = optI(o, "vdecl", a->cfg.valueDeclare ? 1 : 0) != 0;
    a->cfg.declareMargin     = optD(o, "vmargin", a->cfg.declareMargin);
    a->cfg.priorTheta        = optD(o, "ptheta", a->cfg.priorTheta);
    a->cfg.priorPhi          = optD(o, "pphi", a->cfg.priorPhi);
    a->cfg.greedyMAP         = optI(o, "gmap", a->cfg.greedyMAP ? 1 : 0) != 0;
    a->cfg.searchTopK        = optI(o, "topk", a->cfg.searchTopK);
    a->cfg.chainWeight       = optD(o, "chain", a->cfg.chainWeight);
    a->cfg.threatWeight      = optD(o, "threat", a->cfg.threatWeight);
    for (int i = 0; i < NVFEAT; i++) {
      char key[10]; snprintf(key, sizeof(key), "v%d", i);
      a->cfg.vw[i] = optD(o, key, a->cfg.vw[i]);
    }
    auto vv = o.find("vweights");
    if (vv != o.end()) {
      std::stringstream vs(vv->second); std::string tok; int i = 0;
      while (std::getline(vs, tok, '|') && i < NVFEAT) a->cfg.vw[i++] = atof(tok.c_str());
    }
    a->cfg.declareEnabled    = optI(o, "declare", 1) != 0;
    for (int i = 0; i < NFEAT; i++) {
      char key[8]; snprintf(key, sizeof(key), "w%d", i);
      a->cfg.w[i] = optD(o, key, a->cfg.w[i]);
    }
    // Flat parameter vector for the optimiser: 18 ask weights followed by the
    // declaration and search knobs, so everything can be fitted jointly.
    auto ap = o.find("allparams");
    if (ap != o.end()) {
      std::vector<double> v;
      std::stringstream ws(ap->second); std::string tok;
      while (std::getline(ws, tok, '|')) v.push_back(atof(tok.c_str()));
      for (int i = 0; i < NFEAT && i < (int)v.size(); i++) a->cfg.w[i] = v[i];
      // Knobs start immediately after the NFEAT ask weights.  Hard-coding 18
      // here silently aliased two ask weights onto the first two knobs when
      // NFEAT grew to 20, so the vector the optimiser scored was not the vector
      // freeze_config.py bakes in.  Derive the offset instead.
      auto get = [&](size_t i, double d) { return i < v.size() ? v[i] : d; };
      const size_t K = size_t(NFEAT);
      a->cfg.declThreshold     = std::min(0.9999, std::max(0.5, get(K + 0, a->cfg.declThreshold)));
      a->cfg.lockedAllocThresh = std::min(0.99999, std::max(0.5, get(K + 1, a->cfg.lockedAllocThresh)));
      a->cfg.askFloor          = std::min(0.9, std::max(0.0, get(K + 2, a->cfg.askFloor)));
      a->cfg.patiencePool      = std::max(0, std::min(45, int(std::lround(get(K + 3, a->cfg.patiencePool)))));
      a->cfg.oppCardFloor      = std::max(0.0, std::min(20.0, get(K + 4, a->cfg.oppCardFloor)));
      a->cfg.valueWeight       = std::max(0.0, get(K + 5, a->cfg.valueWeight));
      a->cfg.linearWeight      = std::max(0.0, get(K + 6, a->cfg.linearWeight));
      a->cfg.minTeamProb       = std::min(0.99, std::max(0.05, get(K + 7, a->cfg.minTeamProb)));
      a->cfg.declareMargin     = get(K + 8, a->cfg.declareMargin);
      a->cfg.priorTheta        = std::max(0.0, std::min(2.0, get(K + 9, a->cfg.priorTheta)));
      a->cfg.priorPhi          = std::max(0.0, std::min(1.0, get(K + 10, a->cfg.priorPhi)));
      a->cfg.searchTopK        = std::max(0, std::min(24, int(std::lround(get(K + 11, a->cfg.searchTopK)))));
      a->cfg.chainWeight       = std::max(0.0, get(K + 12, a->cfg.chainWeight));
      a->cfg.threatWeight      = std::max(0.0, get(K + 13, a->cfg.threatWeight));
    }
    auto wv = o.find("weights");
    if (wv != o.end()) {
      std::stringstream ws(wv->second); std::string tok; int i = 0;
      while (std::getline(ws, tok, '|') && i < NFEAT) a->cfg.w[i++] = atof(tok.c_str());
    }
    return a;
  }
  Baseline b = Baseline::Detective;
  bool known = false;
  if (base == "random") b = Baseline::Random;
  else if (base == "hunter") b = Baseline::Hunter;
  else if (base == "diversifier") b = Baseline::Diversifier;
  else if (base == "detective") b = Baseline::Detective;
  else if (base == "lockout") b = Baseline::Lockout;
  else if (base == "v02" || base == "fishbot_v02") b = Baseline::FishV02;
  else if (base == "bluffer") b = Baseline::Bluffer;
  else if (base == "v03" || base == "fishbot_v03") b = Baseline::FishV03;
  else if (base != "detective") {
    fprintf(stderr, "fish: unknown policy '%s'\n", base.c_str());
    std::exit(2);
  }
  (void)known;
  auto a = std::make_unique<BaselineAgent>(b);
  a->cfg.useCountConditioning = optI(o, "cond", 1) != 0;
  a->cfg.signalStrength   = optD(o, "signal", a->cfg.signalStrength);
  a->cfg.hitWeight        = optD(o, "hit", a->cfg.hitWeight);
  a->cfg.informationWeight= optD(o, "info", a->cfg.informationWeight);
  a->cfg.setProgressWeight= optD(o, "prog", a->cfg.setProgressWeight);
  a->cfg.teamControlWeight= optD(o, "team", a->cfg.teamControlWeight);
  a->cfg.targetEvidenceWeight = optD(o, "targ", a->cfg.targetEvidenceWeight);
  a->cfg.continuationWeight   = optD(o, "cont", a->cfg.continuationWeight);
  a->cfg.completionWeight     = optD(o, "comp", a->cfg.completionWeight);
  a->cfg.replyThreatWeight    = optD(o, "reply", a->cfg.replyThreatWeight);
  a->cfg.repeatSetWeight      = optD(o, "repeat", a->cfg.repeatSetWeight);
  a->cfg.declarationThreshold = optD(o, "thr", a->cfg.declarationThreshold);
  a->cfg.trailingDelta        = optD(o, "trail", a->cfg.trailingDelta);
  a->cfg.leadingDelta         = optD(o, "lead", a->cfg.leadingDelta);
  a->cfg.allocationSlack      = optD(o, "slack", a->cfg.allocationSlack);
  a->psychTells               = optI(o, "tells", 0) != 0;
  return a;
}

} // namespace fish
