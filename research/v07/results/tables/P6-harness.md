| probe | A | B | win rate A | 95% CI | n | limit-hit rate | events/game | mean sets A | mean sets B |
|---|---|---|---:|---|---:|---:|---:|---:|---:|
| H1-forward | `v06:vmargin=-0.02` | `v06` | 49.77% | [49.47, 50.03] | 3,000 | 0.0000 | 95.3 | 4.497 | 4.503 |
| H1-reverse | `v06` | `v06:vmargin=-0.02` | 50.23% | [49.97, 50.53] | 3,000 | 0.0000 | 95.3 | 4.503 | 4.497 |
| H1-forward-k1 | `v06:vmargin=-0.02` | `v06` | 49.90% | [49.73, 50.07] | 3,000 | 0.0000 | 95.3 | 4.499 | 4.501 |
| H1-reverse-k1 | `v06` | `v06:vmargin=-0.02` | 50.10% | [49.93, 50.27] | 3,000 | 0.0000 | 95.3 | 4.501 | 4.499 |
| H2-cap-declare0 | `v06:declare=0` | `v06` | 42.53% | [40.87, 44.17] | 3,000 | 0.0000 | 104.2 | 4.257 | 4.743 |
| H2-cap-max | `v06:dead=1,deadmargin=-1000,deadbudget=999,declare=0,patient=1,pool=45` | `v06` | 0.00% | [0.00, 0.00] | 3,000 | 0.0000 | 105.5 | 0.822 | 8.178 |
| H2-cap-dead | `v06:dead=1,deadbudget=999,deadmargin=-1000` | `v06` | 0.00% | [0.00, 0.00] | 3,000 | 0.0000 | 101.0 | 0.851 | 8.149 |
| H2-cap-max200 | `v06:dead=1,deadmargin=-1000,deadbudget=999,declare=0,patient=1,pool=45` | `v06` | 0.00% | [0.00, 0.00] | 3,000 | 0.0000 | 105.5 | 0.822 | 8.178 |
| H2-cap-max120 | `v06:dead=1,deadmargin=-1000,deadbudget=999,declare=0,patient=1,pool=45` | `v06` | 0.00% | [0.00, 0.00] | 3,000 | 0.0367 | 105.2 | 0.830 | 8.170 |
