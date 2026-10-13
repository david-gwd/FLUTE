#include "flute/tree.hpp"

#include <algorithm>
#include <stdexcept>

namespace flute {

namespace {

constexpr int kLastColumn = Tree::kSide - 1;  // highest grid point index
constexpr int kLastRow = Tree::kSide - 1;

}  // namespace

Tree Tree::fromBitString(std::string_view bits) {
    if (bits.size() != kEdgeBits) {
        throw std::invalid_argument("tree bit string must have " + std::to_string(kEdgeBits) +
                                    " characters");
    }
    Tree tree;
    for (std::size_t i = 0; i < kEdgeBits; ++i) {
        const char c = bits[i];
        if (c != '0' && c != '1') {
            throw std::invalid_argument("tree bit string may only contain '0' and '1'");
        }
        tree.edges_[kEdgeBits - 1 - i] = (c == '1');
    }
    return tree;
}

std::string Tree::toBitString() const {
    return edges_.to_string();
}

std::size_t Tree::horizontalIndex(int column, int row) {
    if (column < 0 || column >= kLastColumn || row < 0 || row > kLastRow) {
        throw std::out_of_range("horizontal edge outside the grid");
    }
    return (static_cast<std::size_t>(row) * kRowStride) + static_cast<std::size_t>(column);
}

std::size_t Tree::verticalIndex(int column, int row) {
    if (column < 0 || column > kLastColumn || row < 0 || row >= kLastRow) {
        throw std::out_of_range("vertical edge outside the grid");
    }
    return (static_cast<std::size_t>(row) * kRowStride) + kVerticalOffset +
           static_cast<std::size_t>(column);
}

bool Tree::hasHorizontalEdge(int column, int row) const {
    return edges_.test(horizontalIndex(column, row));
}
bool Tree::hasVerticalEdge(int column, int row) const {
    return edges_.test(verticalIndex(column, row));
}
void Tree::addHorizontalEdge(int column, int row) {
    edges_.set(horizontalIndex(column, row));
}
void Tree::addVerticalEdge(int column, int row) {
    edges_.set(verticalIndex(column, row));
}
void Tree::removeHorizontalEdge(int column, int row) {
    edges_.reset(horizontalIndex(column, row));
}
void Tree::removeVerticalEdge(int column, int row) {
    edges_.reset(verticalIndex(column, row));
}

int Tree::maxColumn() const {
    for (int column = kLastColumn; column > 0; --column) {
        for (int row = 0; row <= kLastRow; ++row) {
            if (hasHorizontalEdge(column - 1, row)) {
                return column;
            }
        }
        for (int row = 0; row < kLastRow; ++row) {
            if (hasVerticalEdge(column, row)) {
                return column;
            }
        }
    }
    return 0;
}

int Tree::maxRow() const {
    for (int row = kLastRow; row > 0; --row) {
        for (int column = 0; column <= kLastColumn; ++column) {
            if (hasVerticalEdge(column, row - 1)) {
                return row;
            }
        }
        for (int column = 0; column < kLastColumn; ++column) {
            if (hasHorizontalEdge(column, row)) {
                return row;
            }
        }
    }
    return 0;
}

void Tree::insertColumnAt(int column) {
    if (column < 0 || column > kLastColumn) {
        throw std::out_of_range("column index outside the grid");
    }
    const int last_column = maxColumn();
    const int last_row = maxRow();

    // Horizontal edges: the edge entering the new column is stretched, all edges behind
    // it move one step to the right. Iterating from the right never moves an edge twice.
    for (int w = last_column; w >= column && w > 0; --w) {
        for (int row = 0; row <= last_row; ++row) {
            if (!hasHorizontalEdge(w - 1, row)) {
                continue;
            }
            if (w > column) {
                removeHorizontalEdge(w - 1, row);
            }
            if (w < kLastColumn) {
                addHorizontalEdge(w, row);
            }
        }
    }
    // Vertical edges at or behind the new column move one step to the right.
    for (int w = last_column; w >= column; --w) {
        for (int row = 0; row < last_row; ++row) {
            if (!hasVerticalEdge(w, row)) {
                continue;
            }
            removeVerticalEdge(w, row);
            if (w < kLastColumn) {
                addVerticalEdge(w + 1, row);
            }
        }
    }
}

void Tree::insertRowAt(int row) {
    if (row < 0 || row > kLastRow) {
        throw std::out_of_range("row index outside the grid");
    }
    const int last_column = maxColumn();
    const int last_row = maxRow();

    // Horizontal edges at or above the new row move one row up.
    for (int h = last_row; h >= row; --h) {
        for (int column = 0; column < last_column; ++column) {
            if (!hasHorizontalEdge(column, h)) {
                continue;
            }
            removeHorizontalEdge(column, h);
            if (h < kLastRow) {
                addHorizontalEdge(column, h + 1);
            }
        }
    }
    // Vertical edges: the edge entering the new row is stretched, all edges above move up.
    for (int h = last_row; h >= row && h > 0; --h) {
        for (int column = 0; column <= last_column; ++column) {
            if (!hasVerticalEdge(column, h - 1)) {
                continue;
            }
            if (h > row) {
                removeVerticalEdge(column, h - 1);
            }
            if (h < kLastRow) {
                addVerticalEdge(column, h);
            }
        }
    }
}

void Tree::mirrorHorizontally() {
    const int side = std::max(maxColumn(), maxRow());
    std::bitset<kEdgeBits> mirrored;
    for (int row = 0; row <= side; ++row) {
        for (int column = 0; column < side; ++column) {
            if (!hasHorizontalEdge(column, row)) {
                continue;
            }
            edges_.reset(horizontalIndex(column, row));
            mirrored.set(horizontalIndex(side - column - 1, row));
        }
    }
    for (int row = 0; row < side; ++row) {
        for (int column = 0; column <= side; ++column) {
            if (!hasVerticalEdge(column, row)) {
                continue;
            }
            edges_.reset(verticalIndex(column, row));
            mirrored.set(verticalIndex(side - column, row));
        }
    }
    edges_ |= mirrored;
}

void Tree::swapBottomPins(int columnA, int columnB) {
    const int left = std::min(columnA, columnB);
    const int right = std::max(columnA, columnB);

    bool has_bottom_horizontal = false;
    for (int column = 0; column < kLastColumn; ++column) {
        if (hasHorizontalEdge(column, 0)) {
            has_bottom_horizontal = true;
        }
    }

    std::bitset<kEdgeBits> moved;
    if (has_bottom_horizontal) {
        // Extent [first, last] of the bottom row, in columns.
        int first = 0;
        int last = 0;
        for (int column = 0; column < kLastColumn; ++column) {
            if (hasHorizontalEdge(column, 0) || hasVerticalEdge(column, 0)) {
                first = column;
                break;
            }
        }
        for (int column = kLastColumn - 1; column >= 0; --column) {
            if (hasHorizontalEdge(column, 0) || hasVerticalEdge(column + 1, 0)) {
                last = column + 1;
                break;
            }
        }
        if (first != last) {
            for (int offset = 0; offset <= last - first; ++offset) {
                const int column = first + offset;
                // Horizontal bottom edges are lifted into row 1 ...
                if (offset < last - first && hasHorizontalEdge(column, 0)) {
                    removeHorizontalEdge(column, 0);
                    moved.set(horizontalIndex(column, 1));
                }
                // ... and the vertical edges leaving the bottom row are mirrored.
                if (hasVerticalEdge(column, 0)) {
                    removeVerticalEdge(column, 0);
                    moved.set(verticalIndex(last - offset, 0));
                }
            }
        }
    }
    edges_ |= moved;

    // Toggle the legs of the two bottom pins.
    std::bitset<kEdgeBits> legs;
    legs.set(verticalIndex(left, 0));
    legs.set(verticalIndex(right, 0));
    edges_ ^= legs;
}

}  // namespace flute
