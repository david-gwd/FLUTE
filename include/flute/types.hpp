#pragma once

/// @file types.hpp
/// Basic vocabulary types shared by every part of the library.

#include <cstddef>

namespace flute {

/// Coordinate of a point along one axis.
using Coord = double;

/// Rectilinear (Manhattan) length.
using Length = double;

/// Identifier of a pin. The pins of a HananGrid are numbered 0..n-1.
using PinId = int;

/// Index of a grid line (a column or a row) of a HananGrid.
using GridIndex = int;

/// Largest number of pins for which the lookup table stores optimal trees.
/// It is bounded by the 8x8 grid encoded in Tree.
inline constexpr int kMaxLookupPins = 8;

/// A point in the plane.
struct Point {
    Coord x = 0;
    Coord y = 0;
};

inline bool operator==(const Point& lhs, const Point& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
inline bool operator!=(const Point& lhs, const Point& rhs) {
    return !(lhs == rhs);
}

/// An axis-parallel line segment between two points.
struct Segment {
    Point a;
    Point b;
};

/// Position of a pin inside a HananGrid, in grid-line indices.
struct GridPoint {
    GridIndex column = 0;  ///< index of the vertical line (x direction)
    GridIndex row = 0;     ///< index of the horizontal line (y direction)
};

/// Side of a grid, used for boundary compaction and expansion.
enum class Side { Right, Top, Left, Bottom };

}  // namespace flute
