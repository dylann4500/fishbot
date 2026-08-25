#!/usr/bin/env python3
"""FishBot v0.7 -- freeze one configuration, and prove the freeze round-trips.

WHAT IS DIFFERENT ABOUT THE v0.7 FREEZE.  v0.4, v0.5 and v0.6 froze a fitted
VECTOR, and their freeze scripts are source-code rewriters that edit the numeric
column of a `w[]` initialiser in place.  v0.7's candidate is not a fitted vector:
it is v0.6's own frozen vector plus one hand-set extended coordinate plus a set
of structural switches, so what has to be pinned is the whole CONFIGURATION --
the spec string -- and not a column of numbers.

That makes the round-trip assertion both possible and necessary, and it is
EXECUTED here rather than printed.  `freeze_config_v05.py` printed its check as
a comment and `freeze_config_v06.py` had none; both are recorded as defects of
this project's own convention (ledger P-2's register), and this script fixes it.

Three assertions, all run:

  R1  STRING round-trip.  The JSON stores the base and an ordered option map,
      never the concatenated string alone.  Rebuilding the spec from the map
      must reproduce the input spec character for character.  This is what makes
      the JSON, and not the string, the artifact.

  R2  VECTOR round-trip, and what writing it found.  The JSON also stores the
      explicit 55-coordinate `allparams` vector the configuration resolves to --
      v0.6's frozen NV6PARAM=37 followed by the eighteen v0.7 responder
      coordinates, with the spec's `rN=` keys applied.  Writing the assertion
      turned up a defect worth recording: **three of the frozen configuration's
      keys cannot be expressed in that vector at all.**  `factory.hpp:108-110`
      clamps the vector's askFloor to [0, 0.9], patiencePool to [0, 45] and
      oppCardFloor to [0, 20], while the frozen configuration sets all three to
      the sentinel -1 that switches the urgency escalation off.  Worse, the
      vector WAS applied after the individual keys, so a spec carrying both
      `allparams=` and `askfloor=-1` **silently discarded the sentinel**.  No
      phase-1, phase-2 or phase-3 artifact does that -- their fitted `.spec` files
      carry only options applied after allparams -- but phase 4's OWN cross-play
      fits did: every research/v07/runs/p4-xp*.spec carries all three sentinels AND
      an allparams=, and playing such a spec against itself-with-the-sentinels-
      deleted was an exact mirror, i.e. the switches were doing nothing and those
      runs had the urgency escalation ON.  The engine was fixed -- factory.hpp now
      re-applies the fourteen knob keys AFTER the allparams block, so an explicit
      key beats the bulk vector.  The fix leaves every phase-1/2/3 fitted spec
      bit-identical and leaves the v06 mirror digest unchanged, and it is why R2b
      below still finds the three sentinels unexpressible AS COORDINATES while they
      are now honoured AS KEYS.

      So R2 is split and both halves run:
        R2a  the vector reproduces the coordinates it CAN carry: the frozen spec
             with its three sentinel keys removed, played against the same spec
             with the vector supplied through `allparams` instead of through the
             baked V6PARAMS, must be an exact mirror.  This is what proves the
             JSON pins the policy independently of `src/v06.hpp`: if a later
             phase edits that block, R2a fails loudly instead of the frozen
             configuration silently drifting.
        R2b  the three sentinel keys are asserted to be outside the vector's
             clamped range, so the JSON records them as SWITCHES and not as
             parameters, and says why.

  R3  DIGEST round-trip.  The mirror pathology digest of the frozen spec is
      recorded in the JSON and recomputed on every run.  Any change to the
      engine that moves the frozen policy's play by one byte fails here.

  usage:  ./freeze_config_v07.py [--spec=SPEC] [--out=engine/fishbot_v07.json]
                                 [--note="..."] [--threads=N] [--verify-only]
"""
import hashlib, json, os, re, subprocess, sys, collections

ENG = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(ENG)
FISH = os.path.join(ENG, "fish7")

SPEC = ("v07:m2=0,r12=25,rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,"
        "stall=12,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26")
OUT = os.path.join(ENG, "fishbot_v07.json")
NOTE = ""
THREADS = "13"
VERIFY_ONLY = False
EXPLICIT_SPEC = False
for a in sys.argv[1:]:
    if a.startswith("--spec="): SPEC = a.split("=", 1)[1]; EXPLICIT_SPEC = True
    elif a.startswith("--out="): OUT = a.split("=", 1)[1]
    elif a.startswith("--note="): NOTE = a.split("=", 1)[1]
    elif a.startswith("--threads="): THREADS = a.split("=", 1)[1]
    elif a == "--verify-only": VERIFY_ONLY = True

# The module default above is a fallback for a FIRST freeze only.  Once the
# artifact exists it is the authority: verifying against a constant that has
# drifted from the artifact is the failure mode this script exists to catch, and
# it bit once already when `m2=0` was dropped from the freeze and the constant was
# not updated with it.
if not EXPLICIT_SPEC and os.path.exists(OUT):
    try: SPEC = json.load(open(OUT))["spec"]
    except Exception: pass

def sh(*args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("freeze_config_v07: command failed (%d): %s\n%s" % (r.returncode, " ".join(args), r.stderr))
    return r.stdout

# ---- parse the spec exactly as engine/src/factory.hpp parseOpts does --------
def parse_spec(spec):
    if ":" not in spec:
        return spec, collections.OrderedDict()
    base, rest = spec.split(":", 1)
    o = collections.OrderedDict()
    for item in rest.split(","):
        if "=" not in item:      # parseOpts drops these silently; so do we, loudly
            sys.exit("freeze_config_v07: option %r has no '=' and factory.hpp would drop it" % item)
        k, v = item.split("=", 1)
        if k in o:
            sys.exit("freeze_config_v07: option %r appears twice; std::map keeps the LAST" % k)
        o[k] = v
    return base, o

def build_spec(base, opts):
    if not opts: return base
    return base + ":" + ",".join("%s=%s" % (k, v) for k, v in opts.items())

BASE, OPTS = parse_spec(SPEC)

# ---- R1: string round-trip -------------------------------------------------
rebuilt = build_spec(BASE, OPTS)
assert rebuilt == SPEC, "R1 FAILED: %r != %r" % (rebuilt, SPEC)

# ---- resolve the explicit 55-coordinate vector -----------------------------
v06 = open(os.path.join(ENG, "src/v06.hpp")).read()
blk = re.search(r"V6PARAMS\[NV6PARAM\] = \{(.*?)\};", v06, re.S)
if not blk: sys.exit("freeze_config_v07: could not find V6PARAMS in src/v06.hpp")
V6 = [float(x) for x in re.findall(r"-?\d+\.\d+", blk.group(1))]
NR7 = 18
if len(V6) != 37: sys.exit("freeze_config_v07: V6PARAMS has %d entries, expected 37" % len(V6))
RW = [0.0] * NR7
for k, v in OPTS.items():
    m = re.fullmatch(r"r(\d+)", k)
    if m:
        i = int(m.group(1))
        if not (0 <= i < NR7): sys.exit("freeze_config_v07: r%d is out of range" % i)
        RW[i] = float(v)
VEC = V6 + RW
VECSTR = "|".join("%.5f" % x for x in VEC)

# The vector form carries every option EXCEPT the rN= keys, which allparams
# now supplies.  weightSpec (tuner.hpp:158) continues an existing option list
# with a comma rather than opening a second colon; the same rule applies here.
vopts = collections.OrderedDict((k, v) for k, v in OPTS.items() if not re.fullmatch(r"r\d+", k))
vopts["allparams"] = VECSTR
VECSPEC = build_spec(BASE, vopts)

# ---- R2b: which options the vector provably cannot carry ------------------
# factory.hpp:108-110.  A sentinel outside the clamp is a SWITCH, not a value.
CLAMPS = {"askfloor": (0.0, 0.9, "factory.hpp:108"),
          "pool":     (0.0, 45.0, "factory.hpp:109"),
          "oppfloor": (0.0, 20.0, "factory.hpp:110")}
SENTINEL = {}
for k, (lo, hi, where) in CLAMPS.items():
    if k in OPTS:
        v = float(OPTS[k])
        if v < lo or v > hi:
            SENTINEL[k] = {"value": OPTS[k], "clampedRange": [lo, hi], "clampedAt": where,
                           "note": "outside the range `allparams` clamps this coordinate to, so it "
                                   "is a switch and cannot be expressed as a vector coordinate"}
# Any option the vector cannot carry AND that is applied before allparams would be
# silently discarded if both were present in one spec.  Assert we are not doing that.
assert "allparams" not in OPTS, "R2b FAILED: the frozen spec must not itself carry allparams"

# ---- R2a: vector round-trip on the coordinates the vector CAN carry -------
# Run TWICE: once with the search off and once with it on.
#
# BLUEPRINT (search off) is the assertion that actually tests the vector, and it
# is exact: every paired quantity equal, deal-clustered interval collapsed to a
# point.  If a later phase edits src/v06.hpp's V6PARAMS block, this fails loudly
# instead of the frozen configuration silently drifting.
#
# SEARCH ON is reported and NOT asserted to zero, because it cannot be.  The same
# policy reached by two code paths -- the baked V6PARAMS and an `allparams=`
# string of the identical numbers -- diverges on a handful of decisions in 800
# games, and only when the search is engaged.  That is the same phenomenon
# RESEARCH-LOG section 4.3 measures as the S6 residual: roughly one searched ask
# decision in 10^5 is not reproducible from what the corpus believes is the whole
# of its input.  Asserting exactness here would therefore fail for a reason that
# has nothing to do with the freeze, and hiding the divergence would waste the
# one place it can be seen without the side-channel harness.
SEARCHKEYS = [k for k in ("s1", "det", "cand", "kappa", "rbelief", "depth", "maxq") if k in OPTS]
core = collections.OrderedDict((k, v) for k, v in OPTS.items() if k not in SENTINEL)
def forms(drop_search):
    a = collections.OrderedDict((k, v) for k, v in core.items()
                                if not (drop_search and k in SEARCHKEYS))
    b = collections.OrderedDict((k, v) for k, v in a.items() if not re.fullmatch(r"r\d+", k))
    b["allparams"] = VECSTR
    return build_spec(BASE, a), build_spec(BASE, b)

def cell(sa, sb, deals):
    return json.loads(sh(FISH, "match", "--a=" + sa, "--b=" + sb,
                         "--games=%d" % deals, "--rotations=2", "--seed=7030003",
                         "--threads=" + THREADS, "--json").strip().splitlines()[-1])

bpA, bpB = forms(True)
jb = cell(bpA, bpB, 400)
exact = (jb["winRateA"] == 0.5 and jb["ci"] == [0.5, 0.5]
         and jb["meanSetsA"] == jb["meanSetsB"] and jb["askAccA"] == jb["askAccB"]
         and jb["declAccA"] == jb["declAccB"] and jb["declPerGameA"] == jb["declPerGameB"])
if not exact:
    sys.exit("R2a FAILED (blueprint): the spec form and the vector form are not the same policy "
             "with the search off.\n  winRate %r ci %r sets %r/%r ask %r/%r decl %r/%r\n"
             "  The frozen JSON does not pin the configuration."
             % (jb["winRateA"], jb["ci"], jb["meanSetsA"], jb["meanSetsB"],
                jb["askAccA"], jb["askAccB"], jb["declAccA"], jb["declAccB"]))

R2A_SEARCH = None
if SEARCHKEYS:
    sA, sB = forms(False)
    js = cell(sA, sB, 400)
    R2A_SEARCH = {"winRateA": js["winRateA"], "games": js["games"],
                  "setsA": js["meanSetsA"], "setsB": js["meanSetsB"],
                  "askAccA": js["askAccA"], "askAccB": js["askAccB"],
                  "identical": (js["meanSetsA"] == js["meanSetsB"]
                                and js["askAccA"] == js["askAccB"])}
specA, specB = forms(False)

# ---- R3: digest round-trip -------------------------------------------------
dig = sh(FISH, "pathology", "--a=" + SPEC, "--b=" + SPEC,
         "--games=400", "--rotations=2", "--seed=31", "--threads=2")
DIGMD5 = hashlib.md5(dig.encode()).hexdigest()

def sha(path, n=16):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()[:n]

srcs = sorted(f for f in os.listdir(os.path.join(ENG, "src")) if f.endswith((".hpp", ".cpp")))
commit = sh("git", "-C", ROOT, "rev-parse", "HEAD").strip()
dirty = bool(sh("git", "-C", ROOT, "status", "--porcelain").strip())

doc = collections.OrderedDict()
doc["name"] = "fishbot_v07"
doc["version"] = "0.7"
doc["spec"] = SPEC
doc["base"] = BASE
doc["options"] = OPTS
doc["allparamsSpec"] = specB
doc["switchesNotExpressibleAsVector"] = SENTINEL
doc["allparams"] = VEC
doc["allparamsLayout"] = {"nfeat": 20, "v05knobs": 14, "v06askTerms": 3, "v07responder": NR7,
                          "total": len(VEC),
                          "note": "the flat `allparams` vector: NFEAT ask weights, the fourteen "
                                  "v0.5 knobs, the three v0.6 ask terms, then the eighteen v0.7 "
                                  "responder coordinates (v07_responder.hpp r7Name)"}
doc["inheritedVectorProvenance"] = re.search(r'V6FIT_PROVENANCE = "(.*?)"', v06).group(1)
doc["provenance"] = {"commit": commit, "treeDirty": dirty, "note": NOTE,
                     "srcSha256_16": {f: sha(os.path.join(ENG, "src", f)) for f in srcs}}
doc["roundTrip"] = {
    "R1_string": "PASS -- the spec rebuilt from base+options is character-identical",
    "R2a_vector_blueprint": "PASS -- with the search off and the sentinel switches removed from both "
                            "arms, the spec form and the allparams form play identically on every deal "
                            "of an 800-game cell on bank 7030003",
    "R2a_vector_search": R2A_SEARCH,
    "R2b_switches": sorted(SENTINEL),
    "R3_digestMd5": DIGMD5,
    "R3_command": "fish7 pathology --a=<spec> --b=<spec> --games=400 --rotations=2 --seed=31 --threads=2",
}

if VERIFY_ONLY:
    if not os.path.exists(OUT): sys.exit("freeze_config_v07: --verify-only but %s does not exist" % OUT)
    old = json.load(open(OUT))
    bad = []
    if old["spec"] != SPEC: bad.append("spec (artifact %r vs checked %r)" % (old["spec"], SPEC))
    if old["allparams"] != VEC: bad.append("allparams")
    if old["roundTrip"]["R3_digestMd5"] != DIGMD5: bad.append("R3 digest")
    # R4, source drift.  The 78 sha256 prefixes exist to detect exactly this and
    # were not being checked at all, which made them decoration.  A changed source
    # is not automatically a failure -- most of the tree does not touch the frozen
    # policy, and this phase changed several files while leaving it byte-identical
    # -- so they are REPORTED; only a changed digest or vector is fatal.
    was = old.get("provenance", {}).get("srcSha256_16", {})
    now = {f: sha(os.path.join(ENG, "src", f)) for f in srcs}
    moved = sorted(k for k in set(was) | set(now) if was.get(k) != now.get(k))
    if bad:
        sys.exit("VERIFY FAILED: %s differ from the frozen artifact%s"
                 % ("; ".join(bad),
                    ("\n  engine sources changed since the freeze: " + ", ".join(moved)) if moved else ""))
    print("VERIFY PASS  %s\n  spec   %s\n  digest %s" % (OUT, SPEC, DIGMD5))
    if moved:
        print("  NOTE   %d engine source(s) changed since the freeze and the frozen policy still\n"
              "         plays identically (R3 digest matched): %s" % (len(moved), ", ".join(moved)))
    sys.exit(0)

open(OUT, "w").write(json.dumps(doc, indent=2) + "\n")
print("FROZEN -> %s" % OUT)
print("  spec       %s" % SPEC)
print("  R1 string  PASS")
print("  R2a blueprint PASS  (exact, spec form vs %d-coordinate allparams form, search off,\n"
      "                sentinel switches %s removed from both arms)"
      % (len(VEC), ", ".join(sorted(SENTINEL)) or "(none)"))
if R2A_SEARCH is not None:
    print("  R2a search    %s  (same two forms with the search ON: sets %.5f/%.5f, ask %.6f/%.6f\n"
          "                over %d games -- reported, not asserted; see RESEARCH-LOG 4.3)"
          % ("identical" if R2A_SEARCH["identical"] else "DIVERGES",
             R2A_SEARCH["setsA"], R2A_SEARCH["setsB"],
             R2A_SEARCH["askAccA"], R2A_SEARCH["askAccB"], R2A_SEARCH["games"]))
print("  R2b switch %s cannot be expressed as vector coordinates (clamped); recorded as switches"
      % (", ".join("`%s=%s`" % (k, SENTINEL[k]["value"]) for k in sorted(SENTINEL)) or "(none)"))
print("  R3 digest  %s" % DIGMD5)
print("  commit     %s%s" % (commit, "  (TREE DIRTY)" if dirty else ""))
