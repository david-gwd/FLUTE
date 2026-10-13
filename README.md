# FLUTE — fast rectilinear Steiner minimal trees

A C++17 implementation of **FLUTE** (Chu & Wong, *"FLUTE: Fast Lookup Table Based Rectilinear
Steiner Minimal Tree Algorithm for VLSI Design"*, IEEE TCAD 27(1), 2008). Given the pins of a net, it
computes a short rectilinear (Manhattan) tree connecting them — the wirelength estimate used throughout
VLSI placement and routing.

* Nets with **up to 8 pins are solved optimally** by a table lookup, in microseconds.
* Larger nets are **broken recursively** into small subnets (lossless where the paper proves it is
  possible, by a scored heuristic otherwise).
* Library, command line tool, benchmark tool, unit tests with independent exact oracles, CI.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the structure and
[docs/DESIGN_NOTES.md](docs/DESIGN_NOTES.md) for the pitfalls of the algorithm and how they are tested.

## Quick start

```sh
cmake -B build -G Ninja            # or any generator; Release is the default build type
cmake --build build

./build/flute generate             # one-time: builds flute_lookup.txt (~13 min on 4 cores)
./build/flute solve data/instances/steinlib/es100fst01.txt --verify
```

```
instance:  data/instances/steinlib/es100fst01.txt
pins:      100
length:    7.62486e+07
edges:     148
optimal:   no (net broken at 11 pins)
verified:  yes
time:      236 ms loading, 3.1 ms solving
```

The lookup table is generated, not shipped (23 MB of text). Point `flute` at it with `--lookup`, or
set `FLUTE_LOOKUP_TABLE`. Smaller tables (`--max-pins 6`, generated in a fraction of a second) work too;
nets above the table size are simply broken more often.

`flute solve --output tree.txt …` writes the tree; `python3 tools/draw.py tree.txt` plots it
(`pip install -r tools/requirements.txt`).

### Input format

SteinLib STP files. Only the terminal list and the coordinate section are read:

```
Section Terminals
Terminals 3
T 1
T 2
T 3
End

Section Coordinates
DD 1 0 0
DD 2 10.5 2
DD 3 4 7
End
```

`tools/generate_instance.py` creates random instances. Malformed files are rejected with the file name
and line number (duplicate or unknown terminals, missing coordinates, `nan`/`inf`, over-long lines, …).

## Library

```cpp
#include "flute/lookup_table.hpp"
#include "flute/solver.hpp"

const auto table = flute::LookupTable::load("flute_lookup.txt");
const flute::RsmtSolver solver(table);                       // accuracy 3, like the paper

const flute::Solution tree = solver.solve({{0, 0}, {10, 2}, {4, 7}, {9, 9}});
tree.length();                 // total length
tree.edges;                    // axis-parallel segments
tree.breakingPoints.empty();   // true  <=>  the tree is provably optimal
```

Link against the `flute::core` CMake target. All errors derive from `flute::Error`; violated
preconditions throw `std::invalid_argument`. `flute::verifyConnectedNetwork()` independently checks that
a result connects all pins.

## Quality and speed

Mean error against the known optimum (GeoSteiner) on the SteinLib *ES* benchmarks, 15 instances per
size, default accuracy (`flute_bench`, see below), 4-core Intel laptop:

| pins | mean error | max error | time per net |
|-----:|-----------:|----------:|-------------:|
|   10 |     0.8 % |     4.8 % |     0.15 ms |
|   20 |     1.9 % |     5.8 % |     0.9 ms |
|   40 |     2.8 % |     4.8 % |     1.8 ms |
|   60 |     5.0 % |     7.8 % |     3.3 ms |
|  100 |     5.2 % |     6.8 % |     6 ms |
|  250 |     7.6 % |    10.0 % |    19 ms |
|  500 |     8.1 % |    10.1 % |    49 ms |
| 1000 |     8.9 % |    10.7 % |   100 ms |

Nets of up to 8 pins are exact (checked against a Dreyfus–Wagner oracle). For larger nets the error grows
with the net size; this is not tuned for optimality, and the paper's local refinement step is not
reproduced (`--refine` is a much simpler, experimental substitute). `--accuracy N` trades time for
shorter trees (`--accuracy 12`: 1.0 % / 3.0 % / 6.4 % at 40 / 100 / 500 pins).

Other figures (same machine): loading the 8-pin table takes about 240 ms, a 10 000-pin net is solved in
1.4 s, generating the table takes about 13 minutes on 4 cores (a one-time step).

Reproduce the table with
`./build/flute_bench --lookup flute_lookup.txt --optima data/steinlib_es_optima.csv <instances>`
(the full ES benchmark set is available from [SteinLib](https://steinlib.zib.de/)).

## How it works

The Hanan grid of a net depends, up to edge lengths, only on the *position sequence* of its pins — the
permutation of the pins' x-ranks, ordered by y. For every permutation of up to 8 pins the table stores a few
*potentially optimal Steiner trees* (POSTs) together with their *wirelength vectors* (POWVs: how often each
grid gap is crossed). To solve a net, evaluate every POWV of its group with the actual gap lengths and
take the best tree. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the table generation, the symmetry
reduction and net breaking.

## Repository layout

```
include/flute/   public headers              src/        implementation
apps/            flute (CLI), flute_bench    tests/      Catch2 unit tests, oracles, golden checks
data/            sample instances, optima    tools/      plotting, instance generator
docs/            architecture, design notes   scripts/    maintenance scripts
```

## Development

```sh
cmake -B build -DFLUTE_WARNINGS_AS_ERRORS=ON      # also: -DFLUTE_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build -LE slow                   # unit tests, a few seconds
ctest --test-dir build -L slow                    # generates the full table, golden lengths, exact 8-pin check
```

Style: `clang-format` (config in the repo), `clang-tidy` clean, warnings as errors in CI.

## Limitations

* Pin coordinates are doubles; nets whose pins on one line are closer than the floating-point resolution
  at their magnitude cannot be separated and are rejected with an error (never silently mis-solved).
* The table covers up to 8 pins (the paper goes up to 9); this is fixed by the 8×8 tree encoding.
* Output length is a heuristic above 8 pins, see above.

## License

MIT — see [LICENSE](LICENSE). The algorithm is due to Chris C. N. Chu and
Yiu-Chung Wong; please cite their paper when using this code academically.
