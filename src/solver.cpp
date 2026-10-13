#include "flute/solver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "flute/error.hpp"
#include "flute/net_breaking.hpp"
#include "flute/verify.hpp"

namespace flute {

RsmtSolver::RsmtSolver(const LookupTable& table, SolverOptions options)
    : table_(table), options_(options) {
    if (table.maxPins() < 4) {
        throw std::invalid_argument("RsmtSolver needs a lookup table for at least 4 pins");
    }
    if (options.accuracy < 1) {
        throw std::invalid_argument("RsmtSolver: accuracy must be at least 1");
    }
}

Solution RsmtSolver::solve(const std::vector<Point>& pins) const {
    return solve(HananGrid(pins));
}

Solution RsmtSolver::solve(const HananGrid& grid) const {
    Solution solution;
    solution.edges = solveGrid(grid, options_.accuracy, solution.breakingPoints);
    return solution;
}

std::vector<Segment> RsmtSolver::solveGrid(const HananGrid& grid, int accuracy,
                                           std::vector<Point>& breakingPoints) const {
    if (grid.isSinglePoint()) {
        return {};
    }

    if (grid.hasSharedLines()) {
        // Pins on common grid lines: solve the slightly perturbed grid in general position and
        // map the tree back.
        const HananGrid separated = grid.separated();
        if (separated.hasSharedLines()) {
            throw Error("coordinates are too close to each other to be separated");
        }
        if (grid.numPins() > table_.maxPins()) {
            // The tree lives on the perturbed grid: move it (and the breaking points that were
            // chosen on it) back onto the original grid lines.
            const std::size_t first_new_point = breakingPoints.size();
            std::vector<Segment> edges =
                snapToGrid(grid, breakNet(separated, accuracy, breakingPoints));
            for (std::size_t i = first_new_point; i < breakingPoints.size(); ++i) {
                breakingPoints[i] = snapToGrid(grid, breakingPoints[i]);
            }
            return edges;
        }
        const Tree tree = table_.findOptimalTree(separated);
        return mergeCollinear(treeToSegments(grid, separated, tree));
    }
    if (grid.numPins() > table_.maxPins()) {
        return mergeCollinear(breakNet(grid, accuracy, breakingPoints));
    }
    return mergeCollinear(treeToSegments(grid, table_.findOptimalTree(grid)));
}

std::vector<Segment> RsmtSolver::breakNet(const HananGrid& grid, int accuracy,
                                          std::vector<Point>& breakingPoints) const {
    // Try the most promising ways of breaking the net and keep the shortest result. The parts
    // are solved with a lower accuracy, which keeps the total effort in check.
    const int part_accuracy = std::max(accuracy / 2, 1);
    std::vector<Segment> best_edges;
    std::vector<Point> best_points;
    Length best_length = std::numeric_limits<Length>::infinity();

    for (const Breaking& breaking : chooseBreakings(grid, accuracy)) {
        std::vector<Point> points{breakingPoint(grid, breaking)};
        const auto [first, second] = split(grid, breaking);
        std::vector<Segment> edges = solveGrid(second, part_accuracy, points);
        const std::vector<Segment> first_edges = solveGrid(first, part_accuracy, points);
        edges.insert(edges.end(), first_edges.begin(), first_edges.end());
        edges = mergeCollinear(std::move(edges));

        const Length candidate_length = totalLength(edges);
        if (candidate_length < best_length) {
            best_length = candidate_length;
            best_edges = std::move(edges);
            best_points = std::move(points);
        }
    }
    breakingPoints.insert(breakingPoints.end(), best_points.begin(), best_points.end());
    return best_edges;
}

// ---- refinement (experimental) ------------------------------------------------------------

namespace {

/// True if the tree can be cut at the grid line through @p at without leaving dangling
/// edges: every vertical edge that crosses the line next to the pin is joined to the pin's
/// column by a horizontal edge on the line.
bool isCuttable(const std::vector<Segment>& edges, Point at, Side side) {
    if (side != Side::Top && side != Side::Bottom) {
        throw std::invalid_argument("cannot cut horizontally");
    }
    std::vector<const Segment*> on_line;
    std::vector<const Segment*> crossing;
    for (const Segment& e : edges) {
        const Coord low = std::min(e.a.y, e.b.y);
        const Coord high = std::max(e.a.y, e.b.y);
        if (e.a.y == at.y && e.b.y == at.y) {
            on_line.push_back(&e);
        } else if (side == Side::Top ? (low <= at.y && high > at.y)
                                     : (low < at.y && high >= at.y)) {
            crossing.push_back(&e);
        }
    }
    for (const Segment* vertical : crossing) {
        if (vertical->a.x == at.x) {
            continue;
        }
        const Coord from = std::min(at.x, vertical->a.x);
        const Coord to = std::max(at.x, vertical->a.x);
        const bool joined = std::any_of(on_line.begin(), on_line.end(), [&](const Segment* h) {
            return std::min(h->a.x, h->b.x) == from && std::max(h->a.x, h->b.x) == to;
        });
        if (!joined) {
            return false;
        }
    }
    return true;
}

/// Length of the edges that lie completely between the two rows.
Length lengthBetween(const std::vector<Segment>& edges, Point bottom, Point top) {
    Length total = 0;
    for (const Segment& e : edges) {
        if (std::min(e.a.y, e.b.y) >= bottom.y && std::max(e.a.y, e.b.y) <= top.y) {
            total += length(e);
        }
    }
    return total;
}

/// Removes the part of the tree between the rows of @p bottom and @p top, shortening the
/// vertical edges that reach into the range.
void cutOut(std::vector<Segment>& edges, Point bottom, Point top) {
    for (std::size_t i = edges.size(); i-- > 0;) {
        const Coord low = std::min(edges[i].a.y, edges[i].b.y);
        const Coord high = std::max(edges[i].a.y, edges[i].b.y);
        const bool vertical = low != high;
        if ((low >= bottom.y && high <= top.y && vertical) || (low > bottom.y && high < top.y)) {
            edges.erase(edges.begin() + static_cast<std::ptrdiff_t>(i));
        } else if (low >= bottom.y && low <= top.y && vertical) {
            edges[i].a = {edges[i].a.x, top.y};
        } else if (high >= bottom.y && high <= top.y && vertical) {
            edges[i].b = {edges[i].a.x, bottom.y};
        }
    }
}

}  // namespace

bool RsmtSolver::refine(const HananGrid& grid, Solution& solution) const {
    const auto& rows = grid.rows();
    const auto& ys = grid.yCoords();

    // Rows of the breaking points, bottom to top.
    std::vector<Point> breaking = solution.breakingPoints;
    std::sort(breaking.begin(), breaking.end(),
              [](Point a, Point b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });

    std::vector<Segment> edges = solution.edges;
    Coord bound = -std::numeric_limits<Coord>::infinity();  // rows below were already refined

    for (const Point& point : breaking) {
        const auto found = std::lower_bound(ys.begin(), ys.end(), point.y);
        if (found == ys.end() || *found != point.y) {
            continue;
        }
        const auto center = static_cast<std::size_t>(found - ys.begin());
        if (rows[center].front().y < bound) {
            continue;
        }

        // Up to three rows below and above the breaking point.
        std::vector<Point> below;
        std::vector<Point> above;
        for (std::size_t k = std::min<std::size_t>(center, 3); k > 0; --k) {
            if (rows[center - k].front().y >= bound) {
                below.push_back(rows[center - k].front().point());
            }
        }
        for (std::size_t k = 1; k <= 3 && center + k < rows.size(); ++k) {
            above.push_back(rows[center + k].front().point());
        }

        // Keep the largest part of the neighbourhood that can be cut out of the tree.
        for (std::size_t i = above.size(); i-- > 0;) {
            if (isCuttable(edges, above[i], Side::Top)) {
                above.resize(i + 1);
                break;
            }
        }
        for (std::size_t i = 0; i < below.size(); ++i) {
            if (isCuttable(edges, below[i], Side::Bottom)) {
                below.erase(below.begin(), below.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }

        std::vector<Point> neighbourhood = below;
        neighbourhood.push_back(rows[center].front().point());
        neighbourhood.insert(neighbourhood.end(), above.begin(), above.end());
        bound = neighbourhood.back().y;

        // Re-solve only if the tree there is clearly longer than necessary.
        const Point first = neighbourhood.front();
        const Point last = neighbourhood.back();
        const Length present = lengthBetween(edges, first, last);
        const Length half_perimeter = std::abs(first.x - last.x) + std::abs(first.y - last.y);
        if (!(present > half_perimeter / 2) || neighbourhood.size() < 2) {
            continue;
        }

        cutOut(edges, first, last);
        std::vector<Point> no_breaking;  // the neighbourhood is small enough for the table
        std::vector<Segment> replacement = solveGrid(HananGrid(neighbourhood), 1, no_breaking);
        replacement.insert(replacement.end(), edges.begin(), edges.end());
        edges = mergeCollinear(std::move(replacement));
    }

    std::vector<Point> pins;
    pins.reserve(grid.pins().size());
    for (const Pin& pin : grid.pins()) {
        pins.push_back(pin.point());
    }
    if (totalLength(edges) < solution.length() && verifyConnectedNetwork(pins, edges).valid) {
        solution.edges = std::move(edges);
        return true;
    }
    return false;
}

}  // namespace flute
