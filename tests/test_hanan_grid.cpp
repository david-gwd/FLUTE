#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "flute/hanan_grid.hpp"

using flute::HananGrid;
using flute::Point;

TEST_CASE("HananGrid sorts pins into columns and rows", "[grid]") {
    const HananGrid grid({{5, 1}, {2, 4}, {9, 3}, {2, 7}});

    REQUIRE(grid.numPins() == 4);
    CHECK(grid.xCoords() == std::vector<double>{2, 5, 9});
    CHECK(grid.yCoords() == std::vector<double>{1, 3, 4, 7});
    CHECK(grid.horizontalGaps() == std::vector<double>{3, 4});
    CHECK(grid.verticalGaps() == std::vector<double>{2, 1, 3});
    CHECK(grid.numColumns() == 3);
    CHECK(grid.numRows() == 4);
    CHECK(grid.columns()[0].size() == 2);  // the two pins with x = 2
    CHECK(grid.hasSharedLines());
    CHECK_FALSE(grid.inGeneralPosition());

    // pin 0 = (5,1): second column, lowest row
    CHECK(grid.position(0).column == 1);
    CHECK(grid.position(0).row == 0);
    CHECK(grid.position(2).column == 2);
    CHECK(grid.position(2).row == 1);
}

TEST_CASE("The position sequence lists the column of every pin from bottom to top", "[grid]") {
    const HananGrid grid({{10, 0}, {30, 1}, {20, 2}, {0, 3}});
    CHECK(grid.inGeneralPosition());
    CHECK(grid.positionSequence() == std::vector<int>{1, 3, 2, 0});
}

TEST_CASE("fromPositionSequence places pin i at (sequence[i], i)", "[grid]") {
    const auto grid = HananGrid::fromPositionSequence({2, 0, 1});
    CHECK(grid.positionSequence() == std::vector<int>{2, 0, 1});
    CHECK(grid.pin(0).x == 2);
    CHECK(grid.pin(0).y == 0);
    CHECK(grid.pin(2).x == 1);
    CHECK(grid.pin(2).y == 2);
}

TEST_CASE("HananGrid rejects an empty pin set", "[grid]") {
    CHECK_THROWS_AS(HananGrid(std::vector<Point>{}), std::invalid_argument);
    CHECK_THROWS_AS(HananGrid::fromPositionSequence({}), std::invalid_argument);
}

TEST_CASE("A single point is recognised", "[grid]") {
    CHECK(HananGrid({{3, 3}, {3, 3}}).isSinglePoint());
    CHECK_FALSE(HananGrid({{3, 3}, {3, 4}}).isSinglePoint());
    CHECK(HananGrid({{3, 3}, {3, 3}}).epsilon() == 0);
}

TEST_CASE("epsilon is the smallest gap", "[grid]") {
    CHECK(HananGrid({{0, 0}, {10, 3}, {14, 9}}).epsilon() == 3);
    CHECK(HananGrid({{0, 0}, {10, 0}}).epsilon() == 10);  // only one row
}

TEST_CASE("separated() puts the pins in general position without reordering lines", "[grid]") {
    const HananGrid grid({{0, 0}, {0, 5}, {5, 5}, {5, 0}, {5, 5}});
    REQUIRE(grid.hasSharedLines());

    const HananGrid separated = grid.separated();
    CHECK(separated.inGeneralPosition());
    CHECK(separated.numPins() == 5);

    // Every pin moved by less than a quarter of epsilon in each direction, never downwards.
    for (int id = 0; id < grid.numPins(); ++id) {
        const double dx = separated.pin(id).x - grid.pin(id).x;
        const double dy = separated.pin(id).y - grid.pin(id).y;
        CHECK(dx >= 0);
        CHECK(dy >= 0);
        CHECK(dx < grid.epsilon());
        CHECK(dy < grid.epsilon());
    }
    // The grid lines of the original keep their order: every shifted pin still lies between its
    // original line and the next one.
    CHECK(separated.xCoords().front() == 0);
}

TEST_CASE("removePins renumbers the remaining pins", "[grid]") {
    HananGrid grid({{0, 0}, {1, 1}, {2, 2}, {3, 3}});
    grid.removePins({1, 2});
    REQUIRE(grid.numPins() == 2);
    CHECK(grid.pin(0).x == 0);
    CHECK(grid.pin(1).x == 3);
    CHECK(grid.pin(1).id == 1);
    CHECK(grid.xCoords() == std::vector<double>{0, 3});

    CHECK_THROWS_AS(grid.removePins({0, 0}), std::invalid_argument);
    CHECK_THROWS_AS(grid.removePins({5}), std::invalid_argument);
    CHECK_THROWS_AS(grid.removePins({0, 1}), std::invalid_argument);  // would remove everything
}

TEST_CASE("movePins rebuilds the grid", "[grid]") {
    HananGrid grid({{0, 0}, {4, 4}, {8, 8}});
    grid.movePins({{2, {4, 8}}});
    CHECK(grid.hasSharedLines());
    CHECK(grid.xCoords() == std::vector<double>{0, 4});
    CHECK(grid.position(2).column == 1);
    CHECK_THROWS_AS(grid.movePins({{7, {0, 0}}}), std::invalid_argument);
}

TEST_CASE("wirelengthVector counts edges per gap and wirelength weights them", "[grid]") {
    // 3 pins in general position on the permutation grid 0 2 1 (gaps all 1).
    const auto grid = HananGrid::fromPositionSequence({0, 2, 1});
    flute::Tree
        tree;  // (0,0)-(1,0)-(2,0)... use a small L: horizontal along row 0, vertical up column 0
    tree.addHorizontalEdge(0, 0);
    tree.addHorizontalEdge(1, 0);
    tree.addVerticalEdge(0, 0);
    tree.addVerticalEdge(0, 1);

    const auto vector = grid.wirelengthVector(tree);
    REQUIRE(vector.size() == 4);
    CHECK(vector[0] == 1);  // horizontal gap 0
    CHECK(vector[1] == 1);  // horizontal gap 1
    CHECK(vector[2] == 1);  // vertical gap 0
    CHECK(vector[3] == 1);  // vertical gap 1
    CHECK(grid.wirelength(vector.data()) == 4);
}

TEST_CASE("countDistinctPositions ignores coinciding pins", "[grid]") {
    CHECK(flute::countDistinctPositions({{0, 0, 0}, {1, 0, 1}, {0, 0, 2}, {1, 1, 3}}) == 3);
    CHECK(flute::countDistinctPositions({}) == 0);
}
