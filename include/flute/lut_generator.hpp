#pragma once

/// @file lut_generator.hpp
/// Construction of the lookup table.

#include <cstddef>
#include <functional>
#include <vector>

#include "flute/hanan_grid.hpp"
#include "flute/lookup_table.hpp"
#include "flute/tree.hpp"

namespace flute {

/// Computes the potentially optimal Steiner trees (POSTs) of a Hanan grid with at most
/// kMaxLookupPins pins by the recursive boundary compaction / expansion scheme of the FLUTE
/// paper. The trees are drawn on the grid's column/row indices.
/// @throws std::invalid_argument if the grid has more than kMaxLookupPins pins
std::vector<Tree> generateTrees(const HananGrid& grid);

struct GeneratorOptions {
    /// Largest net degree stored in the table, 2..kMaxLookupPins.
    int maxPins = kMaxLookupPins;

    /// Number of worker threads; 0 selects std::thread::hardware_concurrency().
    unsigned threads = 0;

    /// Optional progress callback, invoked with (pins, groupsDone, groupsTotal) whenever a
    /// group is finished. Calls are serialised, so the callback needs no locking of its own.
    std::function<void(int pins, std::size_t done, std::size_t total)> progress;
};

/// Builds the lookup table. The result does not depend on the number of threads.
LookupTable generateLookupTable(const GeneratorOptions& options = {});

}  // namespace flute
