#pragma once

/// @file hanan_grid.hpp
/// The Hanan grid of a pin set.

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "flute/tree.hpp"
#include "flute/types.hpp"

namespace flute {

/// A pin of a net.
struct Pin {
    Coord x = 0;
    Coord y = 0;
    PinId id = 0;

    Point point() const { return {x, y}; }
};

/// Number of distinct positions among @p pins.
int countDistinctPositions(const std::vector<Pin>& pins);

/// One entry of a wirelength vector: the number of tree edges that cross one gap
/// between two neighbouring grid lines.
using PowvEntry = std::uint8_t;

/// The Hanan grid of a set of pins: the grid spanned by the horizontal and vertical lines
/// through all pins.
///
/// Terminology:
///  - a *column* is the set of pins with the same x coordinate, columns are sorted by x;
///  - a *row* is the set of pins with the same y coordinate, rows are sorted by y;
///  - the *position sequence* lists, going through the rows from bottom to top, the column
///    index of each pin. For pins in general position (no two pins share a line) it is a
///    permutation, which determines the combinatorial structure of the grid.
///
/// Invariants: pin ids are 0..n-1 and pins()[i].id == i. All derived data is kept in sync by the
/// mutating member functions.
class HananGrid {
public:
    /// Builds the grid of @p points; the pin ids are the indices into @p points.
    /// @throws std::invalid_argument if @p points is empty
    explicit HananGrid(const std::vector<Point>& points);

    /// Builds the grid of the permutation @p sequence: pin i is placed at (sequence[i], i).
    /// @throws std::invalid_argument if @p sequence is empty
    static HananGrid fromPositionSequence(const std::vector<int>& sequence);

    // ---- pins -------------------------------------------------------------------------------

    int numPins() const { return static_cast<int>(pins_.size()); }
    const std::vector<Pin>& pins() const { return pins_; }
    const Pin& pin(PinId id) const { return pins_[static_cast<std::size_t>(id)]; }

    // ---- grid lines ---------------------------------------------------------------------------

    const std::vector<std::vector<Pin>>& columns() const { return columns_; }
    const std::vector<std::vector<Pin>>& rows() const { return rows_; }
    int numColumns() const { return static_cast<int>(columns_.size()); }
    int numRows() const { return static_cast<int>(rows_.size()); }

    /// Coordinates of the grid lines (strictly increasing).
    const std::vector<Coord>& xCoords() const { return xCoords_; }
    const std::vector<Coord>& yCoords() const { return yCoords_; }

    /// Distances between neighbouring grid lines: horizontalGaps()[i] is the width of the gap
    /// between columns i and i+1 and verticalGaps()[i] the height between rows i and i+1.
    const std::vector<Coord>& horizontalGaps() const { return horizontalGaps_; }
    const std::vector<Coord>& verticalGaps() const { return verticalGaps_; }

    /// Position of a pin inside the grid.
    GridPoint position(PinId id) const { return positions_[static_cast<std::size_t>(id)]; }

    /// Column index of every pin, listed from the bottom row to the top row.
    const std::vector<GridIndex>& positionSequence() const { return positionSequence_; }

    /// True if the position sequence is a permutation of 0..n-1, i.e. no two pins share a
    /// column or a row.
    bool inGeneralPosition() const { return maxLineLoad_ <= 1; }

    /// True if some column or row contains more than one pin.
    bool hasSharedLines() const { return maxLineLoad_ > 1; }

    /// True if all pins have the same position.
    bool isSinglePoint() const { return xCoords_.size() == 1 && yCoords_.size() == 1; }

    /// Small length used by separated() to break ties; 0 for a single point.
    Coord epsilon() const { return epsilon_; }

    // ---- tree lengths ----------------------------------------------------------------------

    /// Wirelength vector of a tree drawn on this grid: entry i counts the tree edges crossing
    /// gap i; the first numColumns()-1 entries are horizontal gaps, the remaining entries are
    /// vertical gaps.
    std::vector<PowvEntry> wirelengthVector(const Tree& tree) const;

    /// Length of a tree with the given wirelength vector (as returned by wirelengthVector()
    /// or stored in the lookup table).
    ///
    /// With @p mirrored the vector is taken to describe the horizontal mirror image of this
    /// grid: the horizontal entries are matched with the gaps in reverse order.
    Length wirelength(const PowvEntry* vector, bool mirrored = false) const;

    // ---- mutation ---------------------------------------------------------------------------

    /// Moves pins to new positions (pin ids are unchanged) and rebuilds the grid.
    void movePins(const std::vector<std::pair<PinId, Point>>& moves);

    /// Removes pins and renumbers the remaining pins 0..n-1 keeping their relative order.
    /// @throws std::invalid_argument on an invalid or duplicate id, or when no pin would remain
    void removePins(std::vector<PinId> ids);

    /// Returns a copy in which pins that share a column or a row are moved apart by less than
    /// epsilon() / 4 such that the position sequence becomes a permutation. The order of the
    /// grid lines is preserved and no pin moves down or to the left.
    ///
    /// The result is in general position unless the coordinates are so large compared to
    /// epsilon() that the shifts are lost to rounding; check inGeneralPosition().
    HananGrid separated() const;

private:
    HananGrid() = default;

    /// Recomputes all derived data from pins_.
    void rebuild();
    /// Chooses epsilon_ from the gaps; must be called after rebuild().
    void computeEpsilon();

    std::vector<Pin> pins_;
    std::vector<std::vector<Pin>> columns_;
    std::vector<std::vector<Pin>> rows_;
    std::vector<Coord> xCoords_;
    std::vector<Coord> yCoords_;
    std::vector<Coord> horizontalGaps_;
    std::vector<Coord> verticalGaps_;
    std::vector<GridPoint> positions_;
    std::vector<GridIndex> positionSequence_;
    std::size_t maxLineLoad_ = 0;
    Coord epsilon_ = 0;
};

}  // namespace flute
