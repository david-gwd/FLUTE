#include "flute/lookup_table.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "flute/error.hpp"

namespace flute {

std::size_t factorial(int n) {
    if (n < 0 || n > 12) {
        throw std::invalid_argument("factorial: argument out of range");
    }
    std::size_t result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= static_cast<std::size_t>(i);
    }
    return result;
}

GroupKey classify(const HananGrid& grid) {
    const int n = grid.numPins();
    if (n < 1 || n > kMaxLookupPins) {
        throw std::invalid_argument("classify: unsupported pin count");
    }
    if (!grid.inGeneralPosition()) {
        throw std::invalid_argument("classify: grid has pins on a common grid line");
    }
    const auto& sequence = grid.positionSequence();

    // Mixed-radix (Lehmer) code of the permutation: digit i counts the earlier entries that
    // are smaller than entry i.
    const std::size_t total = factorial(n);
    std::size_t group = 0;
    for (int i = n; i > 0; --i) {
        int smaller = 0;
        for (int j = 0; j < i - 1; ++j) {
            if (sequence[static_cast<std::size_t>(j)] < sequence[static_cast<std::size_t>(i - 1)]) {
                ++smaller;
            }
        }
        group += (total / factorial(i)) * static_cast<std::size_t>(smaller);
    }

    if (n < 4 || group < total / 4) {
        return {group, Symmetry::None};
    }
    if (group < total / 2) {
        return {(total / 2) - 1 - group, Symmetry::MirrorAndSwapBottomPins};
    }
    if (group < (3 * total) / 4) {
        return {group - (total / 2), Symmetry::SwapBottomPins};
    }
    return {total - 1 - group, Symmetry::MirrorHorizontally};
}

std::vector<int> positionSequenceOfGroup(int numPins, std::size_t index) {
    if (numPins < 1 || numPins > 12 || index >= factorial(numPins)) {
        throw std::invalid_argument("positionSequenceOfGroup: argument out of range");
    }
    const auto n = static_cast<std::size_t>(numPins);
    std::vector<std::size_t> digits(n, 0);
    std::size_t rest = index;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t radix = n - i;
        digits[n - i - 1] = rest % radix;
        rest /= radix;
    }
    std::vector<int> remaining(n);
    for (std::size_t i = 0; i < n; ++i) {
        remaining[i] = static_cast<int>(i);
    }
    std::vector<int> sequence(n, 0);
    for (std::size_t i = n; i-- > 0;) {
        const auto pick = remaining.begin() + static_cast<std::ptrdiff_t>(digits[i]);
        sequence[i] = *pick;
        remaining.erase(pick);
    }
    return sequence;
}

// ---- LookupTable ----------------------------------------------------------------------------

LookupTable::LookupTable(int maxPins) : maxPins_(maxPins) {
    if (maxPins < 2 || maxPins > kMaxLookupPins) {
        throw std::invalid_argument("LookupTable: maxPins must be in [2, " +
                                    std::to_string(kMaxLookupPins) + "]");
    }
    groups_.resize(static_cast<std::size_t>(maxPins) + 1);
    for (int n = 0; n <= maxPins; ++n) {
        groups_[static_cast<std::size_t>(n)].resize(groupCount(n));
    }
}

std::size_t LookupTable::groupCount(int numPins) {
    if (numPins <= 1) {
        return 1;
    }
    if (numPins <= 3) {
        return factorial(numPins);
    }
    return factorial(numPins) / 4;
}

const std::vector<LookupTable::Group>& LookupTable::groupsOf(int numPins) const {
    if (numPins < 0 || numPins > maxPins_) {
        throw std::out_of_range("LookupTable: no groups for " + std::to_string(numPins) + " pins");
    }
    return groups_[static_cast<std::size_t>(numPins)];
}

const LookupTable::Group& LookupTable::group(int numPins, std::size_t index) const {
    return groupsOf(numPins).at(index);
}

void LookupTable::setGroup(int numPins, std::size_t index, Group group) {
    if (numPins < 2 || numPins > maxPins_) {
        throw std::out_of_range("LookupTable: no groups for " + std::to_string(numPins) + " pins");
    }
    groups_[static_cast<std::size_t>(numPins)].at(index) = std::move(group);
}

std::size_t LookupTable::treeCount() const {
    std::size_t count = 0;
    for (const auto& groups : groups_) {
        for (const Group& group : groups) {
            count += group.size();
        }
    }
    return count;
}

Tree LookupTable::findOptimalTree(const HananGrid& grid) const {
    const int n = grid.numPins();
    if (n < 2 || n > maxPins_) {
        throw std::invalid_argument("findOptimalTree: the table covers 2.." +
                                    std::to_string(maxPins_) + " pins");
    }
    const GroupKey key = classify(grid);
    const Group& group = this->group(n, key.index);
    if (group.trees.empty()) {
        throw LookupTableError("lookup table has no trees for " + std::to_string(n) +
                               " pins, group " + std::to_string(key.index));
    }

    // For mirrored groups the stored trees and vectors belong to the mirror image of the grid.
    const bool mirrored = key.symmetry == Symmetry::MirrorHorizontally ||
                          key.symmetry == Symmetry::MirrorAndSwapBottomPins;

    std::size_t best = 0;
    Length best_length = std::numeric_limits<Length>::infinity();
    for (std::size_t i = 0; i < group.size(); ++i) {
        const Length candidate = grid.wirelength(group.vectorOf(i), mirrored);
        if (candidate < best_length) {
            best_length = candidate;
            best = i;
        }
    }

    Tree tree = group.trees[best];
    const auto& sequence = grid.positionSequence();
    switch (key.symmetry) {
        case Symmetry::None:
            break;
        case Symmetry::MirrorHorizontally:
            tree.mirrorHorizontally();
            break;
        case Symmetry::SwapBottomPins:
            tree.swapBottomPins(sequence[0], sequence[1]);
            break;
        case Symmetry::MirrorAndSwapBottomPins:
            // swapBottomPins() works on the stored (canonical) orientation, in which the two
            // lowest pins sit in the mirrored columns.
            tree.swapBottomPins(n - 1 - sequence[0], n - 1 - sequence[1]);
            tree.mirrorHorizontally();
            break;
    }
    return tree;
}

// ---- text format ----------------------------------------------------------------------------

namespace {

/// Splits text into lines without copying.
class LineReader {
public:
    explicit LineReader(std::string_view text) : text_(text) {}

    bool next(std::string_view& line) {
        if (position_ >= text_.size()) {
            return false;
        }
        const std::size_t end = text_.find('\n', position_);
        const std::size_t stop = end == std::string_view::npos ? text_.size() : end;
        line = text_.substr(position_, stop - position_);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        position_ = end == std::string_view::npos ? text_.size() : end + 1;
        ++lineNumber_;
        return true;
    }

    std::size_t lineNumber() const { return lineNumber_; }

private:
    std::string_view text_;
    std::size_t position_ = 0;
    std::size_t lineNumber_ = 0;
};

[[noreturn]] void fail(std::string_view source, std::size_t line, const std::string& message) {
    throw LookupTableError(std::string(source) + ":" + std::to_string(line) + ": " + message);
}

bool parseUnsigned(std::string_view text, std::size_t& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc() && end == text.data() + text.size();
}

}  // namespace

LookupTable LookupTable::parse(std::string_view text, std::string_view source) {
    // Pass 1 is implicit: groups are collected into a temporary map-like structure sized for
    // the largest pin count the format allows, then trimmed to the real maximum.
    LookupTable table(kMaxLookupPins);
    LineReader reader(text);
    std::string_view line;
    int max_pins = 0;

    while (reader.next(line)) {
        if (line.empty()) {
            continue;  // blank separator between groups
        }

        const std::size_t tilde = line.find('~');
        std::size_t pins = 0;
        std::size_t index = 0;
        if (tilde == std::string_view::npos || !parseUnsigned(line.substr(0, tilde), pins) ||
            !parseUnsigned(line.substr(tilde + 1), index)) {
            fail(source, reader.lineNumber(), "expected a group header 'pins~group'");
        }
        if (pins < 2 || pins > static_cast<std::size_t>(kMaxLookupPins)) {
            fail(source, reader.lineNumber(), "pin count out of range in group header");
        }
        const int n = static_cast<int>(pins);
        if (index >= groupCount(n)) {
            fail(source, reader.lineNumber(), "group index out of range");
        }
        Group& group = table.groups_[pins][index];
        if (group.size() != 0) {
            fail(source, reader.lineNumber(), "duplicate group");
        }
        group.width = vectorWidth(n);

        bool terminated = false;
        while (reader.next(line)) {
            if (line == "EOG") {
                terminated = true;
                break;
            }
            // wirelength vector: 'width' small integers, each followed by a space
            const std::size_t entry_start = group.powv.size();
            const char* cursor = line.data();
            const char* const stop = line.data() + line.size();
            while (cursor < stop) {
                unsigned value = 0;
                const auto [end, error] = std::from_chars(cursor, stop, value);
                if (error != std::errc() || value > std::numeric_limits<PowvEntry>::max()) {
                    fail(source, reader.lineNumber(), "invalid wirelength vector");
                }
                group.powv.push_back(static_cast<PowvEntry>(value));
                cursor = end;
                while (cursor < stop && *cursor == ' ') {
                    ++cursor;
                }
            }
            if (group.powv.size() - entry_start != group.width) {
                fail(source, reader.lineNumber(), "wirelength vector has the wrong length");
            }
            // tree: kEdgeBits characters
            const std::size_t vector_line = reader.lineNumber();
            if (!reader.next(line)) {
                fail(source, vector_line, "missing tree after wirelength vector");
            }
            try {
                group.trees.push_back(Tree::fromBitString(line));
            } catch (const std::invalid_argument& e) {
                fail(source, reader.lineNumber(), e.what());
            }
        }
        if (!terminated) {
            fail(source, reader.lineNumber(), "group is not terminated by EOG");
        }
        if (group.size() == 0) {
            fail(source, reader.lineNumber(), "group without trees");
        }
        max_pins = std::max(max_pins, n);
    }

    if (max_pins < 2) {
        throw LookupTableError(std::string(source) + ": the table is empty");
    }
    for (int n = 2; n <= max_pins; ++n) {
        for (std::size_t k = 0; k < groupCount(n); ++k) {
            if (table.groups_[static_cast<std::size_t>(n)][k].size() == 0) {
                throw LookupTableError(std::string(source) + ": group " + std::to_string(n) + "~" +
                                       std::to_string(k) + " is missing");
            }
        }
    }
    table.groups_.resize(static_cast<std::size_t>(max_pins) + 1);
    table.maxPins_ = max_pins;
    return table;
}

LookupTable LookupTable::load(const std::filesystem::path& path) {
    const std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw LookupTableError("cannot open lookup table '" + path.string() + "'");
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        throw LookupTableError("cannot read lookup table '" + path.string() + "'");
    }
    return parse(buffer.str(), path.string());
}

void LookupTable::write(std::ostream& out) const {
    for (int n = 2; n <= maxPins_; ++n) {
        const auto& groups = groups_[static_cast<std::size_t>(n)];
        for (std::size_t k = 0; k < groups.size(); ++k) {
            out << n << '~' << k << '\n';
            const Group& group = groups[k];
            for (std::size_t i = 0; i < group.size(); ++i) {
                const PowvEntry* vector = group.vectorOf(i);
                for (std::size_t j = 0; j < group.width; ++j) {
                    out << static_cast<int>(vector[j]) << ' ';
                }
                out << '\n' << group.trees[i].toBitString() << '\n';
            }
            out << "EOG\n\n";
        }
    }
}

void LookupTable::save(const std::filesystem::path& path) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw LookupTableError("cannot create lookup table '" + path.string() + "'");
    }
    write(file);
    file.flush();
    if (!file) {
        throw LookupTableError("cannot write lookup table '" + path.string() + "'");
    }
}

}  // namespace flute
