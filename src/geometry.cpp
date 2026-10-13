#include "flute/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace flute {

namespace {

bool isVertical(const Segment& s) {
    return s.a.x == s.b.x;
}
bool isHorizontal(const Segment& s) {
    return s.a.y == s.b.y;
}

// Orders segments by the start point, x first (used for vertical segments).
bool lessByColumn(const Segment& lhs, const Segment& rhs) {
    if (lhs.a.x != rhs.a.x) {
        return lhs.a.x < rhs.a.x;
    }
    return lhs.a.y < rhs.a.y;
}

// Orders segments by the start point, y first (used for horizontal segments).
bool lessByRow(const Segment& lhs, const Segment& rhs) {
    if (lhs.a.y != rhs.a.y) {
        return lhs.a.y < rhs.a.y;
    }
    return lhs.a.x < rhs.a.x;
}

// Final output order: by the sum of all four coordinates.
bool lessByCoordinateSum(const Segment& lhs, const Segment& rhs) {
    return lhs.a.x + lhs.a.y + lhs.b.x + lhs.b.y < rhs.a.x + rhs.a.y + rhs.b.x + rhs.b.y;
}

/// Largest coordinate in the sorted @p lines that is <= @p value (the first line if there is
/// none). Undoes the tiny shifts of HananGrid::separated().
Coord snapDown(const std::vector<Coord>& lines, Coord value) {
    const auto above = std::upper_bound(lines.begin(), lines.end(), value);
    return above == lines.begin() ? lines.front() : *(above - 1);
}

}  // namespace

Length length(const Segment& segment) {
    return std::abs(segment.a.x - segment.b.x) + std::abs(segment.a.y - segment.b.y);
}

Length totalLength(const std::vector<Segment>& segments) {
    Length total = 0;
    for (const Segment& segment : segments) {
        total += length(segment);
    }
    return total;
}

std::vector<Segment> mergeCollinear(std::vector<Segment> segments) {
    std::vector<Segment> verticals;
    std::vector<Segment> horizontals;
    for (Segment segment : segments) {
        if (segment.a == segment.b) {
            continue;
        }
        if (isVertical(segment)) {
            if (segment.a.y > segment.b.y) {
                std::swap(segment.a, segment.b);
            }
            verticals.push_back(segment);
        } else if (isHorizontal(segment)) {
            if (segment.a.x > segment.b.x) {
                std::swap(segment.a, segment.b);
            }
            horizontals.push_back(segment);
        } else {
            throw std::invalid_argument("segment is neither horizontal nor vertical");
        }
    }
    std::sort(verticals.begin(), verticals.end(), lessByColumn);
    std::sort(horizontals.begin(), horizontals.end(), lessByRow);

    segments.clear();
    const auto mergeRuns = [&segments](const std::vector<Segment>& run, bool vertical) {
        if (run.empty()) {
            return;
        }
        Point start = run.front().a;
        Point end = run.front().b;
        for (std::size_t i = 1; i < run.size(); ++i) {
            const Segment& next = run[i];
            const bool same_line = vertical ? next.a.x == end.x : next.a.y == end.y;
            const bool touches = vertical ? next.a.y <= end.y : next.a.x <= end.x;
            if (same_line && touches) {
                if (vertical ? next.b.y > end.y : next.b.x > end.x) {
                    end = next.b;
                }
            } else {
                segments.push_back({start, end});
                start = next.a;
                end = next.b;
            }
        }
        segments.push_back({start, end});
    };
    mergeRuns(verticals, true);
    mergeRuns(horizontals, false);
    std::sort(segments.begin(), segments.end(), lessByCoordinateSum);
    return segments;
}

Point snapToGrid(const HananGrid& grid, Point point) {
    return {snapDown(grid.xCoords(), point.x), snapDown(grid.yCoords(), point.y)};
}

std::vector<Segment> snapToGrid(const HananGrid& grid, const std::vector<Segment>& segments) {
    std::vector<Segment> snapped;
    snapped.reserve(segments.size());
    for (const Segment& segment : segments) {
        snapped.push_back({snapToGrid(grid, segment.a), snapToGrid(grid, segment.b)});
    }
    return mergeCollinear(std::move(snapped));
}

std::vector<Segment> treeToSegments(const HananGrid& grid, const Tree& tree) {
    const auto& xs = grid.xCoords();
    const auto& ys = grid.yCoords();
    if (tree.maxColumn() >= static_cast<int>(xs.size()) ||
        tree.maxRow() >= static_cast<int>(ys.size())) {
        throw std::invalid_argument("tree does not fit on the grid");
    }
    std::vector<Segment> segments;
    for (int row = 0; row < Tree::kSide; ++row) {
        for (int column = 0; column < Tree::kSide - 1; ++column) {
            if (!tree.hasHorizontalEdge(column, row)) {
                continue;
            }
            const auto c = static_cast<std::size_t>(column);
            const auto r = static_cast<std::size_t>(row);
            segments.push_back({{xs.at(c), ys.at(r)}, {xs.at(c + 1), ys.at(r)}});
        }
    }
    for (int column = 0; column < Tree::kSide; ++column) {
        for (int row = 0; row < Tree::kSide - 1; ++row) {
            if (!tree.hasVerticalEdge(column, row)) {
                continue;
            }
            const auto c = static_cast<std::size_t>(column);
            const auto r = static_cast<std::size_t>(row);
            segments.push_back({{xs.at(c), ys.at(r)}, {xs.at(c), ys.at(r + 1)}});
        }
    }
    return segments;
}

std::vector<Segment> treeToSegments(const HananGrid& original, const HananGrid& separated,
                                    const Tree& tree) {
    const int max_column = tree.maxColumn();
    const int max_row = tree.maxRow();
    if (max_column >= separated.numColumns() || max_row >= separated.numRows()) {
        throw std::invalid_argument("tree does not fit on the grid");
    }
    const auto& xs = separated.xCoords();
    const auto& ys = separated.yCoords();
    const auto snap = [&](Coord x, Coord y) { return snapToGrid(original, Point{x, y}); };

    std::vector<Segment> segments;
    const auto add = [&](Point from, Point to) {
        const Point a = snap(from.x, from.y);
        const Point b = snap(to.x, to.y);
        if (a != b) {
            segments.push_back({a, b});
        }
    };
    // Same enumeration order as treeToSegments(grid, tree) would use row by row, so that the
    // result is independent of how the caller merges it afterwards.
    for (int row = 0; row <= max_row; ++row) {
        for (int column = 0; column < max_column; ++column) {
            if (tree.hasHorizontalEdge(column, row)) {
                const auto c = static_cast<std::size_t>(column);
                const auto r = static_cast<std::size_t>(row);
                add({xs[c], ys[r]}, {xs[c + 1], ys[r]});
            }
        }
        if (row == max_row) {
            break;
        }
        for (int column = 0; column <= max_column; ++column) {
            if (tree.hasVerticalEdge(column, row)) {
                const auto c = static_cast<std::size_t>(column);
                const auto r = static_cast<std::size_t>(row);
                add({xs[c], ys[r]}, {xs[c], ys[r + 1]});
            }
        }
    }
    return segments;
}

}  // namespace flute
