#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <numeric>
#include <random>
#include <sstream>

#include "flute/error.hpp"
#include "flute/geometry.hpp"
#include "flute/lookup_table.hpp"
#include "flute/lut_generator.hpp"
#include "flute/verify.hpp"
#include "reference.hpp"
#include "test_support.hpp"

using Catch::Matchers::ContainsSubstring;
using namespace flute;

TEST_CASE("factorial", "[lookup]") {
    CHECK(factorial(0) == 1);
    CHECK(factorial(5) == 120);
    CHECK(factorial(8) == 40320);
    CHECK_THROWS_AS(factorial(-1), std::invalid_argument);
    CHECK_THROWS_AS(factorial(13), std::invalid_argument);
}

TEST_CASE("Group indices and position sequences are inverse to each other", "[lookup]") {
    for (int pins = 2; pins <= 7; ++pins) {
        const std::size_t total = factorial(pins);
        std::vector<std::vector<int>> seen;
        for (std::size_t index = 0; index < total; ++index) {
            const std::vector<int> sequence = positionSequenceOfGroup(pins, index);

            // a permutation of 0..pins-1
            std::vector<int> sorted = sequence;
            std::sort(sorted.begin(), sorted.end());
            std::vector<int> identity(static_cast<std::size_t>(pins));
            std::iota(identity.begin(), identity.end(), 0);
            REQUIRE(sorted == identity);

            // classify() maps it back to the same group, possibly via a symmetry
            const GroupKey key = classify(HananGrid::fromPositionSequence(sequence));
            if (pins < 4 || index < total / 4) {
                CHECK(key.index == index);
                CHECK(key.symmetry == Symmetry::None);
            } else {
                CHECK(key.index < total / 4);
                CHECK(key.symmetry != Symmetry::None);
            }
            seen.push_back(sequence);
        }
        std::sort(seen.begin(), seen.end());
        CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());  // all distinct
    }
}

TEST_CASE("classify rejects grids it cannot handle", "[lookup]") {
    CHECK_THROWS_AS(classify(HananGrid({{0, 0}, {0, 1}, {1, 2}})),
                    std::invalid_argument);  // shared column
    std::vector<Point> nine;
    for (int i = 0; i < 9; ++i) {
        nine.push_back({double(i), double(i)});
    }
    CHECK_THROWS_AS(classify(HananGrid(nine)), std::invalid_argument);
    CHECK_THROWS_AS(positionSequenceOfGroup(3, 6), std::invalid_argument);
}

TEST_CASE("The group counts match the table layout", "[lookup]") {
    CHECK(LookupTable::groupCount(2) == 2);
    CHECK(LookupTable::groupCount(3) == 6);
    CHECK(LookupTable::groupCount(4) == 6);
    CHECK(LookupTable::groupCount(8) == 10080);
    CHECK(LookupTable::vectorWidth(5) == 8);
}

TEST_CASE("LookupTable validates its size", "[lookup]") {
    CHECK_THROWS_AS(LookupTable(1), std::invalid_argument);
    CHECK_THROWS_AS(LookupTable(9), std::invalid_argument);
    CHECK_NOTHROW(LookupTable(8));
}

TEST_CASE("Every generated tree connects all pins of its grid", "[lookup][generator]") {
    const LookupTable& table = testing::tableFor(6);
    for (int pins = 2; pins <= 6; ++pins) {
        for (std::size_t index = 0; index < LookupTable::groupCount(pins); ++index) {
            const HananGrid grid =
                HananGrid::fromPositionSequence(positionSequenceOfGroup(pins, index));
            const auto& group = table.group(pins, index);
            REQUIRE(group.size() > 0);
            REQUIRE(group.width == LookupTable::vectorWidth(pins));

            std::vector<Point> points;
            for (const Pin& pin : grid.pins()) {
                points.push_back(pin.point());
            }
            for (std::size_t i = 0; i < group.size(); ++i) {
                const auto segments = treeToSegments(grid, group.trees[i]);
                const VerificationResult result = verifyConnectedNetwork(points, segments);
                INFO("pins " << pins << " group " << index << " tree " << i << ": "
                             << result.message);
                REQUIRE(result.valid);
                // the stored wirelength vector describes the stored tree
                const auto vector = grid.wirelengthVector(group.trees[i]);
                REQUIRE(std::equal(vector.begin(), vector.end(), group.vectorOf(i)));
            }
        }
    }
}

TEST_CASE("findOptimalTree finds exact Steiner trees for random nets", "[lookup]") {
    const LookupTable& table = testing::tableFor(6);
    std::mt19937 rng(12345);
    for (int round = 0; round < 150; ++round) {
        const int pins = 2 + round % 5;  // 2..6
        std::vector<Point> points = testing::randomPins(rng, pins, 1000);
        const HananGrid grid(points);
        if (!grid.inGeneralPosition()) {
            continue;
        }

        const Tree tree = table.findOptimalTree(grid);
        const auto segments = treeToSegments(grid, tree);
        INFO("round " << round << ", " << pins << " pins");
        CHECK(verifyConnectedNetwork(points, segments).valid);
        CHECK(totalLength(segments) == testing::bruteForceRsmtLength(points));
    }
}

TEST_CASE("The two reference implementations agree", "[reference]") {
    std::mt19937 rng(77);
    for (int round = 0; round < 60; ++round) {
        const std::vector<Point> pins =
            testing::randomPins(rng, 2 + round % 5, round % 2 == 0 ? 40 : 5);
        INFO("round " << round);
        CHECK(testing::exactRsmtLength(pins) == testing::bruteForceRsmtLength(pins));
    }
}

TEST_CASE("Seven-pin lookups are exact", "[lookup]") {
    const LookupTable& table = testing::tableFor(7);
    std::mt19937 rng(8080);
    int checked = 0;
    for (int round = 0; round < 150; ++round) {
        const std::vector<Point> points = testing::randomPins(rng, 7, 1000);
        const HananGrid grid(points);
        if (!grid.inGeneralPosition()) {
            continue;
        }
        const auto segments = treeToSegments(grid, table.findOptimalTree(grid));
        CHECK(verifyConnectedNetwork(points, segments).valid);
        CHECK(totalLength(segments) == testing::exactRsmtLength(points));
        ++checked;
    }
    CHECK(checked > 100);
}

TEST_CASE("findOptimalTree needs a grid in general position within the table range", "[lookup]") {
    const LookupTable& table = testing::tableFor(5);
    CHECK_THROWS_AS(table.findOptimalTree(HananGrid({{0, 0}, {0, 1}, {1, 2}})),
                    std::invalid_argument);
    CHECK_THROWS_AS(table.findOptimalTree(HananGrid({{0, 0}})), std::invalid_argument);
    CHECK_THROWS_AS(table.findOptimalTree(HananGrid::fromPositionSequence({0, 1, 2, 3, 4, 5})),
                    std::invalid_argument);
}

TEST_CASE("Generation is deterministic with respect to the thread count", "[lookup][generator]") {
    GeneratorOptions single;
    single.maxPins = 6;
    single.threads = 1;
    GeneratorOptions parallel = single;
    parallel.threads = 4;
    std::ostringstream a;
    std::ostringstream b;
    generateLookupTable(single).write(a);
    generateLookupTable(parallel).write(b);
    CHECK(a.str() == b.str());
}

TEST_CASE("Generation reports progress", "[lookup][generator]") {
    GeneratorOptions options;
    options.maxPins = 4;
    std::size_t calls = 0;
    std::size_t last_done = 0;
    options.progress = [&](int pins, std::size_t done, std::size_t total) {
        ++calls;
        CHECK(total == LookupTable::groupCount(pins));
        CHECK(done <= total);
        last_done = done;
    };
    generateLookupTable(options);
    CHECK(calls ==
          LookupTable::groupCount(2) + LookupTable::groupCount(3) + LookupTable::groupCount(4));
    CHECK(last_done == LookupTable::groupCount(4));
}

TEST_CASE("Tables survive a write/parse round trip", "[lookup]") {
    const LookupTable& table = testing::tableFor(5);
    std::ostringstream text;
    table.write(text);

    const LookupTable parsed = LookupTable::parse(text.str());
    CHECK(parsed.maxPins() == 5);
    CHECK(parsed.treeCount() == table.treeCount());
    std::ostringstream again;
    parsed.write(again);
    CHECK(again.str() == text.str());
}

TEST_CASE("The text format is the documented one", "[lookup]") {
    const LookupTable& table = testing::tableFor(4);
    std::ostringstream text;
    table.write(text);
    CHECK(text.str().rfind("2~0\n", 0) == 0);  // starts with the first group header
    CHECK(text.str().find("\nEOG\n\n2~1\n") != std::string::npos);
}

namespace {

std::string validTable() {
    std::ostringstream text;
    testing::tableFor(4).write(text);
    return text.str();
}

std::string parseError(const std::string& text) {
    try {
        LookupTable::parse(text, "table.txt");
    } catch (const LookupTableError& e) {
        return e.what();
    }
    return "no error";
}

}  // namespace

TEST_CASE("Corrupt lookup tables are rejected with a clear message", "[lookup]") {
    const std::string valid = validTable();

    CHECK_THAT(parseError(""), ContainsSubstring("table is empty"));
    CHECK_THAT(parseError("hello\n"), ContainsSubstring("table.txt:1: expected a group header"));
    CHECK_THAT(parseError("9~0\n"), ContainsSubstring("pin count out of range"));
    CHECK_THAT(parseError("2~7\n"), ContainsSubstring("group index out of range"));

    SECTION("truncated file") {
        const std::string truncated = valid.substr(0, valid.size() / 2);
        CHECK(parseError(truncated) != "no error");
    }
    SECTION("missing group") {
        const std::size_t first_end = valid.find("EOG\n\n") + 5;
        CHECK_THAT(parseError(valid.substr(first_end)), ContainsSubstring("is missing"));
    }
    SECTION("duplicate group") {
        const std::size_t first_end = valid.find("EOG\n\n") + 5;
        CHECK_THAT(parseError(valid.substr(0, first_end) + valid),
                   ContainsSubstring("duplicate group"));
    }
    SECTION("wrong vector length") {
        std::string broken = valid;
        broken.replace(broken.find('\n') + 1, 2, "");  // drop one number of the first vector
        CHECK_THAT(parseError(broken), ContainsSubstring("wrong length"));
    }
    SECTION("bad tree string") {
        std::string broken = valid;
        const std::size_t tree_line = broken.find('\n', broken.find('\n') + 1) + 1;
        broken[tree_line] = 'x';
        CHECK_THAT(parseError(broken), ContainsSubstring("table.txt:3"));
    }
    SECTION("number out of range") {
        std::string broken = valid;
        broken.insert(broken.find('\n') + 1, "999 ");
        CHECK_THAT(parseError(broken), ContainsSubstring("invalid wirelength vector"));
    }
}

TEST_CASE("Loading a missing table fails cleanly", "[lookup]") {
    CHECK_THROWS_AS(LookupTable::load("/definitely/not/there.txt"), LookupTableError);
}

TEST_CASE("Tables can be saved and loaded from disk", "[lookup]") {
    const auto path = std::filesystem::temp_directory_path() / "flute_test_table.txt";
    testing::tableFor(4).save(path);
    const LookupTable loaded = LookupTable::load(path);
    CHECK(loaded.treeCount() == testing::tableFor(4).treeCount());
    std::filesystem::remove(path);
    CHECK_THROWS_AS(testing::tableFor(4).save("/definitely/not/there/table.txt"), LookupTableError);
}
