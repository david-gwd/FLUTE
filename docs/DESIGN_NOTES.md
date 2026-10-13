# Design notes: pitfalls and how they are covered

FLUTE looks simple on paper but has several places where a plausible implementation silently produces
wrong or poor trees. Each item below is handled in the code and guarded by a test that compares against
an independent exact algorithm (`tests/reference.cpp`) or the network verifier.

| # | Pitfall | Handling |
|---|---------|----------|
| 1 | Groups related by a horizontal mirror store the mirror image of the tree *and* of its wirelength vector. Scoring them with the grid's own gap order picks a suboptimal tree. | Horizontal gaps are reversed for mirrored groups (`HananGrid::wirelength(…, mirrored)`). |
| 2 | "Swap the two lowest pins" is only defined in the stored orientation. Applying it after mirroring disconnects the tree. | Swap in the stored orientation first, then mirror. |
| 3 | The near-ring trees are only complete for exactly 7 pins (paper, Lemma 5). Using them for 8-pin grids with 7 boundary pins ignores the inner pin and omits the needed boundary-branch trees. | Near-ring for 7 pins, `Connect-adj-pins` for 8. All 40 320 eight-pin groups are validated. |
| 4 | Nets with pins on common grid lines are solved on a slightly perturbed grid; the result has to be mapped back or the pins are no longer on the tree. | `snapToGrid` on the result and on the breaking points. |
| 5 | Shifting pins by geometrically decreasing amounts vanishes in floating point after a few dozen pins on one line. | Linear offsets; an explicit error if separation is still impossible. |
| 6 | A lossless cut (Theorems 3 and 4) shares a pseudo-pin at the Hanan-grid corner (xᵣ, yᵣ), not an existing pin. Sharing a pin duplicates edges and loses up to 30 %. | `splitAtCorner`; tested for optimality against the exact solver. |
| 7 | Cutting only across y forces every piece to span the full width (thin strips); trees become several times too long for large nets. | Cut across the longer side, lossless cuts in both directions. |
| 8 | The scoring constants (β = 7.4/(n+10), γ = 4.8/(n−1)) and S₃ = \|sᵣ−(n+1)/2\|·h + \|r−(n+1)/2\|·v matter a lot for the quality of heuristic cuts. | As in the paper (Section V-B); the `accuracy` best cuts are tried (Section VI-C). |
| 9 | The two subtrees of a cut may overlap; counting shared edges twice over-estimates the length. | `mergeCollinear` unites overlapping segments. |
| 10 | Tree edge indices alias between rows (horizontal edge 7 of a row is vertical edge 0); sentinel values converted from infinity are undefined behaviour. | Checked edge accessors, `std::optional`, no sentinels. |

Results: nets of up to 8 pins are solved exactly; on the SteinLib ES instances the error against the optimum
is 0.8 % (10 pins) to 8.9 % (1000 pins), see the README.
