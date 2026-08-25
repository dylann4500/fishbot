// Policy specification parser: "name" or "name:key=value,key=value".
#pragma once
#include "baselines.hpp"
#include "v04.hpp"
#include "v05.hpp"
#include "v06.hpp"
#include "v07_responder.hpp"
#include "v07_adapt.hpp"
#include "v07_cheat.hpp"    // phase 3: planted side-channel cheats (probe-only specs)
#include "v07_learn.hpp"   // phase 3 K5: the amortised (learned) policy
#include "probe_deception.hpp"   // appended: P3 deception archetypes
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

// Shared option application for the v0.5 configuration.  v0.6 derives from
// V05Agent and accepts every v0.5 knob, so the parsing lives in one place.
inline void applyV05Opts(V05Config& c, const std::map<std::string, std::string>& o) {
        auto it = o.find("belief");
    if (it != o.end()) {
      if (it->second == "exact") c.belief = BeliefMode::Exact;
      else if (it->second == "exactdisj") c.belief = BeliefMode::ExactDisj;
      else if (it->second == "sinkhorn") c.belief = BeliefMode::Sinkhorn;
      else if (it->second == "indep") c.belief = BeliefMode::Independent;
      else if (it->second == "hybrid") c.belief = BeliefMode::Hybrid;
      else if (it->second == "fast") c.belief = BeliefMode::Fast;
      else if (it->second == "block") c.belief = BeliefMode::Block;
    }
    // v0.6 E3: `gateaudit` was parsed only in the v0.4 branch, so
    // `fish gateaudit --a=v05:gateaudit=1` returned a vacuous PASS over zero
    // opportunities while the equivalent v0.4 audit did not pass.
    c.gateAudit         = optI(o, "gateaudit", c.gateAudit ? 1 : 0) != 0;
    c.particles         = optI(o, "particles", c.particles);
    c.declThreshold     = optD(o, "decl", c.declThreshold);
    c.lockedAllocThresh = optD(o, "lockthr", c.lockedAllocThresh);
    c.minTeamProb       = optD(o, "minteam", c.minTeamProb);
    c.patientLocked     = optI(o, "patient", c.patientLocked ? 1 : 0) != 0;
    c.askFloor          = optD(o, "askfloor", c.askFloor);
    c.patiencePool      = optI(o, "pool", c.patiencePool);
    c.forceDeclareEvents= optI(o, "force", c.forceDeclareEvents);
    c.oppCardFloor      = optD(o, "oppfloor", c.oppCardFloor);
    // v0.7 phase 3 (K3).  Default 0 = off; with it off nothing in v07_stall.hpp
    // is ever called and the binary is bit-identical to the reference.
    c.stallEvents       = optI(o, "stall",     c.stallEvents);
    c.stallStage2       = optI(o, "stall2",    c.stallStage2);
    c.stallSoft         = optI(o, "stallsoft", c.stallSoft ? 1 : 0) != 0;
    if (c.stallEvents > 0) {
      auto& S = k3stall();
      S.on.store(true, std::memory_order_relaxed);
      S.K.store(c.stallEvents, std::memory_order_relaxed);
      S.K2.store(c.stallStage2 > 0 ? c.stallStage2 : 2 * c.stallEvents, std::memory_order_relaxed);
      S.soft.store(c.stallSoft ? 1 : 0, std::memory_order_relaxed);
    }
    c.gateTeamProb      = optD(o, "gate", c.gateTeamProb);
    c.marginalGate      = optD(o, "mgate", c.marginalGate);
    c.sinkOuter         = optI(o, "souter", c.sinkOuter);
    c.sinkInner         = optI(o, "sinner", c.sinkInner);
    c.useValue          = optI(o, "value", c.useValue ? 1 : 0) != 0;
    c.valueWeight       = optD(o, "vweight", c.valueWeight);
    c.linearWeight      = optD(o, "lweight", c.linearWeight);
    c.valueDeclare      = optI(o, "vdecl", c.valueDeclare ? 1 : 0) != 0;
    c.declareMargin     = optD(o, "vmargin", c.declareMargin);
    c.priorTheta        = optD(o, "ptheta", c.priorTheta);
    c.priorPhi          = optD(o, "pphi", c.priorPhi);
    c.greedyMAP         = optI(o, "gmap", c.greedyMAP ? 1 : 0) != 0;
    c.searchTopK        = optI(o, "topk", c.searchTopK);
    c.chainWeight       = optD(o, "chain", c.chainWeight);
    c.threatWeight      = optD(o, "threat", c.threatWeight);
    c.declareEnabled    = optI(o, "declare", 1) != 0;
    // Flat parameter vector for the optimiser, identical in layout to v0.4's so
    // a v0.4 vector can seed a v0.5 fit.  The offset is derived from NFEAT, not
    // hard-coded -- that aliasing bug cost v0.4 a whole fitting round.
    auto ap5 = o.find("allparams");
    if (ap5 != o.end()) {
      std::vector<double> v;
      std::stringstream ws(ap5->second); std::string tok;
      while (std::getline(ws, tok, '|')) v.push_back(atof(tok.c_str()));
      for (int i = 0; i < NFEAT && i < (int)v.size(); i++) c.w[i] = v[i];
      auto get = [&](size_t i, double d) { return i < v.size() ? v[i] : d; };
      const size_t K = size_t(NFEAT);
      c.declThreshold     = std::min(0.9999, std::max(0.5, get(K + 0, c.declThreshold)));
      c.lockedAllocThresh = std::min(0.99999, std::max(0.5, get(K + 1, c.lockedAllocThresh)));
      c.askFloor          = std::min(0.9, std::max(0.0, get(K + 2, c.askFloor)));
      c.patiencePool      = std::max(0, std::min(45, int(std::lround(get(K + 3, c.patiencePool)))));
      c.oppCardFloor      = std::max(0.0, std::min(20.0, get(K + 4, c.oppCardFloor)));
      c.valueWeight       = std::max(0.0, get(K + 5, c.valueWeight));
      c.linearWeight      = std::max(0.0, get(K + 6, c.linearWeight));
      c.minTeamProb       = std::min(0.99, std::max(0.05, get(K + 7, c.minTeamProb)));
      c.declareMargin     = get(K + 8, c.declareMargin);
      c.priorTheta        = std::max(0.0, std::min(2.0, get(K + 9, c.priorTheta)));
      c.priorPhi          = std::max(0.0, std::min(1.0, get(K + 10, c.priorPhi)));
      c.searchTopK        = std::max(0, std::min(24, int(std::lround(get(K + 11, c.searchTopK)))));
      c.chainWeight       = std::max(0.0, get(K + 12, c.chainWeight));
      c.threatWeight      = std::max(0.0, get(K + 13, c.threatWeight));
      // v0.7 phase 4.  AN EXPLICIT KEY MUST BEAT THE BULK VECTOR.  The fourteen
      // knobs above are also settable individually, and until now those keys were
      // applied BEFORE this block, so `allparams=` silently overwrote them.  Three
      // of them are worse than overwritten: `askfloor`, `pool` and `oppfloor` are
      // set to the SENTINEL -1 to switch the urgency escalation off, and the clamps
      // three lines up ([0,0.9], [0,45], [0,20]) cannot express -1 at all, so the
      // switch was silently discarded and the escalation came back on.
      //
      // Found by re-reading engine/p4_crossfit_v07.sh's own output: every
      // research/v07/runs/p4-xp*.spec carries `pool=-1,oppfloor=-1,askfloor=-1`
      // AND `allparams=`, and playing such a spec against itself-with-the-sentinels-
      // deleted is an exact mirror -- the sentinels were doing nothing.  No phase
      // 1, 2 or 3 artifact carries both (checked over the whole tree), so this
      // changes the meaning of no committed result before phase 4.
      c.declThreshold     = optD(o, "decl",     c.declThreshold);
      c.lockedAllocThresh = optD(o, "lockthr",  c.lockedAllocThresh);
      c.askFloor          = optD(o, "askfloor", c.askFloor);
      c.patiencePool      = optI(o, "pool",     c.patiencePool);
      c.oppCardFloor      = optD(o, "oppfloor", c.oppCardFloor);
      c.valueWeight       = optD(o, "vweight",  c.valueWeight);
      c.linearWeight      = optD(o, "lweight",  c.linearWeight);
      c.minTeamProb       = optD(o, "minteam",  c.minTeamProb);
      c.declareMargin     = optD(o, "vmargin",  c.declareMargin);
      c.priorTheta        = optD(o, "ptheta",   c.priorTheta);
      c.priorPhi          = optD(o, "pphi",     c.priorPhi);
      c.searchTopK        = optI(o, "topk",     c.searchTopK);
      c.chainWeight       = optD(o, "chain",    c.chainWeight);
      c.threatWeight      = optD(o, "threat",   c.threatWeight);
    }
    // v0.5 mechanism switches, for the ablation table.
    c.liveAskGate       = optI(o, "m1", c.liveAskGate ? 1 : 0) != 0;
    c.ownershipByP      = optI(o, "m1p", c.ownershipByP ? 1 : 0) != 0;
    c.feasibleDecl      = optI(o, "m2", c.feasibleDecl ? 1 : 0) != 0;
    // v0.7 phase 3 (K2), ledger L1.  Default OFF; `jalloc=0` is today's code.
    c.jalloc            = optI(o, "jalloc", c.jalloc ? 1 : 0) != 0;
    c.jallocTopM        = optI(o, "jtopm", c.jallocTopM);
    c.l1ReplayTopM      = optI(o, "l1topm", c.l1ReplayTopM);
    // ---- v0.7 planted weakness ---------------------------------------------
    // `hcap` names the family and `hstr` its size.  hit/decl/prior are pure
    // config transforms (no policy code runs for them); leak/tell/gate set
    // V05Config::plantKind.  Applied AFTER `allparams`, so a handicap survives
    // a fitted vector being loaded on top of it.
    { auto hit2 = o.find("hcap");
      if (hit2 != o.end()) {
        double xs = optD(o, "hstr", 1.0);
        const std::string& h = hit2->second;
        if      (h == "hit")   c.w[0] *= (1.0 - xs);
        else if (h == "decl")  c.declareMargin += 0.5 * xs;
        else if (h == "prior") { c.priorTheta *= (1.0 - xs); c.priorPhi *= (1.0 - xs); }
        else if (h == "leak")  { c.plantKind = 1; c.plantStr = xs; }
        else if (h == "tell")  { c.plantKind = 2; c.plantStr = xs; }
        else if (h == "gate")  { c.plantKind = 3; c.plantStr = xs; }
        else if (h == "none")  { }
        else { fprintf(stderr, "fish: unknown handicap '%s'\n", h.c_str()); std::exit(2); }
      } }
    c.forceStage2       = optI(o, "stage2", c.forceStage2 ? 1 : 0) != 0;
    c.repeatGuard       = optI(o, "norepeat", c.repeatGuard ? 1 : 0) != 0;
    for (int i = 0; i < NFEAT; i++) {
      char key[8]; snprintf(key, sizeof(key), "w%d", i);
      c.w[i] = optD(o, key, c.w[i]);
    }
    for (int i = 0; i < NVFEAT; i++) {
      char key[10]; snprintf(key, sizeof(key), "v%d", i);
      c.vw[i] = optD(o, key, c.vw[i]);
    }
    auto wv = o.find("weights");
    if (wv != o.end()) {
      std::stringstream ws(wv->second); std::string tok; int i = 0;
      while (std::getline(ws, tok, '|') && i < NFEAT) c.w[i++] = atof(tok.c_str());
    }
    auto vv2 = o.find("vweights");
    if (vv2 != o.end()) {
      std::stringstream vs(vv2->second); std::string tok; int i = 0;
      while (std::getline(vs, tok, '|') && i < NVFEAT) c.vw[i++] = atof(tok.c_str());
    }
}

// Shared v0.6 option application, so the v0.7 classes accept every v0.6 knob
// without the parsing being copied.  Split out of makeAgent verbatim.
inline void applyV06Opts(V06Agent* a, const std::map<std::string, std::string>& o) {
    if (optI(o, "legacy", 0)) { V05Config d; a->cfg = d; a->x.wVoid = a->x.wTeamHas = a->x.wLastLive = 0.0; a->x.extraFeats = false; }
    applyV05Opts(a->cfg, o);
    { auto ap = o.find("allparams");
      if (ap != o.end()) {
        std::vector<double> v;
        std::stringstream ws(ap->second); std::string tok;
        while (std::getline(ws, tok, '|')) v.push_back(atof(tok.c_str()));
        const size_t K6 = size_t(NFEAT) + 14;
        auto get = [&](size_t i, double d) { return i < v.size() ? v[i] : d; };
        if (v.size() > K6) {
          a->x.wVoid    = get(K6 + 0, a->x.wVoid);
          a->x.wTeamHas = get(K6 + 1, a->x.wTeamHas);
          a->x.wLastLive= get(K6 + 2, a->x.wLastLive);
          a->x.extraFeats = true;
        }
      } }
    a->x.search      = optI(o, "s1", a->x.search ? 1 : 0) != 0;
    a->x.nDet        = optI(o, "det", a->x.nDet);
    a->x.topK        = optI(o, "cand", a->x.topK);
    a->x.maxCand     = optI(o, "maxcand", a->x.maxCand);
    a->x.maxDepth    = optI(o, "depth", a->x.maxDepth);
    a->x.leafLambda  = optD(o, "leaf", a->x.leafLambda);
    a->x.blend       = optD(o, "blend", a->x.blend);
    a->x.kappa       = optD(o, "kappa", a->x.kappa);
    a->x.kappaTie    = optD(o, "kappatie", a->x.kappaTie);
    a->x.tieOnly     = optI(o, "tieonly", a->x.tieOnly ? 1 : 0) != 0;
    a->x.searchMaxQ  = optI(o, "maxq", a->x.searchMaxQ);
    a->x.exactTie    = optI(o, "extie", a->x.exactTie ? 1 : 0) != 0;
    a->x.exactP      = optI(o, "exactp", a->x.exactP ? 1 : 0) != 0;
    a->x.exactMaxQ   = optI(o, "exmaxq", a->x.exactMaxQ);
    a->x.extraFeats  = optI(o, "xf", a->x.extraFeats ? 1 : 0) != 0;
    a->x.wVoid       = optD(o, "wvoid", a->x.wVoid);
    a->x.wTeamHas    = optD(o, "wteam", a->x.wTeamHas);
    a->x.wLastLive   = optD(o, "wlast", a->x.wLastLive);
    { int rt = optI(o, "rtie", a->x.randomTie ? (a->x.tieIndep ? 2 : 1) : 0);
      a->x.randomTie = rt != 0;
      a->x.tieIndep  = rt >= 2; }   // K3: rtie=2 is the private per-seat stream
    a->x.chainPass   = optI(o, "chain2", a->x.chainPass ? 1 : 0) != 0;
    a->x.deadAsk     = optI(o, "dead", a->x.deadAsk ? 1 : 0) != 0;
    a->x.deadMargin  = optD(o, "deadmargin", a->x.deadMargin);
    a->x.deadBudget  = optI(o, "deadbudget", a->x.deadBudget);
    a->x.deadInSearch= optI(o, "deadsearch", a->x.deadInSearch);
    a->x.searchFrom  = optI(o, "from", a->x.searchFrom);
    a->x.policyPrior = optI(o, "detprior", a->x.policyPrior ? 1 : 0) != 0;
    a->x.minGap      = optI(o, "mingap", a->x.minGap);
    a->x.gapEps      = optD(o, "gapeps", a->x.gapEps);
    { auto it = o.find("roll"); if (it != o.end()) a->x.rollBase = it->second; }
    { auto it = o.find("rbelief"); if (it != o.end()) a->x.rollBelief = it->second; }
    { auto it = o.find("roppo"); if (it != o.end()) {
        std::string t = it->second;
        for (auto& ch : t) if (ch == '+') ch = ',';
        a->x.rollOpp = t; } }
    { auto it = o.find("leafeval"); if (it != o.end()) a->x.leafSpec = it->second; }
    a->x.rollOuter = optI(o, "rsouter", a->x.rollOuter);
    a->x.rollInner = optI(o, "rsinner", a->x.rollInner);
    a->x.rollValue = optI(o, "rvalue", a->x.rollValue ? 1 : 0) != 0;
    // K3: rollout-agent hygiene.  Default 0 == the shipped, measured behaviour.
    a->x.rollReset = optI(o, "rreset", a->x.rollReset ? 1 : 0) != 0;
}

inline std::unique_ptr<Agent> makeAgent(const std::string& spec) {
  std::string base;
  auto o = parseOpts(spec, base);
  // ---- v0.7 responder classes ------------------------------------------
  if (base == "v07" || base == "v07r") {
    auto a = std::make_unique<V07Responder>();
    applyV06Opts(a.get(), o);
    a->x.extraFeats = true;              // the widened score is always live
    // The flat vector extends v0.6's by NR7 coordinates.  With those sixteen at
    // zero the class is v0.6 bit for bit, which is the identity control; a
    // phase-1 vector carrying only the first twelve loads unchanged and leaves
    // the four phase-2 information-denial coordinates at zero, so every phase-1
    // fitted responder still reproduces itself under this build.
    { auto ap = o.find("allparams");
      if (ap != o.end()) {
        std::vector<double> v;
        std::stringstream ws(ap->second); std::string tok;
        while (std::getline(ws, tok, '|')) v.push_back(atof(tok.c_str()));
        const size_t K7 = size_t(NFEAT) + 14 + 3;
        for (int i = 0; i < NR7; i++) if (v.size() > K7 + size_t(i)) a->rw[i] = v[K7 + size_t(i)];
      } }
    for (int i = 0; i < NR7; i++) {
      char key[8]; snprintf(key, sizeof(key), "r%d", i);
      a->rw[i] = optD(o, key, a->rw[i]);
    }
    a->admitDead = optI(o, "dead7", a->admitDead ? 1 : 0) != 0;
    a->deadCap   = optI(o, "deadcap", a->deadCap);
    a->corrPlans = optI(o, "corr", a->corrPlans);
    return a;
  }
  // C6, the scripted-adaptive class (v07_adapt.hpp).  `hold=0` reduces it to
  // v0.6 bit for bit, which is the identity control.
  if (base == "v07c") {
    auto a = std::make_unique<V07AdaptAgent>();
    applyV06Opts(a.get(), o);
    a->holdOn   = optI(o, "hold", a->holdOn);
    a->holdMax  = optI(o, "holdmax", a->holdMax);
    a->lockProb = optD(o, "lockp", a->lockProb);
    a->ambigMin = optI(o, "ambig", a->ambigMin);
    a->dumpOn   = optI(o, "dump", a->dumpOn);
    a->mode     = optI(o, "mode", a->mode);
    a->aggrMargin = optD(o, "aggr", a->aggrMargin);
    return a;
  }
  // K5, the amortised search (v07_learn.hpp).  With no `lw=` the model is off
  // and chooseAsk defers to V06Agent, which is the identity control.
  if (base == "v07l") {
    auto a = std::make_unique<V07LAgent>();
    applyV06Opts(a.get(), o);
    { auto it = o.find("lw");
      if (it != o.end()) {
        std::string t = it->second;
        for (auto& ch : t) if (ch == '+') ch = '|';   // spec commas/pipes are taken
        if (!v07learn::parseModel(t, a->lm)) {
          fprintf(stderr, "fish: v07l: could not parse lw=\n"); std::exit(2);
        }
      } }
    a->lm.margin  = optD(o, "lmargin", a->lm.margin);
    a->lm.tieOnly = optI(o, "ltie", a->lm.tieOnly ? 1 : 0) != 0;
    a->lm.maxQ    = optI(o, "lmaxq", a->lm.maxQ);
    return a;
  }
  if (base == "v07i") {
    auto a = std::make_unique<V07InvertAgent>();
    applyV06Opts(a.get(), o);
    a->invOn        = optI(o, "inv", 1) != 0;
    a->invClip      = optD(o, "iclip", a->invClip);
    a->inv.nDet     = optI(o, "idet", a->inv.nDet);
    a->inv.gain     = optD(o, "igain", a->inv.gain);
    a->inv.alpha    = optD(o, "ialpha", a->inv.alpha);
    a->inv.fromEvent= optI(o, "ifrom", a->inv.fromEvent);
    a->inv.maxQ     = optI(o, "imaxq", a->inv.maxQ);
    a->inv.kappa    = optD(o, "ikappa", a->inv.kappa);
    a->inv.stepClip = optD(o, "istep", a->inv.stepClip);
    a->inv.mode     = optI(o, "imode", a->inv.mode);
    a->inv.focus    = optI(o, "ifocus", a->inv.focus);
    { auto it = o.find("imodel");
      std::string t = it != o.end() ? it->second : std::string("v06");
      for (auto& ch : t) if (ch == '+') ch = ',';
      a->inv.oracle.spec = t; }
    return a;
  }
  // ---- v0.7 phase 3: PROBE-ONLY cheats -----------------------------------
  // Positive controls for `fish7 v7side`.  These deliberately violate T8 and
  // exist so the side-channel gate can be shown to FAIL something.  The `v07x`
  // base is produced by no tuner, battery or frozen vector in the corpus, so it
  // is unreachable from an ordinary spec string.
  if (base == "v07x") {
    std::string ck = "none";
    { auto it = o.find("cheat"); if (it != o.end()) ck = it->second; }
    if (ck == "seed") {
      auto a = std::make_unique<CheatSeedAgent>(); applyV06Opts(a.get(), o); return a;
    }
    if (ck == "shared") {
      auto a = std::make_unique<CheatSharedAgent>(); applyV06Opts(a.get(), o); return a;
    }
    if (ck == "conv") {
      auto a = std::make_unique<CheatConvAgent>(); applyV06Opts(a.get(), o); return a;
    }
    if (ck == "none") {   // the identity arm: v0.6 exactly, under the probe name
      auto a = std::make_unique<V06Agent>(); applyV06Opts(a.get(), o); return a;
    }
    fprintf(stderr, "fish: unknown cheat '%s'\n", ck.c_str()); std::exit(2);
  }
  if (base == "v06" || base == "fishbot_v06") {
    auto a = std::make_unique<V06Agent>();
    applyV06Opts(a.get(), o);
    return a;
  }
  if (base == "v05" || base == "fishbot_v05") {
    auto a = std::make_unique<V05Agent>();
    applyV05Opts(a->cfg, o);
    return a;
  }

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
    a->cfg.gateAudit         = optI(o, "gateaudit", a->cfg.gateAudit ? 1 : 0) != 0;
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
  // --- appended (P3): deceptive archetypes, see probe_deception.hpp ---------
  if (base == "silent" || base == "feint" || base == "withholder") {
    auto a = std::make_unique<DeceptiveAgent>();
    a->style = base == "silent" ? DeceitStyle::Silent
             : base == "feint"  ? DeceitStyle::Feint
                                : DeceitStyle::Withholder;
    a->labelStr = base == "silent" ? "silent" : (base == "feint" ? "feint" : "withholder");
    a->cooldownK = optI(o, "k", a->cooldownK);
    a->feintTol  = optD(o, "tol", a->feintTol);
    a->silentTol = optD(o, "tol", a->silentTol);
    a->cfg.priorTheta = optD(o, "ptheta", a->cfg.priorTheta);
    a->cfg.priorPhi   = optD(o, "pphi", a->cfg.priorPhi);
    a->cfg.declareEnabled = optI(o, "declare", 1) != 0;
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
