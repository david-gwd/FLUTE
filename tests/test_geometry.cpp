#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "flute/geometry.hpp"

using flute::Point;
using flute::Segment;

TEST_CASE("Segments have rectilinear length", "[geometry]") {
    CHECK(flute::length({{0, 0}, {3, 0}}) == 3);
    CHECK(flute::length({{1, 5}, {1, 2}}) == 3);
    CHECK(flute::totalLength({{{0, 0}, {3, 0}}, {{3, 0}, {3, 4}}}) == 7);
    CHECK(flute::totalLength({}) == 0);
}

TEST_CASE("mergeCollinear joins end-to-end segments", "[geometry]") {
    const auto merged = flute::mergeCollinear({
        {{0, 0}, {1, 0}},
        {{1, 0}, {3, 0}},  // continues the first one
        {{3, 0}, {3, 2}},
        {{3, 2}, {3, 5}},  // continues the vertical one
        {{5, 5}, {6, 5}},  // separate
    });
    REQUIRE(merged.size() == 3);
    CHECK(flute::totalLength(merged) == 3 + 5 + 1);
    bool found_long_horizontal = false;
    for (const Segment& s : merged) {
        if (s.a == Point{0, 0} && s.b == Point{3, 0}) {
            found_long_horizontal = true;
        }
    }
    CHECK(found_long_horizontal);
}

TEST_CASE("mergeCollinear unites overlapping segments and ignores their orientation",
          "[geometry]") {
    const auto merged = flute::mergeCollinear({
        {{4, 0}, {0, 0}},  // reversed
        {{2, 0}, {6, 0}},  // overlaps the first
        {{1, 0}, {3, 0}},  // contained in the union
        {{2, 5}, {2, 5}},  // zero length, dropped
        {{7, 0}, {8, 0}},  // separated by a gap
        {{0, 1}, {0, 3}},
        {{0, 2}, {0, 4}},  // overlaps in the vertical direction
    });
    REQUIRE(merged.size() == 3);
    CHECK(flute::totalLength(merged) == 6 + 1 + 3);
}

TEST_CASE("snapToGrid moves points down to the nearest grid line", "[geometry]") {
    const flute::HananGrid grid({{0, 0}, {10, 5}, {20, 9}});
    CHECK(flute::snapToGrid(grid, {10.0001, 5.2}) == Point{10, 5});
    CHECK(flute::snapToGrid(grid, {9.999, 8.9}) == Point{0, 5});
    CHECK(flute::snapToGrid(grid, {25, 100}) == Point{20, 9});

    const auto snapped = flute::snapToGrid(grid, {{{0, 0.001}, {10.0004, 0.001}},  // becomes y = 0
                                                  {{10.0004, 5.001}, {10.0004, 9.2}},
                                                  {{20.01, 9.1}, {20.02, 9.3}}});  // collapses
    REQUIRE(snapped.size() == 2);
    CHECK(flute::totalLength(snapped) == 10 + 4);
}

TEST_CASE("mergeCollinear handles empty input and rejects diagonals", "[geometry]") {
    CHECK(flute::mergeCollinear({}).empty());
    CHECK_THROWS_AS(flute::mergeCollinear({{{0, 0}, {1, 1}}}), std::invalid_argument);
}

TEST_CASE("treeToSegments maps grid edges to coordinates", "[geometry]") {
    const flute::HananGrid grid({{10, 100}, {30, 200}, {20, 400}});
    flute::Tree tree;
    tree.addHorizontalEdge(0, 0);
    tree.addHorizontalEdge(1, 0);
    tree.addVerticalEdge(2, 0);

    const auto segments = flute::treeToSegments(grid, tree);
    REQUIRE(segments.size() == 3);
    CHECK(flute::totalLength(segments) == 10 + 10 + 100);
}

TEST_CASE("treeToSegments rejects trees that do not fit", "[geometry]") {
    const flute::HananGrid grid({{0, 0}, {1, 1}});  // 2x2 grid
    flute::Tree tree;
    tree.addHorizontalEdge(3, 0);
    CHECK_THROWS_AS(flute::treeToSegments(grid, tree), std::invalid_argument);
}

TEST_CASE("treeToSegments on a separated grid maps back and drops collapsed edges", "[geometry]") {
    // two pins share x = 0
    const flute::HananGrid original({{0, 0}, {0, 10}, {10, 5}});
    const flute::HananGrid separated = original.separated();
    REQUIRE(separated.inGeneralPosition());

    flute::Tree tree;
    for (int x = 0; x < separated.numColumns() - 1; ++x) {
        tree.addHorizontalEdge(x, 1);
    }
    // an edge between the two nearly equal x lines collapses and disappears
    const auto segments = flute::treeToSegments(original, separated, tree);
    for (const Segment& s : segments) {
        CHECK(s.a != s.b);
    }
    CHECK(flute::totalLength(segments) == 10);
}
