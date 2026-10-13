#pragma once

/// @file tree.hpp
/// Compact encoding of a rectilinear tree on a small grid.

#include <bitset>
#include <cstddef>
#include <string>
#include <string_view>

namespace flute {

/// A rectilinear tree drawn on a grid of at most 8x8 grid points.
///
/// A tree is stored as a bit set with one bit per grid edge. The bits are
/// organised in rows of 15 bits:
///
///   bits 0..6   of row y: horizontal edges (x,y)-(x+1,y) for x = 0..6
///   bits 7..14  of row y: vertical   edges (x,y)-(x,y+1) for x = 0..7
///
/// so the horizontal edge (x,y) has index 15*y + x and the vertical edge
/// (x,y) has index 15*y + 7 + x. Row 7 has no vertical edges, which gives
/// 7*15 + 7 = 112 bits in total.
///
/// The lookup table stores such trees as "potentially optimal Steiner trees"
/// (POSTs); the remaining members implement the grid transformations that
/// are needed to build and to reuse them.
class Tree {
public:
    static constexpr int kSide = 8;            ///< grid points per axis
    static constexpr int kRowStride = 15;      ///< bits per grid row
    static constexpr int kVerticalOffset = 7;  ///< first vertical bit in a row
    static constexpr std::size_t kEdgeBits = (7 * kRowStride) + kVerticalOffset;  ///< = 112

    Tree() = default;

    /// Parses a bit string of exactly kEdgeBits characters ('0'/'1'), most
    /// significant bit first (the format of std::bitset::to_string).
    /// @throws std::invalid_argument if the string is malformed
    static Tree fromBitString(std::string_view bits);

    /// Inverse of fromBitString().
    std::string toBitString() const;

    /// Bit index of an edge. Throws std::out_of_range for coordinates outside the grid.
    static std::size_t horizontalIndex(int column, int row);
    static std::size_t verticalIndex(int column, int row);

    bool hasHorizontalEdge(int column, int row) const;
    bool hasVerticalEdge(int column, int row) const;
    void addHorizontalEdge(int column, int row);
    void addVerticalEdge(int column, int row);
    void removeHorizontalEdge(int column, int row);
    void removeVerticalEdge(int column, int row);

    /// Number of edges in the tree.
    std::size_t edgeCount() const { return edges_.count(); }
    bool empty() const { return edges_.none(); }

    /// Largest column / row that is touched by an edge (0 for an empty tree).
    int maxColumn() const;
    int maxRow() const;

    /// Moves every edge at or to the right of @p column one column to the right and
    /// stretches the edges that cross the line at @p column. This re-inserts a grid column
    /// that was removed earlier.
    void insertColumnAt(int column);

    /// Same as insertColumnAt() for rows (moving upwards).
    void insertRowAt(int row);

    /// Shifts the whole tree by one column to the right / one row up.
    void shiftRight() { edges_ <<= 1; }
    void shiftUp() { edges_ <<= kRowStride; }

    /// Mirrors the tree at the vertical axis of its bounding square
    /// [0, max(maxColumn, maxRow)]^2.
    void mirrorHorizontally();

    /// Re-routes the bottom row of the tree for the lookup-table symmetry "flip the two
    /// lowest pins", where the two pins sit in columns @p columnA and @p columnB.
    void swapBottomPins(int columnA, int columnB);

    friend bool operator==(const Tree& lhs, const Tree& rhs) { return lhs.edges_ == rhs.edges_; }
    friend bool operator!=(const Tree& lhs, const Tree& rhs) { return !(lhs == rhs); }

private:
    std::bitset<kEdgeBits> edges_;
};

}  // namespace flute
