#include <catch2/catch_test_macros.hpp>
#include <random>

#include "flute/error.hpp"
#include "flute/geometry.hpp"
#include "flute/solver.hpp"
#include "flute/verify.hpp"
#include "reference.hpp"
#include "test_support.hpp"

using namespace flute;

TEST_CASE("RsmtSolver needs a table that reaches at least 4 pins", "[solver]") {
    const LookupTable tiny = [] {
        GeneratorOptions options;
        options.maxPins = 3;
        return generateLookupTable(options);
    }();
    CHECK_THROWS_AS(RsmtSolver(tiny), std::invalid_argument);
}

TEST_CASE("Degenerate nets", "[solver]") {
    const RsmtSolver solver(testing::tableFor(5));

    SECTION("empty input") {
        CHECK_THROWS_AS(solver.solve(std::vector<Point>{}), std::invalid_argument);
    }
    SECTION("one pin") {
        const Solution solution = solver.solve(std::vector<Point>{{3, 4}});
        CHECK(solution.edges.empty());
        CHECK(solution.length() == 0);
    }
    SECTION("all pins at the same point") {
        const Solution solution = solver.solve(
            std::vector<Point>{{3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}});
        CHECK(solution.edges.empty());
    }
    SECTION("two pins") {
        const Solution solution = solver.solve(std::vector<Point>{{0, 0}, {3, 4}});
        CHECK(solution.length() == 7);
        CHECK(verifyConnectedNetwork({{0, 0}, {3, 4}}, solution.edges).valid);
    }
    SECTION("pins on a line") {
        const std::vector<Point> pins{{0, 5}, {10, 5}, {4, 5}, {7, 5}};
        const Solution solution = solver.solve(pins);
        CHECK(solution.length() == 10);
        CHECK(solution.edges.size() == 1);
    }
}

TEST_CASE("Collinear nets larger than the table are solved exactly", "[solver]") {
    const RsmtSolver solver(testing::tableFor(5));
    std::vector<Point> vertical;
    std::vector<Point> horizontal;
    for (int i = 0; i < 20; ++i) {
        vertical.push_back({2, double(i * 3 % 20)});
        horizontal.push_back({double(i * 7 % 20), 5});
    }
    for (const auto& pins : {vertical, horizontal}) {
        const Solution solution = solver.solve(pins);
        CHECK(solution.length() == 19);
        CHECK(verifyConnectedNetwork(pins, solution.edges).valid);
    }
}

TEST_CASE("Nets covered by the table are solved optimally", "[solver]") {
    const RsmtSolver solver(testing::tableFor(6));
    std::mt19937 rng(2024);
    for (int round = 0; round < 250; ++round) {
        const int count = 2 + round % 5;              // 2..6
        const int range = round % 2 == 0 ? 1000 : 6;  // second half: many coinciding coordinates
        const std::vector<Point> pins = testing::randomPins(rng, count, range);

        const Solution solution = solver.solve(pins);
        INFO("round " << round << ": " << count << " pins, coordinate range " << range);
        CHECK(solution.breakingPoints.empty());
        CHECK(verifyConnectedNetwork(pins, solution.edges).valid);
        CHECK(solution.length() == testing::bruteForceRsmtLength(pins));
    }
}

TEST_CASE("Large nets give valid trees between the HPWL bound and the MST", "[solver]") {
    const RsmtSolver solver(testing::tableFor(5));  // small table: forces deep net breaking
    std::mt19937 rng(7);
    double ratio_sum = 0;
    for (int round = 0; round < 60; ++round) {
        const int count = 6 + round % 40;
        const int range = round % 3 == 0 ? 20 : 100000;  // coarse grids create shared lines
        const std::vector<Point> pins = testing::randomPins(rng, count, range);

        const Solution solution = solver.solve(pins);
        INFO("round " << round << ": " << count << " pins, coordinate range " << range);
        const VerificationResult verification = verifyConnectedNetwork(pins, solution.edges);
        INFO(verification.message);
        REQUIRE(verification.valid);
        const double mst = testing::manhattanMstLength(pins);
        CHECK(solution.length() >= testing::halfPerimeter(pins));
        CHECK(solution.length() <= 1.35 * mst);  // sanity bound, individual nets can be unlucky
        ratio_sum += solution.length() / mst;
    }
    // On average the trees are clearly shorter than the minimum spanning tree.
    CHECK(ratio_sum / 60 < 1.0);
}

TEST_CASE("The accuracy option must be positive and more accuracy never hurts on average",
          "[solver]") {
    CHECK_THROWS_AS(RsmtSolver(testing::tableFor(5), SolverOptions{0}), std::invalid_argument);

    std::mt19937 rng(11);
    double fast_total = 0;
    double careful_total = 0;
    const RsmtSolver fast(testing::tableFor(6), SolverOptions{1});
    const RsmtSolver careful(testing::tableFor(6), SolverOptions{4});
    for (int round = 0; round < 25; ++round) {
        const std::vector<Point> pins = testing::randomPins(rng, 10 + round, 100000);
        fast_total += fast.solve(pins).length();
        careful_total += careful.solve(pins).length();
    }
    CHECK(careful_total <= fast_total);
}

TEST_CASE("The table size does not change small results, larger tables are never worse",
          "[solver]") {
    const RsmtSolver small(testing::tableFor(5));
    const RsmtSolver large(testing::tableFor(7));
    std::mt19937 rng(99);
    double small_total = 0;
    double large_total = 0;
    for (int round = 0; round < 30; ++round) {
        const std::vector<Point> pins = testing::randomPins(rng, 7 + round % 10, 5000);
        small_total += small.solve(pins).length();
        large_total += large.solve(pins).length();
    }
    CHECK(large_total <= small_total * 1.001);
}

TEST_CASE("Solving is deterministic", "[solver]") {
    const RsmtSolver solver(testing::tableFor(5));
    std::mt19937 rng(5);
    const std::vector<Point> pins = testing::randomPins(rng, 40, 300);
    const Solution a = solver.solve(pins);
    const Solution b = solver.solve(pins);
    REQUIRE(a.edges.size() == b.edges.size());
    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        CHECK(a.edges[i].a == b.edges[i].a);
        CHECK(a.edges[i].b == b.edges[i].b);
    }
}

TEST_CASE("refine never returns an invalid or longer tree", "[solver][refine]") {
    const RsmtSolver solver(testing::tableFor(5));
    std::mt19937 rng(31337);
    for (int round = 0; round < 25; ++round) {
        const std::vector<Point> pins = testing::randomPins(rng, 15 + round, 100000);
        const HananGrid grid(pins);
        Solution solution = solver.solve(grid);
        const double before = solution.length();

        solver.refine(grid, solution);
        INFO("round " << round);
        CHECK(solution.length() <= before);
        CHECK(verifyConnectedNetwork(pins, solution.edges).valid);
    }
}

TEST_CASE("verifyConnectedNetwork detects broken trees", "[verify]") {
    const std::vector<Point> pins{{0, 0}, {4, 0}, {4, 4}};
    CHECK(verifyConnectedNetwork(pins, {{{0, 0}, {4, 0}}, {{4, 0}, {4, 4}}}).valid);
    // Steiner point inside a long segment (T junction)
    CHECK(verifyConnectedNetwork({{0, 0}, {8, 0}, {4, 4}}, {{{0, 0}, {8, 0}}, {{4, 0}, {4, 4}}})
              .valid);
    // missing edge
    CHECK_FALSE(verifyConnectedNetwork(pins, {{{0, 0}, {4, 0}}}).valid);
    // disconnected pieces
    CHECK_FALSE(verifyConnectedNetwork(pins, {{{0, 0}, {3, 0}}, {{4, 0}, {4, 4}}}).valid);
    // diagonal
    CHECK_FALSE(verifyConnectedNetwork(pins, {{{0, 0}, {4, 4}}}).valid);
    // overlapping collinear pieces are connected
    CHECK(verifyConnectedNetwork({{0, 0}, {6, 0}}, {{{0, 0}, {4, 0}}, {{2, 0}, {6, 0}}}).valid);
}

TEST_CASE("Hundreds of pins on a few grid lines are separated and solved", "[solver]") {
    // 400 pins on a 9x9 lattice: every grid line carries dozens of pins, far more than the
    // geometric shifts of a naive separation scheme can distinguish in double precision.
    const RsmtSolver solver(testing::tableFor(6));
    std::mt19937 rng(2718);
    for (const int range : {8, 30}) {
        const std::vector<Point> pins = testing::randomPins(rng, 400, range);
        const Solution solution = solver.solve(pins);
        INFO("coordinate range " << range);
        const VerificationResult verification = verifyConnectedNetwork(pins, solution.edges);
        REQUIRE(verification.valid);
        CHECK(solution.length() >= testing::halfPerimeter(pins));
        CHECK(solution.length() <= 1.25 * testing::manhattanMstLength(pins));  // sanity bound only
    }
}

TEST_CASE("Large coordinates with tiny gaps are rejected instead of mis-solved", "[solver]") {
    // The two pins differ by less than the resolution of doubles at 1e17 after shifting.
    const RsmtSolver solver(testing::tableFor(5));
    std::vector<Point> pins;
    for (int i = 0; i < 12; ++i) {
        pins.push_back({1e17, 1e17 + 32.0 * i});  // same x, y spaced by 32 (ulp at 1e17 is 16)
    }
    // Either solved correctly or rejected with a clear error, never an invalid tree.
    try {
        const Solution solution = solver.solve(pins);
        CHECK(verifyConnectedNetwork(pins, solution.edges).valid);
    } catch (const flute::Error& e) {
        CHECK(std::string(e.what()).find("too close") != std::string::npos);
    }
}
