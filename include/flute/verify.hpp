#pragma once

/// @file verify.hpp
/// Independent validity check for computed trees.

#include <string>
#include <vector>

#include "flute/types.hpp"

namespace flute {

struct VerificationResult {
    bool valid = false;
    std::string message;  ///< empty if valid, otherwise the first problem found
};

/// Checks that @p edges are axis-parallel segments that form one connected network containing
/// every pin of @p pins. Segments may touch or cross anywhere (Steiner points are not
/// required to be segment end points). Cycles are not reported.
VerificationResult verifyConnectedNetwork(const std::vector<Point>& pins,
                                          const std::vector<Segment>& edges);

}  // namespace flute
