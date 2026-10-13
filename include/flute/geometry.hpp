#pragma once

/// @file geometry.hpp
/// Conversion between grid trees and coordinate segments.

#include <vector>

#include "flute/hanan_grid.hpp"
#include "flute/tree.hpp"
#include "flute/types.hpp"

namespace flute {

/// Rectilinear length of one segment.
Length length(const Segment& segment);

/// Total length of a set of segments.
Length totalLength(const std::vector<Segment>& segments);

/// Merges collinear segments that touch or overlap into maximal segments (the result covers
/// exactly the same points) and returns them sorted. Zero-length segments are dropped.
/// @throws std::invalid_argument if a segment is neither horizontal nor vertical
std::vector<Segment> mergeCollinear(std::vector<Segment> segments);

/// Moves @p point to the grid lines of @p grid: each coordinate becomes the largest grid
/// line that is not greater than it. This undoes the small shifts of HananGrid::separated().
Point snapToGrid(const HananGrid& grid, Point point);

/// Applies snapToGrid() to every segment, drops segments that collapse to a point and merges
/// the result.
std::vector<Segment> snapToGrid(const HananGrid& grid, const std::vector<Segment>& segments);

/// Converts a tree that is drawn on @p grid into coordinate segments (one per grid edge, not
/// merged).
/// @throws std::invalid_argument if the tree uses grid lines that do not exist in @p grid
std::vector<Segment> treeToSegments(const HananGrid& grid, const Tree& tree);

/// Converts a tree that is drawn on @p separated (see HananGrid::separated()) into segments
/// on the coordinates of @p original. Edges that collapse to a point are dropped.
std::vector<Segment> treeToSegments(const HananGrid& original, const HananGrid& separated,
                                    const Tree& tree);

}  // namespace flute
