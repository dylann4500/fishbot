| class | rung | dTrue (pts) | dFound pooled | excess over control | excess &minus; dTrue | detected |
|---|---|---:|---|---|---:|:--:|
| C1 | `none` (control) | 0.00 &mdash; mirror | +0.79 [+0.48, +1.10] | &mdash; | &mdash; | &mdash; |
| C1 | `decl,hstr=0.05` | +0.67 [+0.28, +1.06] | +1.04 [+0.73, +1.34] | +0.25 [-0.19, +0.68] | -0.42 | no |
| C1 | `decl,hstr=0.08` | +0.88 [+0.47, +1.28] | +0.78 [+0.47, +1.08] | -0.01 [-0.45, +0.42] | -0.89 | no |
| C1 | `leak,hstr=1.5` | +1.53 [+0.96, +2.11] | +2.53 [+2.22, +2.84] | +1.74 [+1.30, +2.18] | +0.21 | **yes** |
| C1 | `decl,hstr=0.11` | +2.13 [+1.61, +2.65] | +3.51 [+3.20, +3.82] | +2.72 [+2.28, +3.16] | +0.59 | **yes** |
| C1 | `decl,hstr=0.15` | +2.34 [+1.79, +2.88] | +4.71 [+4.40, +5.01] | +3.91 [+3.48, +4.35] | +1.58 | **yes** |

| class | detection floor at this sample size | evaluation games per bank |
|---|---:|---:|
| C1 | 1.53 pts | 48,000 |

Rungs run at this power: the false-positive control plus 5 planted rungs (`decl,hstr=0.05`, `decl,hstr=0.08`, `decl,hstr=0.11`, `decl,hstr=0.15`, `leak,hstr=1.5`), spanning the declaration family and the readability rung phase 1 quoted its 1.68-point floor from.
