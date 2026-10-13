#pragma once

// Slow but obviously correct reference implementations used to check the library.

#include <vector>

#include "flute/types.hpp"

namespace flute::testing {

/// Length of a minimum spanning tree of @p points under the Manhattan metric.
double manhattanMstLength(const std::vector<Point>& points);

/// Exact length of a rectilinear Steiner minimal tree, found by enumerating all subsets of at
/// most n-2 Hanan grid points as Steiner points (Hwang's theorem). Only usable for few pins.
double bruteForceRsmtLength(const std::vector<Point>& pins);

/// Exact length of a rectilinear Steiner minimal tree by the Dreyfus-Wagner dynamic program on
/// the Hanan grid. Exponential in the number of distinct pins; fine up to about 10 pins.
double exactRsmtLength(const std::vector<Point>& pins);

/// Half-perimeter of the bounding box, a lower bound for the Steiner tree length.
double halfPerimeter(const std::vector<Point>& pins);

}  // namespace flute::testing
