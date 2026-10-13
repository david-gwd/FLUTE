#include "flute/hanan_grid.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace flute {

namespace {

bool lessX(const Pin& lhs, const Pin& rhs) {
    return lhs.x < rhs.x;
}
bool lessY(const Pin& lhs, const Pin& rhs) {
    return lhs.y < rhs.y;
}

/// Splits pins that are sorted by @p coordinate into groups of equal coordinate.
template <typename Coordinate>
std::vector<std::vector<Pin>> groupSorted(const std::vector<Pin>& sorted, Coordinate coordinate) {
    std::vector<std::vector<Pin>> groups;
    for (const Pin& pin : sorted) {
        if (groups.empty() || coordinate(pin) != coordinate(groups.back().front())) {
            groups.emplace_back();
        }
        groups.back().push_back(pin);
    }
    return groups;
}

}  // namespace

int countDistinctPositions(const std::vector<Pin>& pins) {
    int distinct = 0;
    for (std::size_t i = 0; i < pins.size(); ++i) {
        bool seen = false;
        for (std::size_t j = 0; j < i && !seen; ++j) {
            seen = pins[i].x == pins[j].x && pins[i].y == pins[j].y;
        }
        if (!seen) {
            ++distinct;
        }
    }
    return distinct;
}

HananGrid::HananGrid(const std::vector<Point>& points) {
    if (points.empty()) {
        throw std::invalid_argument("a Hanan grid needs at least one pin");
    }
    pins_.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        pins_.push_back({points[i].x, points[i].y, static_cast<PinId>(i)});
    }
    rebuild();
    computeEpsilon();
}

HananGrid HananGrid::fromPositionSequence(const std::vector<int>& sequence) {
    std::vector<Point> points;
    points.reserve(sequence.size());
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        points.push_back({static_cast<Coord>(sequence[i]), static_cast<Coord>(i)});
    }
    return HananGrid(points);
}

void HananGrid::rebuild() {
    // Sorting uses std::sort on purpose: the relative order of pins with equal coordinates
    // (and therefore the result of separated()) is defined by this exact procedure.
    std::vector<Pin> sorted = pins_;
    std::sort(sorted.begin(), sorted.end(), lessX);
    columns_ = groupSorted(sorted, [](const Pin& pin) { return pin.x; });
    std::sort(sorted.begin(), sorted.end(), lessY);
    rows_ = groupSorted(sorted, [](const Pin& pin) { return pin.y; });

    xCoords_.clear();
    yCoords_.clear();
    horizontalGaps_.clear();
    verticalGaps_.clear();
    maxLineLoad_ = 0;
    for (const auto& column : columns_) {
        xCoords_.push_back(column.front().x);
        maxLineLoad_ = std::max(maxLineLoad_, column.size());
    }
    for (const auto& row : rows_) {
        yCoords_.push_back(row.front().y);
        maxLineLoad_ = std::max(maxLineLoad_, row.size());
    }
    for (std::size_t i = 1; i < xCoords_.size(); ++i) {
        horizontalGaps_.push_back(xCoords_[i] - xCoords_[i - 1]);
    }
    for (std::size_t i = 1; i < yCoords_.size(); ++i) {
        verticalGaps_.push_back(yCoords_[i] - yCoords_[i - 1]);
    }

    // Grid positions are assigned while walking over the lines, which is linear in the number
    // of pins.
    positions_.assign(pins_.size(), GridPoint{});
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        for (const Pin& pin : columns_[c]) {
            positions_[static_cast<std::size_t>(pin.id)].column = static_cast<GridIndex>(c);
        }
    }
    positionSequence_.clear();
    positionSequence_.reserve(pins_.size());
    for (std::size_t r = 0; r < rows_.size(); ++r) {
        for (const Pin& pin : rows_[r]) {
            GridPoint& position = positions_[static_cast<std::size_t>(pin.id)];
            position.row = static_cast<GridIndex>(r);
            positionSequence_.push_back(position.column);
        }
    }
}

void HananGrid::computeEpsilon() {
    const auto smallest = [](const std::vector<Coord>& gaps) {
        return gaps.empty() ? Coord{0} : *std::min_element(gaps.begin(), gaps.end());
    };
    const Coord eps_x = smallest(horizontalGaps_);
    const Coord eps_y = smallest(verticalGaps_);
    if (eps_x == 0) {
        epsilon_ = eps_y;
    } else if (eps_y == 0) {
        epsilon_ = eps_x;
    } else {
        epsilon_ = std::min(eps_x, eps_y);
    }
}

std::vector<PowvEntry> HananGrid::wirelengthVector(const Tree& tree) const {
    const std::size_t num_h = horizontalGaps_.size();
    const std::size_t num_v = verticalGaps_.size();
    std::vector<PowvEntry> vector(num_h + num_v, 0);
    for (std::size_t row = 0; row <= num_v; ++row) {
        for (std::size_t column = 0; column < num_h; ++column) {
            if (tree.hasHorizontalEdge(static_cast<int>(column), static_cast<int>(row))) {
                ++vector[column];
            }
        }
    }
    for (std::size_t row = 0; row < num_v; ++row) {
        for (std::size_t column = 0; column <= num_h; ++column) {
            if (tree.hasVerticalEdge(static_cast<int>(column), static_cast<int>(row))) {
                ++vector[num_h + row];
            }
        }
    }
    return vector;
}

Length HananGrid::wirelength(const PowvEntry* vector, bool mirrored) const {
    Length length = 0;
    const std::size_t num_h = horizontalGaps_.size();
    for (std::size_t i = 0; i < num_h; ++i) {
        length += horizontalGaps_[i] * vector[mirrored ? num_h - 1 - i : i];
    }
    for (std::size_t i = 0; i < verticalGaps_.size(); ++i) {
        length += verticalGaps_[i] * vector[num_h + i];
    }
    return length;
}

void HananGrid::movePins(const std::vector<std::pair<PinId, Point>>& moves) {
    for (const auto& [id, target] : moves) {
        if (id < 0 || id >= numPins()) {
            throw std::invalid_argument("movePins: invalid pin id");
        }
        Pin& pin = pins_[static_cast<std::size_t>(id)];
        pin.x = target.x;
        pin.y = target.y;
    }
    rebuild();
}

void HananGrid::removePins(std::vector<PinId> ids) {
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
        throw std::invalid_argument("removePins: duplicate pin id");
    }
    if (!ids.empty() && (ids.front() < 0 || ids.back() >= numPins())) {
        throw std::invalid_argument("removePins: invalid pin id");
    }
    if (ids.size() >= pins_.size()) {
        throw std::invalid_argument("removePins: cannot remove all pins");
    }
    std::vector<Pin> remaining;
    remaining.reserve(pins_.size() - ids.size());
    auto next_removed = ids.begin();
    for (const Pin& pin : pins_) {
        if (next_removed != ids.end() && *next_removed == pin.id) {
            ++next_removed;
            continue;
        }
        remaining.push_back({pin.x, pin.y, static_cast<PinId>(remaining.size())});
    }
    pins_ = std::move(remaining);
    rebuild();
}

HananGrid HananGrid::separated() const {
    HananGrid result = *this;

    // The pins of a line with k pins are moved by 0, 1, 2, ... k-1 steps of size
    // epsilon / (4k): the first pin stays, the others keep their order but never move by
    // more than a quarter of the smallest gap, so no grid line is overtaken. (Shrinking
    // the shift geometrically, as one might, makes it vanish in floating point after a few
    // dozen pins.)
    const Coord quarter = epsilon_ / 4;
    std::vector<std::pair<PinId, Point>> moves;
    for (const auto& column : result.columns_) {
        const auto count = static_cast<Coord>(column.size());
        for (std::size_t i = 1; i < column.size(); ++i) {
            const Coord shift = quarter * static_cast<Coord>(i) / count;
            moves.push_back({column[i].id, {column[i].x + shift, column[i].y}});
        }
    }
    result.movePins(moves);

    moves.clear();
    for (const auto& row : result.rows_) {
        const auto count = static_cast<Coord>(row.size());
        for (std::size_t i = 1; i < row.size(); ++i) {
            const Coord shift = quarter * static_cast<Coord>(i) / count;
            moves.push_back({row[i].id, {row[i].x, row[i].y + shift}});
        }
    }
    result.movePins(moves);
    return result;
}

}  // namespace flute
