#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <sstream>

#include "flute/error.hpp"
#include "flute/io.hpp"

using Catch::Matchers::ContainsSubstring;

namespace {

std::vector<flute::Point> parse(const std::string& text) {
    std::istringstream in(text);
    return flute::readStp(in, "test.stp");
}

const std::string kValid =
    "33d32945 STP File, STP Format Version 1.00\n"
    "Section Graph\nNodes 4\nEdges 0\nEnd\n\n"
    "Section Terminals\nTerminals 3\nT 1\nT 3\nT 4\nEnd\n\n"
    "Section Coordinates\nDD 1 0 0\nDD 2 5 5\nDD 3 10.5 2\nDD 4 -1 7\nEnd\n\nEOF\n";

}  // namespace

TEST_CASE("readStp returns the terminals in coordinate order", "[io]") {
    const auto pins = parse(kValid);
    REQUIRE(pins.size() == 3);
    CHECK(pins[0] == flute::Point{0, 0});
    CHECK(pins[1] == flute::Point{10.5, 2});
    CHECK(pins[2] == flute::Point{-1, 7});
}

TEST_CASE("readStp tolerates windows line endings and trailing blanks", "[io]") {
    std::string text = kValid;
    for (std::size_t at = text.find('\n'); at != std::string::npos; at = text.find('\n', at + 2)) {
        text.insert(at, "\r");
    }
    CHECK(parse(text).size() == 3);
    CHECK(parse("Section Terminals\nTerminals 1\nT 1\nEnd\nSection Coordinates\nDD 1 4 5   \nEnd\n")
              .size() == 1);
}

TEST_CASE("readStp reports malformed instances with file and line", "[io]") {
    const auto message = [](const std::string& text) {
        try {
            parse(text);
        } catch (const flute::ParseError& e) {
            return std::string(e.what());
        }
        return std::string("no error");
    };

    CHECK_THAT(message(""), ContainsSubstring("no terminals"));
    CHECK_THAT(message("T 1\nT 1\nDD 1 0 0\n"),
               ContainsSubstring("test.stp:2: duplicate terminal"));
    CHECK_THAT(message("T 1\nDD 1 0 0\nDD 1 1 1\n"),
               ContainsSubstring("test.stp:3: duplicate coordinates"));
    CHECK_THAT(message("T 1\nT 2\nDD 1 0 0\n"), ContainsSubstring("terminal 2 has no coordinates"));
    CHECK_THAT(message("T 1\nDD 1 0\n"), ContainsSubstring("test.stp:2: expected 'DD"));
    CHECK_THAT(message("T 1\nDD 1 0 abc\n"), ContainsSubstring("expected 'DD"));
    CHECK_THAT(message("T 1\nDD 1 0 nan\n"), ContainsSubstring("expected 'DD"));
    CHECK_THAT(message("T 1\nDD 1 0 1e999\n"), ContainsSubstring("expected 'DD"));
    CHECK_THAT(message("T x\n"), ContainsSubstring("test.stp:1: expected 'T"));
    CHECK_THAT(message("Terminals 2\nT 1\nDD 1 0 0\n"),
               ContainsSubstring("declared 2 terminals but found 1"));
    CHECK_THAT(message("Terminals -3\n"), ContainsSubstring("invalid terminal count"));
    CHECK_THAT(message("T 1\nDD 1 0 0" + std::string(70000, ' ') + "\n"),
               ContainsSubstring("line too long"));
}

TEST_CASE("readStpFile reports a missing file", "[io]") {
    CHECK_THROWS_AS(flute::readStpFile("/definitely/not/here.stp"), flute::ParseError);
}

TEST_CASE("writeSolution produces the documented format", "[io]") {
    flute::Solution solution;
    solution.edges = {{{0, 0}, {3, 0}}, {{3, 0}, {3, 4.5}}};
    solution.breakingPoints = {{3, 0}};
    std::ostringstream out;
    out.precision(3);  // must not leak out of the function
    flute::writeSolution(out, {{0, 0}, {3, 4.5}}, solution);

    CHECK(out.precision() == 3);
    CHECK(out.str() ==
          "# FLUTE result\npins 2\nedges 2\nlength 7.5\n"
          "P 0 0\nP 3 4.5\nB 3 0\nE 0 0 3 0\nE 3 0 3 4.5\n");
}
