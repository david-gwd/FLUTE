#include "flute/lut_generator.hpp"

#include <algorithm>
#include <atomic>
#include <exception>
#include <initializer_list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace flute {

namespace {

using Trees = std::vector<Tree>;

Trees generate(const HananGrid& grid);

// ---- boundary compaction and expansion --------------------------------------------------

/// The pins on the boundary line of @p side.
const std::vector<Pin>& boundaryPins(const HananGrid& grid, Side side) {
    switch (side) {
        case Side::Right:
            return grid.columns().back();
        case Side::Left:
            return grid.columns().front();
        case Side::Top:
            return grid.rows().back();
        case Side::Bottom:
            return grid.rows().front();
    }
    throw std::logic_error("invalid side");
}

int distinctOnBoundary(const HananGrid& grid, Side side) {
    return countDistinctPositions(boundaryPins(grid, side));
}

int distinctOnBoundaries(const HananGrid& grid, std::initializer_list<Side> sides) {
    std::vector<Pin> pins;
    for (const Side side : sides) {
        const auto& line = boundaryPins(grid, side);
        pins.insert(pins.end(), line.begin(), line.end());
    }
    return countDistinctPositions(pins);
}

/// Moves the pins of the boundary line one grid line inwards so that they merge with their
/// neighbour line.
HananGrid compact(const HananGrid& grid, Side side) {
    const auto& columns = grid.columns();
    const auto& rows = grid.rows();
    std::vector<std::pair<PinId, Point>> moves;
    for (const Pin& pin : boundaryPins(grid, side)) {
        switch (side) {
            case Side::Right:
                moves.push_back({pin.id, {columns[columns.size() - 2].front().x, pin.y}});
                break;
            case Side::Left:
                moves.push_back({pin.id, {columns[1].front().x, pin.y}});
                break;
            case Side::Top:
                moves.push_back({pin.id, {pin.x, rows[rows.size() - 2].front().y}});
                break;
            case Side::Bottom:
                moves.push_back({pin.id, {pin.x, rows[1].front().y}});
                break;
        }
    }
    HananGrid compacted = grid;
    compacted.movePins(moves);
    return compacted;
}

/// Undoes compact(): re-inserts the removed boundary line into every tree. @p original is
/// the grid that was compacted, @p compacted the result of compact().
void expand(const HananGrid& original, const HananGrid& compacted, Side side, Trees& trees) {
    const auto& pins = boundaryPins(original, side);
    for (Tree& tree : trees) {
        if (side == Side::Left) {
            tree.shiftRight();
        }
        if (side == Side::Bottom) {
            tree.shiftUp();
        }
        for (const Pin& pin : pins) {
            const GridPoint position = compacted.position(pin.id);
            if (side == Side::Right || side == Side::Left) {
                tree.addHorizontalEdge(position.column, position.row);
            } else {
                tree.addVerticalEdge(position.column, position.row);
            }
        }
    }
}

/// POSTs obtained by compacting @p side, solving the smaller grid and expanding the result.
Trees viaCompaction(const HananGrid& grid, Side side) {
    const HananGrid compacted = compact(grid, side);
    Trees trees = generate(compacted);
    expand(grid, compacted, side, trees);
    return trees;
}

void append(Trees& target, const Trees& source) {
    target.insert(target.end(), source.begin(), source.end());
}

// ---- pruning ------------------------------------------------------------------------------

/// True if @p a <= @p b in every component.
bool dominates(const std::vector<PowvEntry>& a, const std::vector<PowvEntry>& b) {
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] > b[i]) {
            return false;
        }
    }
    return true;
}

/// Removes trees that are never better than another tree: trees that leave a gap unused, and
/// trees whose wirelength vector is componentwise >= that of an earlier or later tree
/// (of equal vectors, the first is kept).
void prune(const HananGrid& grid, Trees& trees) {
    const std::size_t count = trees.size();
    std::vector<std::vector<PowvEntry>> vectors;
    vectors.reserve(count);
    for (const Tree& tree : trees) {
        vectors.push_back(grid.wirelengthVector(tree));
    }

    std::vector<bool> pruned(count, false);
    for (std::size_t s = 0; s < count; ++s) {
        if (std::find(vectors[s].begin(), vectors[s].end(), PowvEntry{0}) != vectors[s].end()) {
            pruned[s] = true;
        }
        if (pruned[s]) {
            continue;
        }
        for (std::size_t t = 0; t < count; ++t) {
            if (s != t && !pruned[t] && dominates(vectors[s], vectors[t])) {
                pruned[t] = true;
            }
        }
    }
    std::size_t kept = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!pruned[i]) {
            trees[kept++] = trees[i];
        }
    }
    trees.resize(kept);
}

// ---- special constructions ----------------------------------------------------------------

/// For 7 pins that all lie on the boundary: the trees are the boundary ring with the edges
/// between two neighbouring pins removed.
Trees nearRingTrees(const HananGrid& grid) {
    const Coord x_max = grid.xCoords().back();
    const Coord y_min = grid.yCoords().front();

    // The pins in anticlockwise order, starting at the bottom left.
    std::vector<Pin> ring;
    std::vector<Pin> rest;
    for (const Pin& pin : grid.pins()) {
        (pin.x == x_max || pin.y == y_min ? ring : rest).push_back(pin);
    }
    std::sort(ring.begin(), ring.end(),
              [](const Pin& a, const Pin& b) { return a.x == b.x ? a.y < b.y : a.x < b.x; });
    std::sort(rest.begin(), rest.end(),
              [](const Pin& a, const Pin& b) { return a.x == b.x ? b.y < a.y : b.x < a.x; });
    ring.insert(ring.end(), rest.begin(), rest.end());

    const GridIndex last_column = grid.numColumns() - 1;
    const GridIndex last_row = grid.numRows() - 1;
    Tree full_ring;
    for (GridIndex x = 0; x < last_column; ++x) {
        full_ring.addHorizontalEdge(x, 0);
        full_ring.addHorizontalEdge(x, last_row);
    }
    for (GridIndex y = 0; y < last_row; ++y) {
        full_ring.addVerticalEdge(0, y);
        full_ring.addVerticalEdge(last_column, y);
    }

    Trees trees;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const GridPoint a = grid.position(ring[i].id);
        const GridPoint b = grid.position(ring[(i + 1) % ring.size()].id);
        if (a.column == b.column && a.row == b.row) {
            continue;
        }

        Tree tree = full_ring;
        const GridIndex x_from = std::min(a.column, b.column);
        const GridIndex x_to = std::max(a.column, b.column);
        const GridIndex y_from = std::min(a.row, b.row);
        const GridIndex y_to = std::max(a.row, b.row);
        if (a.column == b.column) {
            // both pins on the left or right border
            for (GridIndex y = y_from; y < y_to; ++y) {
                tree.removeVerticalEdge(a.column, y);
            }
        } else if (a.row == b.row) {
            // both pins on the top or bottom border
            for (GridIndex x = x_from; x < x_to; ++x) {
                tree.removeHorizontalEdge(x, a.row);
            }
        } else if (a.column == last_column || a.column == 0) {
            // the pins are separated by the top-right or the bottom-left corner
            for (GridIndex x = x_from; x < x_to; ++x) {
                tree.removeHorizontalEdge(x, b.row);
            }
            for (GridIndex y = y_from; y < y_to; ++y) {
                tree.removeVerticalEdge(a.column, y);
            }
        } else {
            // the pins are separated by the bottom-right or the top-left corner
            for (GridIndex x = x_from; x < x_to; ++x) {
                tree.removeHorizontalEdge(x, a.row);
            }
            for (GridIndex y = y_from; y < y_to; ++y) {
                tree.removeVerticalEdge(b.column, y);
            }
        }
        trees.push_back(tree);
    }
    return trees;
}

/// Adds, for one boundary side, the trees in which the pins of that side are connected
/// along the boundary: for every pin of the side, the other pins of the side are projected
/// onto it, the resulting smaller grid is solved, and the removed lines and the boundary
/// path are put back.
void appendBoundaryPathTrees(const HananGrid& grid, Side side, Trees& result) {
    const bool along_x = side == Side::Top || side == Side::Bottom;
    const auto& line = boundaryPins(grid, side);
    const int allowed_span = grid.numPins() - 3;

    GridIndex low = along_x ? grid.numColumns() : grid.numRows();
    GridIndex high = 0;
    for (const Pin& pin : line) {
        const GridPoint position = grid.position(pin.id);
        const GridIndex index = along_x ? position.column : position.row;
        low = std::min(low, index);
        high = std::max(high, index);
    }
    if (high - low >= allowed_span + 1) {
        return;
    }

    const auto num_lines = static_cast<std::size_t>(along_x ? grid.numColumns() : grid.numRows());
    for (const Pin& target : line) {
        const GridPoint target_position = grid.position(target.id);
        const GridIndex target_index = along_x ? target_position.column : target_position.row;
        const Coord reference =
            along_x ? grid.columns()[static_cast<std::size_t>(target_index)].front().x
                    : grid.rows()[static_cast<std::size_t>(target_index)].front().y;

        // Project the boundary pins onto the reference line; lines that become empty are
        // hidden and have to be re-inserted into the trees afterwards.
        std::vector<bool> hidden(num_lines, false);
        std::vector<std::pair<PinId, Point>> moves;
        for (const Pin& pin : line) {
            const GridPoint position = grid.position(pin.id);
            if (along_x) {
                const auto& column = grid.columns()[static_cast<std::size_t>(position.column)];
                if (pin.x != reference && countDistinctPositions(column) == 1) {
                    hidden[static_cast<std::size_t>(position.column)] = true;
                }
                moves.push_back({pin.id, {reference, pin.y}});
            } else {
                const auto& row = grid.rows()[static_cast<std::size_t>(position.row)];
                if (pin.y != reference && countDistinctPositions(row) == 1) {
                    hidden[static_cast<std::size_t>(position.row)] = true;
                }
                moves.push_back({pin.id, {pin.x, reference}});
            }
        }
        HananGrid projected = grid;
        projected.movePins(moves);

        Trees trees = generate(projected);
        for (Tree& tree : trees) {
            for (std::size_t k = 0; k < num_lines; ++k) {
                if (!hidden[k]) {
                    continue;
                }
                if (along_x) {
                    tree.insertColumnAt(static_cast<int>(k));
                } else {
                    tree.insertRowAt(static_cast<int>(k));
                }
            }
            for (GridIndex i = low; i < high; ++i) {
                switch (side) {
                    case Side::Top:
                        tree.addHorizontalEdge(i, grid.numRows() - 1);
                        break;
                    case Side::Bottom:
                        tree.addHorizontalEdge(i, 0);
                        break;
                    case Side::Right:
                        tree.addVerticalEdge(grid.numColumns() - 1, i);
                        break;
                    case Side::Left:
                        tree.addVerticalEdge(0, i);
                        break;
                }
            }
        }
        append(result, trees);
    }
}

Trees boundaryPathTrees(const HananGrid& grid) {
    Trees result;
    for (const Side side : {Side::Top, Side::Bottom, Side::Right, Side::Left}) {
        appendBoundaryPathTrees(grid, side, result);
    }
    return result;
}

// ---- the recursion ----------------------------------------------------------------------

Trees straightLine(GridIndex length, bool horizontal) {
    Tree tree;
    for (GridIndex i = 0; i < length; ++i) {
        if (horizontal) {
            tree.addHorizontalEdge(i, 0);
        } else {
            tree.addVerticalEdge(0, i);
        }
    }
    return {tree};
}

/// POSTs for a corner configuration: two boundaries that share one pin. Solved by
/// compacting either boundary.
Trees viaTwoCompactions(const HananGrid& grid, Side first, Side second) {
    Trees trees = viaCompaction(grid, first);
    append(trees, viaCompaction(grid, second));
    prune(grid, trees);
    return trees;
}

Trees generate(const HananGrid& grid) {
    // The grid is a single row or column: the only tree is the line.
    if (grid.numRows() == 1) {
        return straightLine(grid.numColumns() - 1, true);
    }
    if (grid.numColumns() == 1) {
        return straightLine(grid.numRows() - 1, false);
    }

    // 2x2 box with pins in all four corners: the two shapes that use three of its four sides.
    if (grid.numColumns() == 2 && grid.numRows() == 2 && grid.numPins() > 3) {
        Tree open_left;  // top, right and bottom side
        open_left.addHorizontalEdge(0, 1);
        open_left.addVerticalEdge(1, 0);
        Tree open_bottom = open_left;  // left, top and right side
        open_left.addHorizontalEdge(0, 0);
        open_bottom.addVerticalEdge(0, 0);
        return {open_bottom, open_left};
    }

    // A boundary with a single pin can simply be compacted.
    for (const Side side : {Side::Right, Side::Left, Side::Top, Side::Bottom}) {
        if (distinctOnBoundary(grid, side) == 1) {
            return viaCompaction(grid, side);
        }
    }

    // A corner with one extra pin on each of its two boundaries.
    for (const Side vertical : {Side::Right, Side::Left}) {
        for (const Side horizontal : {Side::Top, Side::Bottom}) {
            if (distinctOnBoundary(grid, vertical) == 2 &&
                distinctOnBoundary(grid, horizontal) == 2 &&
                distinctOnBoundaries(grid, {vertical, horizontal}) == 3) {
                return viaTwoCompactions(grid, vertical, horizontal);
            }
        }
    }

    Trees trees;
    const int boundary_pins =
        distinctOnBoundaries(grid, {Side::Left, Side::Right, Side::Bottom, Side::Top});
    if (grid.numPins() == 7 && boundary_pins == 7) {
        // Lemma 5 of the paper: for seven pins the near-ring trees complete boundary compaction.
        append(trees, nearRingTrees(grid));
    } else if (grid.numPins() >= 8 && boundary_pins >= 7) {
        // Lemma 6: from eight pins on, trees with a branch along a boundary are needed. (The
        // near-ring trees must not be used here: they ignore pins inside the ring.)
        append(trees, boundaryPathTrees(grid));
    }
    for (const Side side : {Side::Left, Side::Right, Side::Top, Side::Bottom}) {
        append(trees, viaCompaction(grid, side));
    }
    prune(grid, trees);
    return trees;
}

}  // namespace

std::vector<Tree> generateTrees(const HananGrid& grid) {
    if (grid.numPins() > kMaxLookupPins) {
        throw std::invalid_argument("generateTrees: too many pins");
    }
    return generate(grid);
}

LookupTable generateLookupTable(const GeneratorOptions& options) {
    LookupTable table(options.maxPins);
    unsigned threads = options.threads != 0 ? options.threads : std::thread::hardware_concurrency();
    threads = std::max(threads, 1U);

    for (int pins = 2; pins <= options.maxPins; ++pins) {
        const std::size_t total = LookupTable::groupCount(pins);
        std::vector<LookupTable::Group> groups(total);
        std::atomic<std::size_t> next{0};
        std::size_t done = 0;
        std::mutex mutex;
        std::exception_ptr failure;

        const auto worker = [&] {
            try {
                for (std::size_t index = next++; index < total; index = next++) {
                    const HananGrid grid =
                        HananGrid::fromPositionSequence(positionSequenceOfGroup(pins, index));
                    const Trees trees = generate(grid);
                    LookupTable::Group& group = groups[index];
                    group.width = LookupTable::vectorWidth(pins);
                    group.trees = trees;
                    for (const Tree& tree : trees) {
                        const auto vector = grid.wirelengthVector(tree);
                        group.powv.insert(group.powv.end(), vector.begin(), vector.end());
                    }
                    if (options.progress) {
                        const std::lock_guard<std::mutex> lock(mutex);
                        options.progress(pins, ++done, total);
                    }
                }
            } catch (...) {
                const std::lock_guard<std::mutex> lock(mutex);
                if (!failure) {
                    failure = std::current_exception();
                }
                next = total;  // stop the other workers
            }
        };

        std::vector<std::thread> pool;
        const auto worker_count = static_cast<unsigned>(std::min<std::size_t>(threads, total));
        pool.reserve(worker_count);
        for (unsigned i = 0; i < worker_count; ++i) {
            pool.emplace_back(worker);
        }
        for (std::thread& thread : pool) {
            thread.join();
        }
        if (failure) {
            std::rethrow_exception(failure);
        }

        for (std::size_t index = 0; index < total; ++index) {
            table.setGroup(pins, index, std::move(groups[index]));
        }
    }
    return table;
}

}  // namespace flute
