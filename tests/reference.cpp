#include "reference.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace flute::testing {

namespace {

double distance(const Point& a, const Point& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

void enumerate(const std::vector<Point>& candidates, std::size_t start, std::size_t budget,
               std::vector<Point>& current, double& best) {
    best = std::min(best, manhattanMstLength(current));
    if (budget == 0) {
        return;
    }
    for (std::size_t i = start; i < candidates.size(); ++i) {
        current.push_back(candidates[i]);
        enumerate(candidates, i + 1, budget - 1, current, best);
        current.pop_back();
    }
}

}  // namespace

double manhattanMstLength(const std::vector<Point>& points) {
    if (points.size() < 2) {
        return 0;
    }
    std::vector<double> key(points.size(), std::numeric_limits<double>::infinity());
    std::vector<bool> inTree(points.size(), false);
    key[0] = 0;
    double total = 0;
    for (std::size_t round = 0; round < points.size(); ++round) {
        std::size_t next = points.size();
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (!inTree[i] && (next == points.size() || key[i] < key[next])) {
                next = i;
            }
        }
        inTree[next] = true;
        total += key[next];
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (!inTree[i]) {
                key[i] = std::min(key[i], distance(points[next], points[i]));
            }
        }
    }
    return total;
}

double bruteForceRsmtLength(const std::vector<Point>& pins) {
    // distinct pins only
    std::vector<Point> terminals;
    for (const Point& pin : pins) {
        if (std::find(terminals.begin(), terminals.end(), pin) == terminals.end()) {
            terminals.push_back(pin);
        }
    }
    if (terminals.size() < 2) {
        return 0;
    }

    std::vector<double> xs;
    std::vector<double> ys;
    for (const Point& p : terminals) {
        xs.push_back(p.x);
        ys.push_back(p.y);
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

    std::vector<Point> candidates;
    for (double x : xs) {
        for (double y : ys) {
            const Point candidate{x, y};
            if (std::find(terminals.begin(), terminals.end(), candidate) == terminals.end()) {
                candidates.push_back(candidate);
            }
        }
    }
    double best = std::numeric_limits<double>::infinity();
    std::vector<Point> current = terminals;
    enumerate(candidates, 0, terminals.size() - 2, current, best);
    return best;
}

double exactRsmtLength(const std::vector<Point>& pins) {
    std::vector<Point> terminals;
    for (const Point& pin : pins) {
        if (std::find(terminals.begin(), terminals.end(), pin) == terminals.end()) {
            terminals.push_back(pin);
        }
    }
    const std::size_t k = terminals.size();
    if (k < 2) {
        return 0;
    }

    // Nodes of the Hanan grid; the shortest path between two nodes is their Manhattan distance.
    std::vector<double> xs;
    std::vector<double> ys;
    for (const Point& p : terminals) {
        xs.push_back(p.x);
        ys.push_back(p.y);
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    std::vector<Point> nodes;
    for (double x : xs) {
        for (double y : ys) {
            nodes.push_back({x, y});
        }
    }
    const std::size_t v = nodes.size();

    // cost[mask][node]: shortest tree connecting the terminals in mask and the node. The last
    // terminal is the root and not part of the masks.
    const std::size_t masks = std::size_t{1} << (k - 1);
    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<std::vector<double>> cost(masks, std::vector<double>(v, infinity));
    for (std::size_t i = 0; i + 1 < k; ++i) {
        for (std::size_t n = 0; n < v; ++n) {
            cost[std::size_t{1} << i][n] = distance(terminals[i], nodes[n]);
        }
    }
    for (std::size_t mask = 1; mask < masks; ++mask) {
        std::vector<double>& current = cost[mask];
        if ((mask & (mask - 1)) != 0) {  // at least two terminals: merge two subtrees at a node
            for (std::size_t sub = (mask - 1) & mask; sub > 0; sub = (sub - 1) & mask) {
                if (sub < (mask ^ sub)) {
                    continue;  // each split once
                }
                for (std::size_t n = 0; n < v; ++n) {
                    current[n] = std::min(current[n], cost[sub][n] + cost[mask ^ sub][n]);
                }
            }
        }
        // extend by a path to every node
        std::vector<double> relaxed = current;
        for (std::size_t from = 0; from < v; ++from) {
            for (std::size_t to = 0; to < v; ++to) {
                relaxed[to] =
                    std::min(relaxed[to], current[from] + distance(nodes[from], nodes[to]));
            }
        }
        current = relaxed;
    }
    double best = infinity;
    for (std::size_t n = 0; n < v; ++n) {
        if (nodes[n] == terminals[k - 1]) {
            best = cost[masks - 1][n];
        }
    }
    return best;
}

double halfPerimeter(const std::vector<Point>& pins) {
    double x_min = pins.front().x, x_max = x_min, y_min = pins.front().y, y_max = y_min;
    for (const Point& p : pins) {
        x_min = std::min(x_min, p.x);
        x_max = std::max(x_max, p.x);
        y_min = std::min(y_min, p.y);
        y_max = std::max(y_max, p.y);
    }
    return (x_max - x_min) + (y_max - y_min);
}

}  // namespace flute::testing
