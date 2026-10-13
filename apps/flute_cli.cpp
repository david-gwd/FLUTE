// Command line interface of the FLUTE library.

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "flute/error.hpp"
#include "flute/io.hpp"
#include "flute/lookup_table.hpp"
#include "flute/lut_generator.hpp"
#include "flute/solver.hpp"
#include "flute/verify.hpp"

#ifndef FLUTE_VERSION
#define FLUTE_VERSION "unknown"
#endif

namespace {

constexpr int kExitOk = 0;
constexpr int kExitFailure = 1;
constexpr int kExitUsage = 2;

constexpr const char* kDefaultLookupFile = "flute_lookup.txt";
constexpr const char* kLookupEnvironment = "FLUTE_LOOKUP_TABLE";

class UsageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void printUsage(std::ostream& out) {
    out << "FLUTE " FLUTE_VERSION
           " - fast rectilinear Steiner minimal trees\n\n"
           "Usage:\n"
           "  flute generate [options]       build the lookup table\n"
           "  flute solve <instance> [options]  compute a Steiner tree for an STP instance\n"
           "  flute --help | --version\n\n"
           "Options for 'generate':\n"
           "  -o, --output <file>   lookup table to write (default: "
        << kDefaultLookupFile
        << ")\n"
           "  --max-pins <n>        largest net degree stored, 2..8 (default: 8)\n"
           "  -j, --threads <n>     worker threads (default: all cores)\n"
           "  -f, --force           overwrite an existing file\n"
           "  -q, --quiet           no progress output\n\n"
           "Options for 'solve':\n"
           "  -l, --lookup <file>   lookup table (default: $"
        << kLookupEnvironment << ", else " << kDefaultLookupFile
        << ")\n"
           "  -o, --output <file>   write the tree to a file (format: see tools/draw.py)\n"
           "  --accuracy <n>        ways of breaking large nets that are tried, default 3 (more = "
           "shorter\n"
           "                        trees, slower)\n"
           "  --refine              experimental post-optimisation around the breaking points\n"
           "  --verify              check that the result is a connected tree over all pins\n"
           "  -q, --quiet           print only the tree length\n";
}

/// Returns the value of an option that takes an argument.
std::string optionValue(const std::vector<std::string>& args, std::size_t& index) {
    if (index + 1 >= args.size()) {
        throw UsageError("option '" + args[index] + "' needs a value");
    }
    return args[++index];
}

int parsePositive(const std::string& text, const std::string& option) {
    int value = 0;
    const char* const end = text.data() + text.size();
    const auto [parsed_to, error] = std::from_chars(text.data(), end, value);
    if (error != std::errc() || parsed_to != end || value <= 0) {
        throw UsageError("option '" + option + "' needs a positive integer, got '" + text + "'");
    }
    return value;
}

int generateCommand(const std::vector<std::string>& args) {
    std::filesystem::path output = kDefaultLookupFile;
    flute::GeneratorOptions options;
    bool force = false;
    bool quiet = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "-o" || arg == "--output") {
            output = optionValue(args, i);
        } else if (arg == "--max-pins") {
            options.maxPins = parsePositive(optionValue(args, i), arg);
        } else if (arg == "-j" || arg == "--threads") {
            options.threads = static_cast<unsigned>(parsePositive(optionValue(args, i), arg));
        } else if (arg == "-f" || arg == "--force") {
            force = true;
        } else if (arg == "-q" || arg == "--quiet") {
            quiet = true;
        } else {
            throw UsageError("unknown option '" + arg + "' for 'generate'");
        }
    }
    if (options.maxPins < 2 || options.maxPins > flute::kMaxLookupPins) {
        throw UsageError("--max-pins must be between 2 and " +
                         std::to_string(flute::kMaxLookupPins));
    }
    if (std::filesystem::exists(output) && !force) {
        throw flute::Error("'" + output.string() + "' already exists (use --force to overwrite)");
    }

    if (!quiet) {
        options.progress = [](int pins, std::size_t done, std::size_t total) {
            if (done == total || done % 256 == 0) {
                std::cerr << "\rnets with " << pins << " pins: " << done << " / " << total
                          << " groups" << (done == total ? "\n" : "") << std::flush;
            }
        };
    }
    const auto begin = std::chrono::steady_clock::now();
    const flute::LookupTable table = flute::generateLookupTable(options);
    table.save(output);
    const auto seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    std::cout << "wrote " << table.treeCount() << " trees to " << output.string() << " in "
              << seconds << " s\n";
    return kExitOk;
}

std::filesystem::path defaultLookupPath() {
    if (const char* environment = std::getenv(kLookupEnvironment)) {
        if (*environment != '\0') {
            return environment;
        }
    }
    return kDefaultLookupFile;
}

int solveCommand(const std::vector<std::string>& args) {
    std::filesystem::path instance;
    std::filesystem::path lookup = defaultLookupPath();
    std::filesystem::path output;
    flute::SolverOptions options;
    bool refine = false;
    bool verify = false;
    bool quiet = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--accuracy") {
            options.accuracy = parsePositive(optionValue(args, i), arg);
        } else if (arg == "-l" || arg == "--lookup") {
            lookup = optionValue(args, i);
        } else if (arg == "-o" || arg == "--output") {
            output = optionValue(args, i);
        } else if (arg == "--refine") {
            refine = true;
        } else if (arg == "--verify") {
            verify = true;
        } else if (arg == "-q" || arg == "--quiet") {
            quiet = true;
        } else if (!arg.empty() && arg.front() == '-') {
            throw UsageError("unknown option '" + arg + "' for 'solve'");
        } else if (instance.empty()) {
            instance = arg;
        } else {
            throw UsageError("more than one instance file given");
        }
    }
    if (instance.empty()) {
        throw UsageError("'solve' needs an instance file");
    }

    const auto begin = std::chrono::steady_clock::now();
    const std::vector<flute::Point> pins = flute::readStpFile(instance);
    const flute::LookupTable table = flute::LookupTable::load(lookup);
    const auto loaded = std::chrono::steady_clock::now();

    const flute::RsmtSolver solver(table, options);
    const flute::HananGrid grid(pins);
    flute::Solution solution = solver.solve(grid);
    if (refine) {
        solver.refine(grid, solution);
    }
    const auto solved = std::chrono::steady_clock::now();

    if (verify) {
        const flute::VerificationResult result =
            flute::verifyConnectedNetwork(pins, solution.edges);
        if (!result.valid) {
            throw flute::Error("verification failed: " + result.message);
        }
    }
    if (!output.empty()) {
        flute::writeSolutionFile(output, pins, solution);
    }

    const auto milliseconds = [](auto from, auto to) {
        return std::chrono::duration<double, std::milli>(to - from).count();
    };
    if (quiet) {
        std::cout << std::setprecision(12) << solution.length() << '\n';
    } else {
        std::cout << "instance:  " << instance.string() << '\n'
                  << "pins:      " << pins.size() << '\n'
                  << "length:    " << solution.length() << '\n'
                  << "edges:     " << solution.edges.size() << '\n'
                  << "optimal:   "
                  << (solution.breakingPoints.empty()
                          ? "yes"
                          : "no (net broken at " + std::to_string(solution.breakingPoints.size()) +
                                " pins)")
                  << '\n'
                  << (verify ? "verified:  yes\n" : "")
                  << "time:      " << milliseconds(begin, loaded) << " ms loading, "
                  << milliseconds(loaded, solved) << " ms solving\n";
    }
    return kExitOk;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        if (args.empty()) {
            printUsage(std::cerr);
            return kExitUsage;
        }
        const std::string& command = args.front();
        if (command == "-h" || command == "--help" || command == "help") {
            printUsage(std::cout);
            return kExitOk;
        }
        if (command == "--version") {
            std::cout << "flute " FLUTE_VERSION "\n";
            return kExitOk;
        }
        const std::vector<std::string> rest(args.begin() + 1, args.end());
        if (command == "generate") {
            return generateCommand(rest);
        }
        if (command == "solve") {
            return solveCommand(rest);
        }
        throw UsageError("unknown command '" + command + "'");
    } catch (const UsageError& e) {
        std::cerr << "flute: " << e.what() << "\nTry 'flute --help'.\n";
        return kExitUsage;
    } catch (const std::exception& e) {
        std::cerr << "flute: " << e.what() << '\n';
        return kExitFailure;
    }
}
