| id | class | responder base | target | objective | fitting bank | budget (gens x pop x deals) | games spent | regime / seats | hypothesis |
|---|---|---|---|---|---:|---|---:|---|---|
| X01 | C1 | `v06` | Ffast | `win` | 7040001 | 12x16x250 | 96000 | A1, k = 3 | L0 in-class control at phase-1's standard budget |
| X02 | C1 | `v06` | Ffast | `win` | 7040002 | 20x20x350 | 280000 | A1, k = 3 | L0 budget-curve top rung: is the null a budget artifact |
| X03 | C1 | `v06` | Ffast | `win` | 7040003 | 8x12x150 | &mdash; | A1, k = 3 | L0 independent replicate at a third seed |
| X04 | C1 | `v06` | Ffast | `setdiff` | 7040004 | 8x12x150 | &mdash; | A1, k = 3 | L0 margin in half-suits, not games: the low-variance objective |
| X05 | C1 | `v06` | Ffast | `declerr` | 7040005 | 8x12x150 | 28800 | A1, k = 3 | L1 drive the target's misdeclaration rate |
| X06 | C1 | `v06` | Ffast | `asksupp` | 7040006 | 8x12x150 | &mdash; | A1, k = 3 | L3/L10 suppress the target's ask hit rate |
| X07 | C1 | `v06` | Ffast | `forced` | 7040007 | 8x12x150 | &mdash; | A1, k = 3 | L13 raise forced-endgame incidence under pressure |
| X08 | C1 | `v06` | Ffast | `events` | 7040008 | 8x12x150 | &mdash; | A1, k = 3 | stalling: lengthen the game |
| X09 | C1 | `v06` | Ffast | `declsupp` | 7040009 | 8x12x150 | &mdash; | A1, k = 3 | L1 timing: deny the target its declarations |
| X10 | C2 | `v07` | Ffast | `win` | 7040010 | 12x16x250 | &mdash; | A1, k = 3 | L11/L1 extended features incl. information denial |
| X11 | C2 | `v07` | Ffast | `declerr` | 7040011 | 8x12x150 | &mdash; | A1, k = 3 | L1 information denial, scored on the target's errors |
| X12 | C2 | `v07` | Ffast | `setdiff` | 7040012 | 8x12x150 | &mdash; | A1, k = 3 | L1 information denial, scored on the margin |
| X13 | C2 | `v07:dead7=1` | Ffast | `win` | 7040013 | 8x12x150 | 28800 | A1, k = 3 | L14 the deliberate miss admitted to the scored set |
| X14 | C2 | `v07:corr=3` | Ffast | `win` | 7040014 | 8x12x150 | 28800 | A2 correlated | A2 ex-ante correlated role plans |
| X15 | C2 | `v07` | Ffast | `asksupp` | 7040015 | 8x12x150 | &mdash; | A1, k = 3 | L10 gate pressure via the extended class |
| X16 | C1 | `v06` | Ffast | `limit` | 7040016 | 8x12x200 | &mdash; | A1, k = 3 | harness probe: drive games to the action cap |
| X17 | C1 | `v06` | Ffast | `win` | 7040017 | 8x12x150 | &mdash; | k = 1 | k=1 one-seat deviation column, fitted |
| X18 | C1 | `v06` | Fcheap | `win` | 7040018 | 8x12x120 | 23040 | A1, k = 3 | attack the cheap search end in class |
| X19 | C2 | `v07` | Fcheap | `win` | 7040019 | 8x12x120 | 23040 | A1, k = 3 | attack the cheap search end, extended |
| X20 | C1 | `v06` | Fcheap | `declerr` | 7040020 | 8x12x120 | 23040 | A1, k = 3 | L1 against a searching target |
| X21 | C1 | `v06` | Fmid | `win` | 7040021 | 6x10x60 | &mdash; | A1, k = 3 | attack the strongest measured configuration directly |
| X22 | C1 | `v06` | FRONT | `win` | 7040022 | 8x12x120 | &mdash; | A1, k = 3 | dominate the FRONTIER: min over {Ffast, Fcheap} |
| X23 | C2 | `v07` | FRONT | `win` | 7040023 | 8x12x120 | &mdash; | A1, k = 3 | dominate the frontier, extended class |
| X24 | C2 | `v07:dead7=1,corr=3` | Ffast | `win` | 7040024 | 8x12x150 | &mdash; | A2 correlated | deliberate miss x correlated roles |
| X25 | C1 | `v06` | Ffast | `win` | 7040025 | 8x12x150 | &mdash; | A1, k = 3, v0.5 basin | different starting basin: v0.5 defaults, not the incumbent |
| X26 | C1 | `v06` | Ffast | `win` | 7040026 | 8x12x150 | &mdash; | A1, k = 3, wide sigma | wide exploration: is the CEM trapped near the incumbent |
| X27 | C2 | `v07` | Ffast | `win` | 7040027 | 8x12x150 | &mdash; | A1, k = 3, wide sigma | wide exploration in the extended class |
| X28 | C1 | `v06` | Ffast | `setdiff` | 7040028 | 8x12x150 | &mdash; | k = 1 | k=1 one-seat deviation, margin objective |
| X29 | C2 | `v07` | Ffast | `win` | 7040029 | 12x16x250 | 96000 | A1, k = 3, seeded at the denial direction | refine the denial direction rather than rediscover it |
| X30 | C2 | `v07` | Ffast | `setdiff` | 7040030 | 8x12x150 | &mdash; | A1, k = 3, seeded at the denial direction | the same, on the low-variance margin objective |
| X31 | C2 | `v07` | Fcheap | `win` | 7040031 | 8x12x120 | &mdash; | A1, k = 3, seeded at the denial direction | does the refined denial direction survive the frontier's search |
