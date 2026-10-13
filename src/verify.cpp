#include "flute/verify.hpp"

#include <algorithm>
#include <numeric>

namespace flute {

namespace {

class DisjointSets {
public:
    explicit DisjointSets(std::size_t size) : parent_(size) {
        std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    }
    std::size_t find(std::size_t item) {
        while (parent_[item] != item) {
            parent_[item] = parent_[parent_[item]];
            item = parent_[item];
        }
        return item;
    }
    void unite(std::size_t a, std::size_t b) { parent_[find(a)] = find(b); }

private:
    std::vector<std::size_t> parent_;
};

/// An axis-parallel segment normalised to fixed < lo <= hi along its own axis.
struct Run {
    Coord fixed;  // y for horizontal runs, x for vertical runs
    Coord lo;
    Coord hi;
    std::size_t id;
};

bool lessByFixed(const Run& lhs, const Run& rhs) {
    return lhs.fixed != rhs.fixed ? lhs.fixed < rhs.fixed : lhs.lo < rhs.lo;
}

/// Unites runs on the same line that overlap or touch. @p runs must be sorted by (fixed, lo).
void uniteCollinear(const std::vector<Run>& runs, DisjointSets& sets) {
    std::size_t reach = 0;  // run on the current line that extends furthest to the right
    for (std::size_t i = 1; i < runs.size(); ++i) {
        if (runs[i].fixed != runs[i - 1].fixed) {
            reach = i;
            continue;
        }
        if (runs[i].lo <= runs[reach].hi) {
            sets.unite(runs[reach].id, runs[i].id);
        }
        if (runs[i].hi > runs[reach].hi) {
            reach = i;
        }
    }
}

}  // namespace

VerificationResult verifyConnectedNetwork(const std::vector<Point>& pins,
                                          const std::vector<Segment>& edges) {
    const auto fail = [](std::string message) {
        return VerificationResult{false, std::move(message)};
    };

    std::vector<Run> horizontals;
    std::vector<Run> verticals;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const Segment& e = edges[i];
        if (e.a.y == e.b.y) {
            horizontals.push_back({e.a.y, std::min(e.a.x, e.b.x), std::max(e.a.x, e.b.x), i});
        } else if (e.a.x == e.b.x) {
            verticals.push_back({e.a.x, std::min(e.a.y, e.b.y), std::max(e.a.y, e.b.y), i});
        } else {
            return fail("segment " + std::to_string(i) + " is not axis-parallel");
        }
    }
    std::sort(horizontals.begin(), horizontals.end(), lessByFixed);
    std::sort(verticals.begin(), verticals.end(), lessByFixed);

    DisjointSets sets(edges.size());
    uniteCollinear(horizontals, sets);
    uniteCollinear(verticals, sets);

    // Horizontal and vertical segments that touch or cross.
    const auto firstAtLeast = [](const std::vector<Run>& runs, Coord fixed) {
        return std::lower_bound(runs.begin(), runs.end(), fixed,
                                [](const Run& run, Coord value) { return run.fixed < value; });
    };
    for (const Run& vertical : verticals) {
        for (auto it = firstAtLeast(horizontals, vertical.lo);
             it != horizontals.end() && it->fixed <= vertical.hi; ++it) {
            if (it->lo <= vertical.fixed && vertical.fixed <= it->hi) {
                sets.unite(it->id, vertical.id);
            }
        }
    }

    // Every pin has to lie on the network.
    std::vector<std::size_t> pin_segment;
    for (std::size_t p = 0; p < pins.size(); ++p) {
        bool found = false;
        for (auto it = firstAtLeast(horizontals, pins[p].y);
             it != horizontals.end() && it->fixed == pins[p].y; ++it) {
            if (it->lo <= pins[p].x && pins[p].x <= it->hi) {
                pin_segment.push_back(it->id);
                found = true;
                break;
            }
        }
        for (auto it = firstAtLeast(verticals, pins[p].x);
             !found && it != verticals.end() && it->fixed == pins[p].x; ++it) {
            if (it->lo <= pins[p].y && pins[p].y <= it->hi) {
                pin_segment.push_back(it->id);
                found = true;
            }
        }
        if (!found) {
            // A net whose pins all coincide needs no edges.
            const bool all_equal =
                std::all_of(pins.begin(), pins.end(), [&](const Point& q) { return q == pins[0]; });
            if (all_equal && edges.empty()) {
                return {true, {}};
            }
            return fail("pin " + std::to_string(p) + " is not on the network");
        }
    }

    // One connected component.
    if (!edges.empty()) {
        const std::size_t root = sets.find(0);
        for (std::size_t i = 1; i < edges.size(); ++i) {
            if (sets.find(i) != root) {
                return fail("the network is not connected");
            }
        }
    }
    return {true, {}};
}

}  // namespace flute
