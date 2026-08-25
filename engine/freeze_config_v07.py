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
      vector is applied at `factory.hpp:98` AFTER the individual keys at
      `factory.hpp:63-67`, so a spec carrying both `allparams=` and `askfloor=-1`
      **silently discards the sentinel**.  No committed artifact in this corpus
      does that -- every fitted `.spec` carries only options applied after
      allparams -- but a future one could, and it would fail silently.

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
for a in sys.argv[1:]:
    if a.startswith("--spec="): SPEC = a.split("=", 1)[1]
    elif a.startswith("--out="): OUT = a.split("=", 1)[1]
    elif a.startswith("--note="): NOTE = a.split("=", 1)[1]
    elif a.startswith("--threads="): THREADS = a.split("=", 1)[1]
    elif a == "--verify-only": VERIFY_ONLY = True

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
core = collections.OrderedDict((k, v) for k, v in OPTS.items() if k not in SENTINEL)
specA = build_spec(BASE, core)
vopts = collections.OrderedDict((k, v) for k, v in core.items() if not re.fullmatch(r"r\d+", k))
vopts["allparams"] = VECSTR
specB = build_spec(BASE, vopts)
j = json.loads(sh(FISH, "match", "--a=" + specA, "--b=" + specB,
                  "--games=400", "--rotations=2", "--seed=7030003",
                  "--threads=" + THREADS, "--json").strip().splitlines()[-1])
# `power.mirror` is a STRING comparison of the two specs (v07_power.hpp), so it is
# false here by construction -- the whole point is that the two strings differ.
# The behavioural test is that every paired quantity coincides exactly and that the
# deal-clustered interval collapses to a point, which only happens when the two
# arms play the same moves on every deal.
same = (j["winRateA"] == 0.5
        and j["ci"] == [0.5, 0.5]
        and j["meanSetsA"] == j["meanSetsB"]
        and j["askAccA"] == j["askAccB"]
        and j["declAccA"] == j["declAccB"]
        and j["declPerGameA"] == j["declPerGameB"])
if not same:
    sys.exit("R2a FAILED: the spec form and the vector form are not the same policy.\n"
             "  winRate %r ci %r  sets %r/%r  ask %r/%r  decl %r/%r\n"
             "  The frozen JSON does not pin the configuration."
             % (j["winRateA"], j["ci"], j["meanSetsA"], j["meanSetsB"],
                j["askAccA"], j["askAccB"], j["declAccA"], j["declAccB"]))

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
    "R2a_vector": "PASS -- with the sentinel switches removed from both arms, the spec form and the "
                  "allparams form play identically on every deal at 800 games on bank 7030003",
    "R2b_switches": sorted(SENTINEL),
    "R3_digestMd5": DIGMD5,
    "R3_command": "fish7 pathology --a=<spec> --b=<spec> --games=400 --rotations=2 --seed=31 --threads=2",
}

if VERIFY_ONLY:
    if not os.path.exists(OUT): sys.exit("freeze_config_v07: --verify-only but %s does not exist" % OUT)
    old = json.load(open(OUT))
    bad = []
    if old["spec"] != SPEC: bad.append("spec")
    if old["allparams"] != VEC: bad.append("allparams")
    if old["roundTrip"]["R3_digestMd5"] != DIGMD5: bad.append("R3 digest")
    if bad: sys.exit("VERIFY FAILED: %s differ from the frozen artifact" % ", ".join(bad))
    print("VERIFY PASS  %s\n  spec   %s\n  digest %s" % (OUT, SPEC, DIGMD5))
    sys.exit(0)

open(OUT, "w").write(json.dumps(doc, indent=2) + "\n")
print("FROZEN -> %s" % OUT)
print("  spec       %s" % SPEC)
print("  R1 string  PASS")
print("  R2a vector PASS (exact mirror, spec form vs %d-coordinate allparams form,\n"
      "             with the sentinel switches %s removed from both arms)"
      % (len(VEC), ", ".join(sorted(SENTINEL)) or "(none)"))
print("  R2b switch %s cannot be expressed as vector coordinates (clamped); recorded as switches"
      % (", ".join("`%s=%s`" % (k, SENTINEL[k]["value"]) for k in sorted(SENTINEL)) or "(none)"))
print("  R3 digest  %s" % DIGMD5)
print("  commit     %s%s" % (commit, "  (TREE DIRTY)" if dirty else ""))
