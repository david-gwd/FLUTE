#pragma once

/// @file solver.hpp
/// Computation of rectilinear Steiner minimal trees.

#include <vector>

#include "flute/geometry.hpp"
#include "flute/hanan_grid.hpp"
#include "flute/lookup_table.hpp"
#include "flute/types.hpp"

namespace flute {

/// A rectilinear Steiner tree for a net.
struct Solution {
    /// Tree edges as maximal axis-parallel segments, sorted.
    std::vector<Segment> edges;

    /// Pins at which a large net was split by net breaking (empty for nets that are covered by
    /// the lookup table). The tree is optimal if this is empty.
    std::vector<Point> breakingPoints;

    Length length() const { return totalLength(edges); }
};

/// Tuning parameters of the solver.
struct SolverOptions {
    /// Number of ways a net is broken that are tried and compared (the accuracy parameter A of
    /// the FLUTE paper). Each recursion level halves it, never going below 1. Larger values
    /// give shorter trees for nets above the table size at the cost of run time.
    int accuracy = 3;
};

/// Computes rectilinear Steiner trees with the FLUTE algorithm: nets with at most
/// table.maxPins() pins are solved optimally by lookup, larger nets are split at suitable pins
/// until the parts are small enough.
class RsmtSolver {
public:
    /// @param table lookup table; it must outlive the solver. Needs maxPins() >= 4.
    /// @throws std::invalid_argument if the table is too small or options.accuracy < 1
    explicit RsmtSolver(const LookupTable& table, SolverOptions options = {});

    /// @throws std::invalid_argument if @p pins is empty
    Solution solve(const std::vector<Point>& pins) const;
    Solution solve(const HananGrid& grid) const;

    /// Experimental post-optimisation: re-solves the neighbourhood of each breaking point of
    /// @p solution exactly. The result replaces the solution only if it is a valid connected
    /// network that is not longer than before. Returns true if the solution was improved.
    bool refine(const HananGrid& grid, Solution& solution) const;

private:
    std::vector<Segment> solveGrid(const HananGrid& grid, int accuracy,
                                   std::vector<Point>& breakingPoints) const;
    std::vector<Segment> breakNet(const HananGrid& grid, int accuracy,
                                  std::vector<Point>& breakingPoints) const;

    const LookupTable& table_;
    SolverOptions options_;
};

}  // namespace flute
