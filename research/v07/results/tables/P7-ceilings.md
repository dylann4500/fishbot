| arm | target the incumbent plays | `v06` edge (pts) | 95% CI | target decl. acc. | target decl./game | target lock hold | n |
|---|---|---:|---|---:|---:|---:|---:|
| `control` | `v06` | +0.00 | mirror &mdash; no interval | 0.9777 | 4.49 | 4.67 | 12,000 |
| `urg-always-pool` | `v06:pool=45` | +0.38 | [-0.06, +0.81] | 0.9768 | 4.49 | 4.85 | 12,000 |
| `urg-always-oppcard` | `v06:oppfloor=54` | +0.38 | [-0.06, +0.81] | 0.9768 | 4.49 | 4.85 | 12,000 |
| `urg-always-events` | `v06:force=1` | +15.18 | [+14.41, +15.94] | 0.8115 | 4.75 | 2.62 | 12,000 |
| `urg-always-askfl` | `v06:askfloor=1.1` | +0.38 | [-0.06, +0.81] | 0.9768 | 4.49 | 4.85 | 12,000 |
| `urg-always-all` | `v06:pool=45,oppfloor=54,force=1,askfloor=1.1` | +15.18 | [+14.41, +15.94] | 0.8115 | 4.75 | 2.62 | 12,000 |
| `urg-never` | `v06:pool=-1,oppfloor=-1,force=1000000,askfloor=-1` | -1.62 | [-1.99, -1.24] | 0.9902 | 4.47 | 4.79 | 12,000 |
| `m2-off` | `v06:m2=0` | -0.87 | [-1.04, -0.71] | 0.9867 | 4.47 | 4.66 | 12,000 |
| `m2-off-never-urg` | `v06:m2=0,pool=-1,oppfloor=-1,force=1000000,askfloor=-1` | -1.62 | [-1.99, -1.24] | 0.9902 | 4.47 | 4.79 | 12,000 |
| `no-declare` | `v06:declare=0` | +7.75 | [+6.92, +8.58] | 0.0000 | 0.00 | 48.37 | 12,000 |
