// End-to-end regression check: solves every instance listed in data/golden_lengths.csv with a
// full lookup table and compares the tree length with the recorded value.
//
// Usage: golden_check <lookup table> <data directory>

#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "flute/geometry.hpp"
#include "flute/io.hpp"
#include "flute/lookup_table.hpp"
#include "flute/solver.hpp"
#include "flute/verify.hpp"
#include "reference.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: golden_check <lookup table> <data directory>\n";
        return 2;
    }
    try {
        const std::string data_directory = argv[2];
        const flute::LookupTable table = flute::LookupTable::load(argv[1]);
        const flute::RsmtSolver solver(table);

        std::ifstream golden(data_directory + "/golden_lengths.csv");
        if (!golden) {
            std::cerr << "cannot open golden_lengths.csv in " << data_directory << '\n';
            return 2;
        }

        int checked = 0;
        int failed = 0;
        std::string line;
        while (std::getline(golden, line)) {
            if (line.empty() || line.front() == '#') {
                continue;
            }
            const std::size_t comma = line.find(',');
            const std::string instance = line.substr(0, comma);
            const double expected = std::stod(line.substr(comma + 1));

            const std::vector<flute::Point> pins =
                flute::readStpFile(data_directory + "/" + instance);
            const flute::Solution solution = solver.solve(pins);
            const double actual = solution.length();
            const flute::VerificationResult verification =
                flute::verifyConnectedNetwork(pins, solution.edges);

            ++checked;
            const bool length_ok =
                std::abs(actual - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
            if (!length_ok || !verification.valid) {
                ++failed;
                std::cerr << "FAIL " << instance << ": length " << actual << ", expected "
                          << expected
                          << (verification.valid ? "" : ", invalid tree: " + verification.message)
                          << '\n';
            }
        }
        std::cout << (checked - failed) << " of " << checked << " golden lengths match\n";

        // Eight-pin nets are covered by the table and have to be solved exactly.
        std::mt19937 rng(8888);
        std::uniform_int_distribution<int> coordinate(0, 100000);
        int exact_checked = 0;
        for (int round = 0; round < 400; ++round) {
            std::vector<flute::Point> pins;
            for (int i = 0; i < 8; ++i) {
                pins.push_back({double(coordinate(rng)), double(coordinate(rng))});
            }
            const flute::Solution solution = solver.solve(pins);
            const bool valid = flute::verifyConnectedNetwork(pins, solution.edges).valid;
            const double optimum = flute::testing::exactRsmtLength(pins);
            ++exact_checked;
            if (!valid || std::abs(solution.length() - optimum) > 1e-6) {
                ++failed;
                std::cerr << "FAIL random 8-pin net " << round << ": length " << solution.length()
                          << ", optimum " << optimum << (valid ? "" : " (invalid tree)") << '\n';
            }
        }
        std::cout << exact_checked << " random 8-pin nets checked against the exact algorithm\n";
        return failed == 0 && checked > 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
