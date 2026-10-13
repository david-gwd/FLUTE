#pragma once

/// @file lookup_table.hpp
/// The FLUTE lookup table: potentially optimal Steiner trees for all small pin configurations.

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string_view>
#include <vector>

#include "flute/hanan_grid.hpp"
#include "flute/tree.hpp"

namespace flute {

/// n! for 0 <= n <= 12.
std::size_t factorial(int n);

/// How the optimal tree of a canonical group has to be transformed to obtain the tree of
/// another group. Groups related by reflecting the grid vertically or by swapping the two
/// lowest pins share their trees.
enum class Symmetry {
    None,
    MirrorHorizontally,
    SwapBottomPins,
    MirrorAndSwapBottomPins,
};

/// Position of a grid in the lookup table.
struct GroupKey {
    std::size_t index = 0;  ///< canonical group index, < LookupTable::groupCount(n)
    Symmetry symmetry = Symmetry::None;
};

/// Computes the table position of a grid in general position with at most
/// kMaxLookupPins pins.
GroupKey classify(const HananGrid& grid);

/// The position sequence of the group with the given (unfolded) index, 0 <= index < n!.
/// Inverse of the index computation inside classify().
std::vector<int> positionSequenceOfGroup(int numPins, std::size_t index);

/// Stores, for every group of n pins, the potentially optimal Steiner trees (POSTs) together
/// with their wirelength vectors (POWVs). The best tree of a concrete net is the POST whose
/// POWV gives the smallest length for the actual grid gaps.
class LookupTable {
public:
    /// All trees of one group; entry i has the wirelength vector
    /// powv[i*width .. (i+1)*width) and the tree trees[i].
    struct Group {
        std::size_t width = 0;
        std::vector<PowvEntry> powv;
        std::vector<Tree> trees;

        std::size_t size() const { return trees.size(); }
        const PowvEntry* vectorOf(std::size_t entry) const { return powv.data() + (entry * width); }
    };

    /// Creates a table without any trees for nets with up to @p maxPins pins.
    /// @throws std::invalid_argument unless 2 <= maxPins <= kMaxLookupPins
    explicit LookupTable(int maxPins);

    int maxPins() const { return maxPins_; }

    /// Number of groups for nets with @p numPins pins: n!/4 for n >= 4, n! below.
    static std::size_t groupCount(int numPins);

    /// Number of entries of a wirelength vector for @p numPins pins.
    static std::size_t vectorWidth(int numPins) {
        return (2 * static_cast<std::size_t>(numPins)) - 2;
    }

    const Group& group(int numPins, std::size_t index) const;
    void setGroup(int numPins, std::size_t index, Group group);

    /// Total number of stored trees.
    std::size_t treeCount() const;

    /// Returns the optimal rectilinear Steiner tree of @p grid, drawn on the grid.
    /// @pre grid.inGeneralPosition() and grid.numPins() <= maxPins()
    /// @throws std::invalid_argument if the precondition is violated
    /// @throws LookupTableError if the table does not contain the group
    Tree findOptimalTree(const HananGrid& grid) const;

    /// Parses the text format written by write(). @p source is only used in error messages.
    /// @throws LookupTableError if the data is malformed or incomplete
    static LookupTable parse(std::string_view text, std::string_view source = "<memory>");

    /// Reads a table from a file.
    /// @throws LookupTableError if the file cannot be read or is malformed
    static LookupTable load(const std::filesystem::path& path);

    /// Writes the table in text format (one "n~group" block per group).
    void write(std::ostream& out) const;

    /// Writes the table to a file.
    /// @throws LookupTableError on I/O errors
    void save(const std::filesystem::path& path) const;

private:
    const std::vector<Group>& groupsOf(int numPins) const;

    int maxPins_;
    std::vector<std::vector<Group>> groups_;  // groups_[n][index]
};

}  // namespace flute
