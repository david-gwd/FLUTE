# Architecture

```
HananGrid ──► LookupTable::findOptimalTree ──► treeToSegments ──► mergeCollinear
    │              ▲                                                   ▲
    │              └── generateLookupTable (offline, parallel)         │
    └── (> maxPins) net_breaking: chooseBreakings / split ── RsmtSolver::breakNet (recursive)
```

* **`Tree`** — a tree on an 8×8 grid as a 112-bit set (15 bits per row: 7 horizontal + 8 vertical edges).
  Provides the transformations the generator needs: re-inserting a removed column/row, mirroring, the
  "swap the two lowest pins" operation.
* **`HananGrid`** — pins, columns/rows, gaps, the *position sequence*; `separated()` moves pins on common
  lines apart by tiny offsets so that every net has a well-defined group.
* **`LookupTable`** — for each group (permutation up to 4-fold symmetry: n!/4 groups) a list of POSTs
  with their POWVs, stored flat. `findOptimalTree` classifies the grid, picks the best POWV for the
  real gap lengths and undoes the symmetry. Text format `n~group … EOG`.
* **`lut_generator`** — the recursion of the paper (Fig. 8): boundary compaction/expansion, near-ring
  trees for 7 pins, `Connect-adj-pins` for 8, pruning of dominated POWVs. Groups are independent and
  generated on all cores; the result is deterministic.
* **`net_breaking` / `RsmtSolver`** — lossless cuts (Theorems 3/4) in either direction, otherwise the
  scored heuristic across the longer side; the best `accuracy` candidates are solved recursively and the
  shortest union is kept.
* **`verify`** — independent connectivity check, used by `--verify`, tests and the benchmark tool.

Testing strategy: every claim of optimality is checked against an exact algorithm that shares no code
with the library (`tests/reference.cpp`); lossless cuts, the table for 4–8 pins and the solver are all
compared with it on random nets.
