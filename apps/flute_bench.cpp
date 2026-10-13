// Benchmark tool: solves many instances with one loaded lookup table and reports how far the
// trees are from known optima.
//
//   flute_bench --lookup table.txt [--optima optima.csv] [--accuracy A] instance...

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "flute/io.hpp"
#include "flute/lookup_table.hpp"
#include "flute/solver.hpp"
#include "flute/verify.hpp"

namespace {

std::map<std::string, double> readOptima(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open " + path.string());
    }
    std::map<std::string, double> optima;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t comma = line.find(',');
        if (comma == std::string::npos) {
            throw std::runtime_error("malformed line in " + path.string() + ": " + line);
        }
        optima[line.substr(0, comma)] = std::stod(line.substr(comma + 1));
    }
    return optima;
}

struct Totals {
    int count = 0;
    double error_sum = 0;
    double error_max = 0;
    double milliseconds = 0;
};

}  // namespace

int main(int argc, char* argv[]) {
    try {
        std::filesystem::path lookup = "flute_lookup.txt";
        std::filesystem::path optima_path;
        flute::SolverOptions options;
        bool verify = false;
        bool refine = false;
        std::vector<std::filesystem::path> instances;
        const std::vector<std::string> args(argv + 1, argv + argc);
        for (std::size_t i = 0; i < args.size(); ++i) {
            const std::string& arg = args[i];
            const auto value = [&]() -> std::string {
                if (i + 1 >= args.size()) {
                    throw std::runtime_error("option " + arg + " needs a value");
                }
                return args[++i];
            };
            if (arg == "--lookup") {
                lookup = value();
            } else if (arg == "--optima") {
                optima_path = value();
            } else if (arg == "--accuracy") {
                options.accuracy = std::stoi(value());
            } else if (arg == "--verify") {
                verify = true;
            } else if (arg == "--refine") {
                refine = true;
            } else {
                instances.emplace_back(arg);
            }
        }
        if (instances.empty()) {
            std::cerr << "usage: flute_bench --lookup table [--optima csv] [--accuracy A] "
                         "[--verify] [--refine] "
                         "instance...\n";
            return 2;
        }

        const flute::LookupTable table = flute::LookupTable::load(lookup);
        const flute::RsmtSolver solver(table, options);
        const std::map<std::string, double> optima =
            optima_path.empty() ? std::map<std::string, double>{} : readOptima(optima_path);

        std::map<std::size_t, Totals> by_size;  // number of pins -> totals
        std::cout << std::fixed << std::setprecision(3);
        for (const std::filesystem::path& instance : instances) {
            const std::vector<flute::Point> pins = flute::readStpFile(instance);
            const auto begin = std::chrono::steady_clock::now();
            flute::Solution solution = solver.solve(pins);
            if (refine) {
                solver.refine(flute::HananGrid(pins), solution);
            }
            const double milliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin)
                    .count();
            if (verify) {
                const auto result = flute::verifyConnectedNetwork(pins, solution.edges);
                if (!result.valid) {
                    throw std::runtime_error(instance.string() +
                                             ": invalid tree: " + result.message);
                }
            }

            const auto known = optima.find(instance.stem().string());
            Totals& totals = by_size[pins.size()];
            ++totals.count;
            totals.milliseconds += milliseconds;
            std::cout << std::left << std::setw(18) << instance.stem().string() << std::right
                      << std::setw(7) << pins.size() << std::setw(16) << std::setprecision(1)
                      << solution.length() << std::setprecision(3);
            if (known != optima.end()) {
                const double error = 100.0 * (solution.length() - known->second) / known->second;
                totals.error_sum += error;
                totals.error_max = std::max(totals.error_max, error);
                std::cout << std::setw(10) << error << " %";
            }
            std::cout << std::setw(10) << milliseconds << " ms\n";
        }

        if (!optima.empty()) {
            std::cout << "\n  pins  instances  mean error  max error  mean time\n";
            for (const auto& [pins, totals] : by_size) {
                std::cout << std::setw(6) << pins << std::setw(11) << totals.count << std::setw(10)
                          << totals.error_sum / totals.count << " %" << std::setw(8)
                          << totals.error_max << " %" << std::setw(9)
                          << totals.milliseconds / totals.count << " ms\n";
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "flute_bench: " << e.what() << '\n';
        return 1;
    }
}
