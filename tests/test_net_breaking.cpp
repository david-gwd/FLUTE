#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <random>

#include "flute/geometry.hpp"
#include "flute/net_breaking.hpp"
#include "flute/solver.hpp"
#include "reference.hpp"
#include "test_support.hpp"

using namespace flute;

namespace {

HananGrid gridOf(const std::vector<int>& sequence) {
    return HananGrid::fromPositionSequence(sequence);
}

}  // namespace

TEST_CASE("findLosslessCut finds cuts that separate the net into two squares", "[breaking]") {
    // rows 0..2 use only columns 0..2, rows 3..5 only columns 3..5
    const auto cut = findLosslessCut(gridOf({1, 0, 2, 4, 3, 5}));
    REQUIRE(cut.has_value());
    CHECK(cut->row == 2);
    CHECK(cut->lowerIsLeft);

    // mirrored: the lower part lies in the rightmost columns
    const auto mirrored = findLosslessCut(gridOf({4, 5, 3, 1, 2, 0}));
    REQUIRE(mirrored.has_value());
    CHECK(mirrored->row == 2);
    CHECK_FALSE(mirrored->lowerIsLeft);

    // a diagonal net: every inner row separates, the median one is returned
    const auto diagonal = findLosslessCut(gridOf({0, 1, 2, 3, 4, 5, 6}));
    REQUIRE(diagonal.has_value());
    CHECK(diagonal->row == 3);  // candidates are the rows 1..4, the median is the third
}

TEST_CASE("findLosslessCut reports nets without a lossless cut", "[breaking]") {
    CHECK_FALSE(findLosslessCut(gridOf({2, 5, 0, 4, 1, 3})).has_value());
    // too small to be cut usefully: five pins on a diagonal have the single candidate row 1
    // (parts of 3 and 4 pins); with four pins the candidate row 1 gives parts of 3 and 3
    CHECK(findLosslessCut(gridOf({0, 1, 2, 3})).has_value());
}

TEST_CASE("Breaking functions validate their input", "[breaking]") {
    CHECK_THROWS_AS(findLosslessCut(HananGrid({{0, 0}, {0, 1}, {1, 2}, {2, 3}})),
                    std::invalid_argument);
    CHECK_THROWS_AS(findLosslessCut(gridOf({0, 1})), std::invalid_argument);
    CHECK_THROWS_AS(breakingScores(gridOf({0, 1, 2, 3})), std::invalid_argument);
    CHECK_THROWS_AS(splitAtRow(gridOf({0, 1, 2, 3}), 0), std::invalid_argument);
    CHECK_THROWS_AS(splitAtRow(gridOf({0, 1, 2, 3}), 3), std::invalid_argument);
    CHECK_THROWS_AS(splitAtCorner(gridOf({0, 1, 2, 3, 4, 5}), {0, true}), std::invalid_argument);
    CHECK_THROWS_AS(splitAtCorner(gridOf({0, 1, 2, 3, 4, 5}), {4, true}), std::invalid_argument);
}

TEST_CASE("The corner is shared by both parts of a lossless cut", "[breaking]") {
    const HananGrid grid = gridOf({1, 0, 2, 4, 3, 5});
    const LosslessCut cut{2, true};
    CHECK(cornerOf(grid, cut) == Point{2, 2});  // pin (2,2) exists: no pseudo-pin needed

    const auto [lower, upper] = splitAtCorner(grid, cut);
    CHECK(lower.numPins() == 3);
    CHECK(upper.numPins() == 4);  // the corner + rows 3..5

    // a corner that is not a pin: the pin of row 2 is in column 0, the corner in column 2
    const HananGrid other = gridOf({1, 2, 0, 4, 3, 5});
    const auto [lower2, upper2] = splitAtCorner(other, {2, true});
    CHECK(lower2.numPins() == 4);  // 3 pins + corner
    CHECK(upper2.numPins() == 4);
    CHECK(cornerOf(other, {2, true}) == Point{2, 2});
}

TEST_CASE("breakingScores rank only the rows 3..n-2 (1-based)", "[breaking]") {
    const std::vector<Length> scores = breakingScores(gridOf({4, 1, 6, 3, 0, 7, 2, 5, 8}));
    REQUIRE(scores.size() == 9);
    const double minus_infinity = -std::numeric_limits<double>::infinity();
    for (const std::size_t row : {0U, 1U, 7U, 8U}) {
        CHECK(scores[row] == minus_infinity);
    }
    for (std::size_t row = 2; row <= 6; ++row) {
        CHECK(std::isfinite(scores[row]));
    }

    const GridIndex best = chooseBreakingRow(gridOf({4, 1, 6, 3, 0, 7, 2, 5, 8}));
    CHECK(best >= 2);
    CHECK(best <= 6);
    CHECK(scores[static_cast<std::size_t>(best)] ==
          *std::max_element(scores.begin(), scores.end()));
}

TEST_CASE("splitAtRow shares the breaking pin between both parts", "[breaking]") {
    const HananGrid grid = gridOf({4, 1, 6, 3, 0, 7, 2, 5, 8});
    const auto [lower, upper] = splitAtRow(grid, 4);

    CHECK(lower.numPins() == 5);  // rows 0..4
    CHECK(upper.numPins() == 5);  // rows 4..8
    CHECK(lower.yCoords().back() == 4);
    CHECK(upper.yCoords().front() == 4);
    // pin ids are renumbered 0..n-1 in both parts
    for (int id = 0; id < lower.numPins(); ++id) {
        CHECK(lower.pin(id).id == id);
    }
    for (int id = 0; id < upper.numPins(); ++id) {
        CHECK(upper.pin(id).id == id);
    }
    // both parts are consistent grids
    CHECK(lower.inGeneralPosition());
    CHECK(upper.inGeneralPosition());
    CHECK(lower.positionSequence().size() == 5);
}

TEST_CASE("transposed exchanges x and y and keeps the pin ids", "[breaking]") {
    const HananGrid grid({{1, 10}, {5, 20}, {3, 30}});
    const HananGrid flipped = transposed(grid);
    REQUIRE(flipped.numPins() == 3);
    for (int id = 0; id < 3; ++id) {
        CHECK(flipped.pin(id).x == grid.pin(id).y);
        CHECK(flipped.pin(id).y == grid.pin(id).x);
    }
    CHECK(flipped.xCoords() == grid.yCoords());
    CHECK(flipped.yCoords() == grid.xCoords());
}

TEST_CASE("chooseBreakings prefers lossless cuts", "[breaking]") {
    // Rows 0..3 use the columns 0..3, the rows 4..8 the columns 4..8: cutting is free.
    const HananGrid grid = gridOf({2, 0, 3, 1, 5, 7, 4, 8, 6});
    const auto options = chooseBreakings(grid, 3);
    REQUIRE(options.size() == 1);
    CHECK(options.front().lossless);
    CHECK(std::isinf(options.front().score));
}

TEST_CASE("chooseBreakings ranks heuristic cuts across the longer side", "[breaking]") {
    // No lossless cut exists in either direction.
    const std::vector<int> sequence{1, 5, 6, 0, 9, 4, 7, 2, 8, 3};
    std::vector<Point> tall;  // height 90, width 9 -> cut at rows
    std::vector<Point> wide;  // the same net, stretched in x -> cut at columns
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        tall.push_back({double(sequence[i]), 10.0 * double(i)});
        wide.push_back({10.0 * double(sequence[i]), double(i)});
    }
    REQUIRE_FALSE(findLosslessCut(HananGrid(tall)).has_value());
    REQUIRE_FALSE(findLosslessCut(transposed(HananGrid(tall))).has_value());

    const auto rows = chooseBreakings(HananGrid(tall), 3);
    const auto columns = chooseBreakings(HananGrid(wide), 3);
    REQUIRE(rows.size() == 3);
    REQUIRE(columns.size() == 3);
    for (const Breaking& breaking : rows) {
        CHECK(breaking.direction == Breaking::Direction::AtRow);
        CHECK_FALSE(breaking.lossless);
        CHECK(breaking.line >= 2);
        CHECK(breaking.line <= 7);
    }
    for (const Breaking& breaking : columns) {
        CHECK(breaking.direction == Breaking::Direction::AtColumn);
    }
    CHECK(rows[0].score >= rows[1].score);
    CHECK(rows[1].score >= rows[2].score);

    // fewer candidates than requested are fine, but never more
    CHECK(chooseBreakings(HananGrid(tall), 1).size() == 1);
    CHECK(chooseBreakings(HananGrid(tall), 100).size() <= 6);  // only rows 2..n-3 qualify
    CHECK_THROWS_AS(chooseBreakings(HananGrid(tall), 0), std::invalid_argument);
}

TEST_CASE("split at a column returns the left and the right part", "[breaking]") {
    const HananGrid grid = gridOf({4, 1, 6, 3, 0, 7, 2, 5, 8});
    Breaking breaking;
    breaking.direction = Breaking::Direction::AtColumn;
    breaking.line = 3;
    const auto [left, right] = split(grid, breaking);

    CHECK(left.numPins() == 4);   // columns 0..3
    CHECK(right.numPins() == 6);  // columns 3..8
    CHECK(left.xCoords().back() == 3);
    CHECK(right.xCoords().front() == 3);
    CHECK(breakingPoint(grid, breaking) == Point{3, 3});
}

TEST_CASE("Lossless cuts keep the tree optimal", "[breaking][reference]") {
    // Both parts are solved exactly with a 7-pin table; their union must be an optimal tree.
    const LookupTable& table = testing::tableFor(7);
    const RsmtSolver solver(table);
    std::mt19937 rng(606);
    int tested = 0;
    for (int round = 0; round < 4000 && tested < 60; ++round) {
        const auto pins = testing::randomPins(rng, 9 + round % 3, 100000);
        const HananGrid grid(pins);
        if (!grid.inGeneralPosition()) {
            continue;
        }
        const auto options = chooseBreakings(grid, 1);
        if (!options.front().lossless) {
            continue;
        }
        const auto [first, second] = split(grid, options.front());
        if (first.numPins() > 7 || second.numPins() > 7) {
            continue;
        }
        auto edges = solver.solve(first).edges;
        const auto more = solver.solve(second).edges;
        edges.insert(edges.end(), more.begin(), more.end());
        INFO("round " << round);
        CHECK(totalLength(mergeCollinear(edges)) == testing::exactRsmtLength(pins));
        ++tested;
    }
    CHECK(tested >= 20);
}
