#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>

#include "flute/tree.hpp"

using flute::Tree;

TEST_CASE("Tree edge indices follow the documented layout", "[tree]") {
    CHECK(Tree::horizontalIndex(0, 0) == 0);
    CHECK(Tree::horizontalIndex(6, 0) == 6);
    CHECK(Tree::verticalIndex(0, 0) == 7);
    CHECK(Tree::verticalIndex(7, 0) == 14);
    CHECK(Tree::horizontalIndex(0, 1) == 15);
    CHECK(Tree::verticalIndex(7, 6) == 104);
    CHECK(Tree::horizontalIndex(6, 7) == 111);
}

TEST_CASE("Tree rejects edges outside the 8x8 grid", "[tree]") {
    Tree tree;
    CHECK_THROWS_AS(tree.addHorizontalEdge(7, 0),
                    std::out_of_range);  // only 7 horizontal edges per row
    CHECK_THROWS_AS(tree.addHorizontalEdge(0, 8), std::out_of_range);
    CHECK_THROWS_AS(tree.addVerticalEdge(8, 0), std::out_of_range);
    CHECK_THROWS_AS(tree.addVerticalEdge(0, 7),
                    std::out_of_range);  // only 7 vertical edges per column
    CHECK_THROWS_AS(tree.addHorizontalEdge(-1, 0), std::out_of_range);
    CHECK_THROWS_AS(static_cast<void>(tree.hasVerticalEdge(0, -1)), std::out_of_range);
    CHECK(tree.empty());
}

TEST_CASE("Tree edges can be added, queried and removed", "[tree]") {
    Tree tree;
    tree.addHorizontalEdge(2, 3);
    tree.addVerticalEdge(2, 3);
    CHECK(tree.hasHorizontalEdge(2, 3));
    CHECK(tree.hasVerticalEdge(2, 3));
    CHECK_FALSE(tree.hasHorizontalEdge(3, 3));
    CHECK(tree.edgeCount() == 2);
    tree.removeHorizontalEdge(2, 3);
    CHECK_FALSE(tree.hasHorizontalEdge(2, 3));
    CHECK(tree.hasVerticalEdge(2, 3));
}

TEST_CASE("Tree bit strings round-trip and are validated", "[tree]") {
    Tree tree;
    tree.addHorizontalEdge(0, 0);
    tree.addVerticalEdge(7, 6);
    const std::string bits = tree.toBitString();
    REQUIRE(bits.size() == Tree::kEdgeBits);
    CHECK(bits.back() == '1');                      // edge index 0 is the last character
    CHECK(bits[Tree::kEdgeBits - 1 - 104] == '1');  // vertical edge (7,6) has index 104
    CHECK(bits.front() == '0');                     // ... and nothing sets the highest index
    CHECK(Tree::fromBitString(bits) == tree);

    CHECK_THROWS_AS(Tree::fromBitString("0101"), std::invalid_argument);
    CHECK_THROWS_AS(Tree::fromBitString(std::string(Tree::kEdgeBits, '2')), std::invalid_argument);
}

TEST_CASE("Tree reports its extent", "[tree]") {
    Tree tree;
    CHECK(tree.maxColumn() == 0);
    CHECK(tree.maxRow() == 0);
    tree.addHorizontalEdge(1, 2);  // touches columns 1..2 in row 2
    CHECK(tree.maxColumn() == 2);
    CHECK(tree.maxRow() == 2);
    tree.addVerticalEdge(4, 2);  // touches rows 2..3 in column 4
    CHECK(tree.maxColumn() == 4);
    CHECK(tree.maxRow() == 3);
}

TEST_CASE("insertColumnAt stretches the edge across the new column", "[tree]") {
    // Path (0,0)-(1,0)-(2,0) with an upward leg at column 2.
    Tree tree;
    tree.addHorizontalEdge(0, 0);
    tree.addHorizontalEdge(1, 0);
    tree.addVerticalEdge(2, 0);

    tree.insertColumnAt(1);

    // the edge entering column 1 is stretched over columns 1-2, the rest moves right
    CHECK(tree.hasHorizontalEdge(0, 0));
    CHECK(tree.hasHorizontalEdge(1, 0));
    CHECK(tree.hasHorizontalEdge(2, 0));
    CHECK(tree.hasVerticalEdge(3, 0));
    CHECK_FALSE(tree.hasVerticalEdge(2, 0));
    CHECK(tree.edgeCount() == 4);
}

TEST_CASE("insertRowAt stretches the edge across the new row", "[tree]") {
    Tree tree;
    tree.addVerticalEdge(0, 0);
    tree.addVerticalEdge(0, 1);
    tree.addHorizontalEdge(0, 2);

    tree.insertRowAt(1);

    CHECK(tree.hasVerticalEdge(0, 0));
    CHECK(tree.hasVerticalEdge(0, 1));
    CHECK(tree.hasVerticalEdge(0, 2));
    CHECK(tree.hasHorizontalEdge(0, 3));
    CHECK_FALSE(tree.hasHorizontalEdge(0, 2));
    CHECK(tree.edgeCount() == 4);
}

TEST_CASE("mirrorHorizontally reflects the tree in its bounding square", "[tree]") {
    Tree tree;
    tree.addHorizontalEdge(0, 0);
    tree.addVerticalEdge(1, 0);
    tree.addVerticalEdge(1, 1);
    const Tree original = tree;

    tree.mirrorHorizontally();  // side = 2

    CHECK(tree.hasHorizontalEdge(1, 0));
    CHECK(tree.hasVerticalEdge(1, 0));
    CHECK(tree.hasVerticalEdge(1, 1));
    CHECK(tree.edgeCount() == 3);

    tree.mirrorHorizontally();
    CHECK(tree == original);
}

TEST_CASE("shifting moves every edge", "[tree]") {
    Tree tree;
    tree.addHorizontalEdge(0, 0);
    tree.addVerticalEdge(0, 0);
    tree.shiftRight();
    CHECK(tree.hasHorizontalEdge(1, 0));
    CHECK(tree.hasVerticalEdge(1, 0));
    tree.shiftUp();
    CHECK(tree.hasHorizontalEdge(1, 1));
    CHECK(tree.hasVerticalEdge(1, 1));
    CHECK(tree.edgeCount() == 2);
}
