#pragma once

/// @file net_breaking.hpp
/// Splitting of nets with more pins than the lookup table covers (Section V of the FLUTE paper).
///
/// A net is broken into two subnets that are solved separately; their trees together form a
/// tree of the whole net. Two kinds of breaking exist:
///
///  - *lossless* breaking (Theorems 3 and 4 of the paper): if the lowest m pins of a net lie
///    in the m leftmost (or rightmost) columns, an optimal tree of the whole net is the union of
///    optimal trees of the two parts, which share a pseudo-pin at a corner of the Hanan grid;
///  - *heuristic* breaking at a pin: the parts are the pins below and above, both including
///    that pin, chosen by a score that predicts the resulting tree length.
///
/// Cuts can be made across rows (splitting into lower and upper part) or across columns
/// (left and right part); a column cut is a row cut of the transposed net.

#include <optional>
#include <utility>
#include <vector>

#include "flute/hanan_grid.hpp"

namespace flute {

/// The grid with x and y exchanged (pin ids are kept).
HananGrid transposed(const HananGrid& grid);

// ---- lossless breaking ------------------------------------------------------------------------

/// A lossless way of breaking a net across rows: the rows 0..row form the lower part.
struct LosslessCut {
    GridIndex row = 0;
    /// True if the lower part lies in the leftmost columns (Theorem 3), false if it lies in the
    /// rightmost columns (Theorem 4).
    bool lowerIsLeft = true;
};

/// Looks for a lossless cut that leaves both parts smaller than the net; among several
/// candidates the median one is returned, which splits most evenly.
/// @pre grid.inGeneralPosition() and grid.numPins() >= 4
std::optional<LosslessCut> findLosslessCut(const HananGrid& grid);

/// The corner of the Hanan grid that both parts of a lossless cut have in common.
Point cornerOf(const HananGrid& grid, const LosslessCut& cut);

/// Splits the net: the lower part consists of the pins in the rows 0..row plus the corner, the
/// upper part of the corner plus the remaining pins. (The corner is omitted from a part where
/// it coincides with a pin.) Pins are renumbered.
/// @pre grid.inGeneralPosition() and 1 <= cut.row <= grid.numRows() - 3
std::pair<HananGrid, HananGrid> splitAtCorner(const HananGrid& grid, const LosslessCut& cut);

// ---- heuristic breaking -----------------------------------------------------------------------

/// Heuristic score of breaking the net at the pin of each row (higher is better). Only the rows
/// 3..n-2 (counted from 1) are candidates; all others get -infinity.
/// @pre grid.inGeneralPosition() and grid.numPins() >= 5
std::vector<Length> breakingScores(const HananGrid& grid);

/// Row with the best breakingScores().
GridIndex chooseBreakingRow(const HananGrid& grid);

/// Splits the net at @p row into the pins of the rows 0..row and the pins of the rows
/// row..top; the pin at @p row belongs to both. Pins are renumbered.
/// @pre grid.inGeneralPosition() and 0 < row < grid.numRows() - 1
std::pair<HananGrid, HananGrid> splitAtRow(const HananGrid& grid, GridIndex row);

// ---- choosing a breaking ------------------------------------------------------------------------

/// A way of breaking a net, across a row or a column.
struct Breaking {
    enum class Direction { AtRow, AtColumn };

    Direction direction = Direction::AtRow;
    GridIndex line = 0;       ///< row / column of the breaking pin (heuristic) or of the cut
    bool lossless = false;    ///< breaking here cannot increase the optimal tree length
    bool lowerIsLeft = true;  ///< lossless only, see LosslessCut
    Length score = 0;         ///< heuristic score, larger is better (infinite if lossless)
};

/// The best ways of breaking a net, best first: a single lossless breaking if there is one
/// (in either direction), otherwise the @p maxCount best-scoring breakings across the longer
/// side of the net. Never empty.
/// @pre grid.inGeneralPosition() and grid.numPins() >= 5 and maxCount >= 1
std::vector<Breaking> chooseBreakings(const HananGrid& grid, int maxCount);

/// The point at which the net is broken: the shared corner of a lossless breaking, otherwise
/// the breaking pin.
Point breakingPoint(const HananGrid& grid, const Breaking& breaking);

/// Splits the net as described by @p breaking; the parts are returned as (lower or left part,
/// upper or right part).
std::pair<HananGrid, HananGrid> split(const HananGrid& grid, const Breaking& breaking);

}  // namespace flute
