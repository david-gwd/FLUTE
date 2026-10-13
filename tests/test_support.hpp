#pragma once

#include <map>
#include <random>
#include <vector>

#include "flute/lookup_table.hpp"
#include "flute/lut_generator.hpp"
#include "flute/types.hpp"

namespace flute::testing {

/// A lookup table for up to @p maxPins pins, generated once per process and cached.
inline const LookupTable& tableFor(int maxPins) {
    static std::map<int, LookupTable> cache;  // the tests run single-threaded
    auto found = cache.find(maxPins);
    if (found == cache.end()) {
        GeneratorOptions options;
        options.maxPins = maxPins;
        found = cache.emplace(maxPins, generateLookupTable(options)).first;
    }
    return found->second;
}

/// Random pins with integer coordinates in [0, range]; small ranges produce many pins that
/// share grid lines or coincide.
inline std::vector<Point> randomPins(std::mt19937& rng, int count, int range) {
    std::uniform_int_distribution<int> coordinate(0, range);
    std::vector<Point> pins;
    for (int i = 0; i < count; ++i) {
        pins.push_back(
            {static_cast<double>(coordinate(rng)), static_cast<double>(coordinate(rng))});
    }
    return pins;
}

}  // namespace flute::testing
