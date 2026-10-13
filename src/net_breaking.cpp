#include "flute/net_breaking.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace flute {

namespace {

void requireGeneralPosition(const HananGrid& grid, int minPins) {
    if (!grid.inGeneralPosition()) {
        throw std::invalid_argument("net breaking needs pins in general position");
    }
    if (grid.numPins() < minPins) {
        throw std::invalid_argument("net too small to be broken");
    }
}

}  // namespace

std::optional<LosslessCut> findLosslessCut(const HananGrid& grid) {
    requireGeneralPosition(grid, 4);
    const auto& sequence = grid.positionSequence();
    const std::size_t n = sequence.size();

    // prefix_*[i] / suffix_*[i]: maximum / minimum of sequence[0..i] and sequence[i..n-1]
    std::vector<GridIndex> prefix_max(n);
    std::vector<GridIndex> prefix_min(n);
    std::vector<GridIndex> suffix_max(n);
    std::vector<GridIndex> suffix_min(n);
    for (std::size_t i = 0; i < n; ++i) {
        prefix_max[i] = i == 0 ? sequence[i] : std::max(prefix_max[i - 1], sequence[i]);
        prefix_min[i] = i == 0 ? sequence[i] : std::min(prefix_min[i - 1], sequence[i]);
    }
    for (std::size_t i = n; i-- > 0;) {
        suffix_max[i] = i + 1 == n ? sequence[i] : std::max(suffix_max[i + 1], sequence[i]);
        suffix_min[i] = i + 1 == n ? sequence[i] : std::min(suffix_min[i + 1], sequence[i]);
    }

    // The lower part has row + 1 pins and each part gets the corner as an extra pin, so both
    // parts are smaller than the net exactly for 1 <= row <= n-3.
    std::vector<LosslessCut> candidates;
    for (std::size_t row = 1; row + 3 <= n; ++row) {
        if (prefix_max[row] <= suffix_min[row + 1]) {
            candidates.push_back({static_cast<GridIndex>(row), true});
        } else if (suffix_max[row + 1] <= prefix_min[row]) {
            candidates.push_back({static_cast<GridIndex>(row), false});
        }
    }
    if (candidates.empty()) {
        return std::nullopt;
    }
    return candidates[candidates.size() / 2];
}

Point cornerOf(const HananGrid& grid, const LosslessCut& cut) {
    const int n = grid.numPins();
    const int lower_pins = cut.row + 1;
    const GridIndex column = cut.lowerIsLeft ? lower_pins - 1 : n - lower_pins;
    return {grid.xCoords().at(static_cast<std::size_t>(column)),
            grid.yCoords().at(static_cast<std::size_t>(cut.row))};
}

std::pair<HananGrid, HananGrid> splitAtCorner(const HananGrid& grid, const LosslessCut& cut) {
    requireGeneralPosition(grid, 4);
    if (cut.row < 1 || cut.row > grid.numRows() - 3) {
        throw std::invalid_argument("splitAtCorner: row out of range");
    }
    const Point corner = cornerOf(grid, cut);
    std::vector<Point> lower;
    std::vector<Point> upper;
    for (GridIndex row = 0; row < grid.numRows(); ++row) {
        const Point pin = grid.rows()[static_cast<std::size_t>(row)].front().point();
        (row <= cut.row ? lower : upper).push_back(pin);
    }
    if (std::find(lower.begin(), lower.end(), corner) == lower.end()) {
        lower.push_back(corner);
    }
    upper.push_back(corner);
    return {HananGrid(lower), HananGrid(upper)};
}

std::vector<Length> breakingScores(const HananGrid& grid) {
    requireGeneralPosition(grid, 5);
    const int n = grid.numPins();
    const auto& xs = grid.xCoords();
    const auto& ys = grid.yCoords();
    const auto& rows = grid.rows();
    const auto& sequence = grid.positionSequence();
    const auto at = [](const std::vector<Coord>& values, int index) {
        return values[static_cast<std::size_t>(index)];
    };

    // Weights determined empirically by Chu and Wong (2008), Section V-B.
    const double alpha = 0.3;
    const double beta = 7.4 / (n + 10);
    const double gamma = 4.8 / (n - 1);
    const double center = (n - 1) / 2.0;  // middle column / row index
    const Coord h = (at(xs, n - 2) - at(xs, 1)) / (n - 3);
    const Coord v = (at(ys, n - 2) - at(ys, 1)) / (n - 3);

    // Extent in x of the rows 0..p (prefix) and p..n-1 (suffix); they give the half-perimeter
    // wirelengths of the lower and the upper part in constant time per row.
    const auto count = static_cast<std::size_t>(n);
    std::vector<Coord> prefix_min(count);
    std::vector<Coord> prefix_max(count);
    std::vector<Coord> suffix_min(count);
    std::vector<Coord> suffix_max(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Coord x = rows[i].front().x;
        prefix_min[i] = i == 0 ? x : std::min(prefix_min[i - 1], x);
        prefix_max[i] = i == 0 ? x : std::max(prefix_max[i - 1], x);
    }
    for (std::size_t i = count; i-- > 0;) {
        const Coord x = rows[i].front().x;
        suffix_min[i] = i + 1 == count ? x : std::min(suffix_min[i + 1], x);
        suffix_max[i] = i + 1 == count ? x : std::max(suffix_max[i + 1], x);
    }

    // As in the paper's implementation, only rows 3..n-2 (1-based) are candidates; the rows
    // next to the boundary would leave one part almost as large as the whole net.
    std::vector<Length> scores(count, -std::numeric_limits<Length>::infinity());
    for (int p = 2; p < n - 2; ++p) {
        const auto row = static_cast<std::size_t>(p);
        const int column = sequence[row];

        const Coord s1 = at(ys, p + 1) - at(ys, p - 1);
        Coord s2 = 0;
        if (column == 0 || column == 1) {
            s2 = 2 * (at(xs, 2) - at(xs, 1));
        } else if (column == n - 1 || column == n - 2) {
            s2 = 2 * (at(xs, n - 2) - at(xs, n - 3));
        } else {
            s2 = at(xs, column + 1) - at(xs, column - 1);
        }
        const Coord s3 = (std::abs(column - center) * h) + (std::abs(p - center) * v);
        const Coord lower = at(ys, p) - at(ys, 0) + prefix_max[row] - prefix_min[row];
        const Coord upper = at(ys, n - 1) - at(ys, p) + suffix_max[row] - suffix_min[row];
        const Coord s4 = lower + upper;

        scores[row] = s1 - alpha * s2 - beta * s3 - gamma * s4;
    }
    return scores;
}

GridIndex chooseBreakingRow(const HananGrid& grid) {
    const std::vector<Length> scores = breakingScores(grid);
    return static_cast<GridIndex>(std::max_element(scores.begin(), scores.end()) - scores.begin());
}

std::pair<HananGrid, HananGrid> splitAtRow(const HananGrid& grid, GridIndex row) {
    requireGeneralPosition(grid, 3);
    if (row <= 0 || row >= grid.numRows() - 1) {
        throw std::invalid_argument("splitAtRow: the first and the last row cannot split a net");
    }
    std::vector<PinId> above;  // removed from the lower part
    std::vector<PinId> below;  // removed from the upper part
    for (GridIndex r = 0; r < grid.numRows(); ++r) {
        for (const Pin& pin : grid.rows()[static_cast<std::size_t>(r)]) {
            if (r < row) {
                below.push_back(pin.id);
            }
            if (r > row) {
                above.push_back(pin.id);
            }
        }
    }
    HananGrid lower = grid;
    HananGrid upper = grid;
    lower.removePins(std::move(above));
    upper.removePins(std::move(below));
    return {std::move(lower), std::move(upper)};
}

HananGrid transposed(const HananGrid& grid) {
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(grid.numPins()));
    for (const Pin& pin : grid.pins()) {
        points.push_back({pin.y, pin.x});
    }
    return HananGrid(points);
}

namespace {

Breaking::Direction longerSide(const HananGrid& grid) {
    const Coord width = grid.xCoords().back() - grid.xCoords().front();
    const Coord height = grid.yCoords().back() - grid.yCoords().front();
    return height >= width ? Breaking::Direction::AtRow : Breaking::Direction::AtColumn;
}

}  // namespace

std::vector<Breaking> chooseBreakings(const HananGrid& grid, int maxCount) {
    if (maxCount < 1) {
        throw std::invalid_argument("chooseBreakings: maxCount must be positive");
    }
    requireGeneralPosition(grid, 5);
    const HananGrid flipped = transposed(grid);
    const Breaking::Direction preferred = longerSide(grid);
    const auto gridFor = [&](Breaking::Direction direction) -> const HananGrid& {
        return direction == Breaking::Direction::AtRow ? grid : flipped;
    };
    const Breaking::Direction other = preferred == Breaking::Direction::AtRow
                                          ? Breaking::Direction::AtColumn
                                          : Breaking::Direction::AtRow;

    // A lossless breaking is always best; look across the longer side first.
    for (const Breaking::Direction direction : {preferred, other}) {
        if (const auto cut = findLosslessCut(gridFor(direction))) {
            return {{direction, cut->row, true, cut->lowerIsLeft,
                     std::numeric_limits<Length>::infinity()}};
        }
    }

    const std::vector<Length> scores = breakingScores(gridFor(preferred));
    std::vector<GridIndex> order(scores.size());
    std::iota(order.begin(), order.end(), GridIndex{0});
    std::stable_sort(order.begin(), order.end(), [&](GridIndex a, GridIndex b) {
        return scores[static_cast<std::size_t>(a)] > scores[static_cast<std::size_t>(b)];
    });

    std::vector<Breaking> result;
    for (const GridIndex line : order) {
        const Length score = scores[static_cast<std::size_t>(line)];
        if (!std::isfinite(score) || result.size() >= static_cast<std::size_t>(maxCount)) {
            break;
        }
        result.push_back({preferred, line, false, true, score});
    }
    if (result.empty()) {
        throw std::logic_error("chooseBreakings: no row can split the net");
    }
    return result;
}

Point breakingPoint(const HananGrid& grid, const Breaking& breaking) {
    const bool by_row = breaking.direction == Breaking::Direction::AtRow;
    const HananGrid flipped = by_row ? grid : transposed(grid);
    Point point;
    if (breaking.lossless) {
        point = cornerOf(flipped, {breaking.line, breaking.lowerIsLeft});
    } else {
        point = flipped.rows().at(static_cast<std::size_t>(breaking.line)).front().point();
    }
    return by_row ? point : Point{point.y, point.x};
}

std::pair<HananGrid, HananGrid> split(const HananGrid& grid, const Breaking& breaking) {
    const bool by_row = breaking.direction == Breaking::Direction::AtRow;
    const HananGrid flipped = by_row ? grid : transposed(grid);
    auto parts = breaking.lossless ? splitAtCorner(flipped, {breaking.line, breaking.lowerIsLeft})
                                   : splitAtRow(flipped, breaking.line);
    if (by_row) {
        return parts;
    }
    return {transposed(parts.first), transposed(parts.second)};
}

}  // namespace flute
