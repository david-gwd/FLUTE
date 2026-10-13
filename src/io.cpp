#include "flute/io.hpp"

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <set>
#include <sstream>
#include <unordered_map>

#include "flute/error.hpp"

namespace flute {

namespace {

constexpr std::size_t kMaxLineLength = 1 << 16;

bool parseInteger(const std::string& token, long long& value) {
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    return error == std::errc() && end == token.data() + token.size();
}

bool parseCoordinate(const std::string& token, Coord& value) {
    errno = 0;
    char* end = nullptr;
    value = std::strtod(token.c_str(), &end);
    return errno == 0 && end == token.c_str() + token.size() && std::isfinite(value);
}

}  // namespace

std::vector<Point> readStp(std::istream& in, const std::string& sourceName) {
    std::set<long long> terminals;
    std::vector<std::pair<long long, Point>> coordinates;  // in file order
    std::unordered_map<long long, std::size_t> seen_coordinates;
    long long declared_terminals = -1;

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(in, line)) {
        ++line_number;
        if (line.size() > kMaxLineLength) {
            throw ParseError(sourceName, line_number, "line too long");
        }

        std::istringstream tokens(line);
        std::string keyword;
        if (!(tokens >> keyword)) {
            continue;
        }

        if (keyword == "Terminals") {
            std::string count;
            if (!(tokens >> count) || !parseInteger(count, declared_terminals) ||
                declared_terminals < 0) {
                throw ParseError(sourceName, line_number, "invalid terminal count");
            }
        } else if (keyword == "T") {
            std::string id_token;
            long long id = 0;
            if (!(tokens >> id_token) || !parseInteger(id_token, id)) {
                throw ParseError(sourceName, line_number, "expected 'T <terminal id>'");
            }
            if (!terminals.insert(id).second) {
                throw ParseError(sourceName, line_number, "duplicate terminal " + id_token);
            }
            if (terminals.size() > kMaxTerminals) {
                throw ParseError(sourceName, line_number, "too many terminals");
            }
        } else if (keyword == "DD") {
            std::string id_token;
            std::string x_token;
            std::string y_token;
            long long id = 0;
            Point point;
            if (!(tokens >> id_token >> x_token >> y_token) || !parseInteger(id_token, id) ||
                !parseCoordinate(x_token, point.x) || !parseCoordinate(y_token, point.y)) {
                throw ParseError(sourceName, line_number, "expected 'DD <id> <x> <y>'");
            }
            if (!seen_coordinates.emplace(id, coordinates.size()).second) {
                throw ParseError(sourceName, line_number,
                                 "duplicate coordinates for node " + id_token);
            }
            coordinates.emplace_back(id, point);
        }
    }
    if (in.bad()) {
        throw ParseError(sourceName, "read error");
    }

    if (terminals.empty()) {
        throw ParseError(sourceName, "the instance has no terminals");
    }
    if (declared_terminals >= 0 &&
        static_cast<std::size_t>(declared_terminals) != terminals.size()) {
        throw ParseError(sourceName, "declared " + std::to_string(declared_terminals) +
                                         " terminals but found " +
                                         std::to_string(terminals.size()));
    }

    std::vector<Point> pins;
    pins.reserve(terminals.size());
    for (const auto& [id, point] : coordinates) {
        if (terminals.count(id) != 0) {
            pins.push_back(point);
        }
    }
    if (pins.size() != terminals.size()) {
        for (const long long id : terminals) {
            if (seen_coordinates.count(id) == 0) {
                throw ParseError(sourceName,
                                 "terminal " + std::to_string(id) + " has no coordinates");
            }
        }
    }
    return pins;
}

std::vector<Point> readStpFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw ParseError(path.string(), "cannot open file");
    }
    return readStp(file, path.string());
}

void writeSolution(std::ostream& out, const std::vector<Point>& pins, const Solution& solution) {
    const std::ios_base::fmtflags flags = out.flags();
    const std::streamsize precision = out.precision();
    out << std::setprecision(15);

    out << "# FLUTE result\n";
    out << "pins " << pins.size() << '\n';
    out << "edges " << solution.edges.size() << '\n';
    out << "length " << solution.length() << '\n';
    for (const Point& pin : pins) {
        out << "P " << pin.x << ' ' << pin.y << '\n';
    }
    for (const Point& point : solution.breakingPoints) {
        out << "B " << point.x << ' ' << point.y << '\n';
    }
    for (const Segment& edge : solution.edges) {
        out << "E " << edge.a.x << ' ' << edge.a.y << ' ' << edge.b.x << ' ' << edge.b.y << '\n';
    }

    out.flags(flags);
    out.precision(precision);
}

void writeSolutionFile(const std::filesystem::path& path, const std::vector<Point>& pins,
                       const Solution& solution) {
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        throw Error("cannot create '" + path.string() + "'");
    }
    writeSolution(file, pins, solution);
    file.flush();
    if (!file) {
        throw Error("cannot write '" + path.string() + "'");
    }
}

}  // namespace flute
