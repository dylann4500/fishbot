**train half** — 4 banks, written at commit `f4581da58ff0`.

| seed | deals | role | unseal phase | commitment digest | note |
|---:|---:|---|---:|---|---|
| 7030001 | 24,000 | train | 0 | `4bafec09b74925d6` | phase-2 adversary evaluation bank A; phases 3-4 training |
| 7030002 | 24,000 | train | 0 | `fb56483b3fffe0bb` | phase-2 adversary evaluation bank B; phases 3-4 training |
| 7030003 | 24,000 | train | 0 | `031ce425e3fdef3d` | phase-2 replication / transfer; phases 3-4 training |
| 7030004 | 24,000 | train | 0 | `e4acaae8326faed9` | phases 3-4 reserve |

**holdout half** — 7 banks, written at commit `f4581da58ff0`.

| seed | deals | role | unseal phase | commitment digest | note |
|---:|---:|---|---:|---|---|
| 7090001 | 24,000 | holdout | 5 | `896dbc89be124d85` | phase-5 holdout bank 1 |
| 7090002 | 24,000 | holdout | 5 | `0b6e40d834ac0ca1` | phase-5 holdout bank 2 |
| 7090003 | 24,000 | holdout | 5 | `863bea69baf6e73c` | phase-5 holdout bank 3 |
| 7090004 | 24,000 | holdout | 5 | `54f257c3f8ae9fab` | phase-5 fresh adversary search against the frozen v0.7 |
| 7090005 | 24,000 | holdout | 5 | `268a1dae71a31713` | phase-5 negative controls / planted-edge recovery |
| 7091001 | 24,000 | holdout | 5 | `958ada042cc26900` | sealed adversary half: evaluation bank |
| 7091002 | 24,000 | holdout | 5 | `5c39af3b5e0bd9a0` | sealed adversary half: fitting bank for the phase-5 fresh search |

The adversary bank is split 14 / 14 by a rule fixed before any result was known — rows sorted by id, alternating. The sealed half's plaintext SHA-256 is `1ca0346a332586c70a750f1523b10548…`, recorded in the file header and in `SEAL.json`, so phase 5 can verify it was not changed.
