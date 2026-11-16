// maxflow.hpp - our own max-flow solver (Edmonds-Karp) used by the graph-cut seam finder.
//
// Edmonds-Karp = Ford-Fulkerson where every augmenting path is found by BFS, i.e. it is a
// SHORTEST path (fewest edges) in the residual graph. That choice bounds the number of
// augmentations by O(V * E), which gives the O(V * E^2) total running time.
#pragma once

#include <limits>
#include <vector>

namespace seam {

// Capacity used for "infinite" edges.
// It must be larger than any possible finite cut (the sum of all energies), so that an
// infinite edge is never saturated and never part of a minimum cut.
// Why max / 4 and not max itself: inside max_flow the two residuals of an edge pair
// always add up to the capacity of the real (forward) edge of that pair, so a residual
// never exceeds that capacity and the solver alone cannot overflow. The headroom is for
// code that ADDS capacities, for example the capacity of a cut that contains a few
// infinite edges: up to four infinite values can be added without overflowing a long long.
constexpr long long kInfiniteCapacity = std::numeric_limits<long long>::max() / 4;

// A directed flow network stored as adjacency lists of residual edges.
class FlowNetwork {
public:
    // Creates a network with `node_count` nodes (numbered 0 .. node_count-1) and no edges.
    explicit FlowNetwork(int node_count);

    // Adds a directed edge from -> to with the given capacity.
    // Also adds the paired reverse edge to -> from with capacity 0; pushing flow on the
    // forward edge increases the reverse edge's residual capacity, which lets a later
    // augmenting path "undo" that flow.
    // Time O(1) amortised.
    void add_edge(int from, int to, long long capacity);

    // Edmonds-Karp. Repeatedly finds a shortest augmenting path with BFS and pushes the
    // bottleneck capacity along it, until the sink can no longer be reached.
    // Returns the value of the maximum flow (= capacity of the minimum cut).
    // Time O(V * E^2), space O(V + E).
    long long max_flow(int source, int sink);

    // After max_flow(): the source side S of the minimum cut, i.e. every node that can
    // still be reached from the source through edges with residual capacity > 0.
    // Returns reachable[node] == true for the nodes in S.
    // Time O(V + E), space O(V).
    std::vector<bool> reachable_from(int source) const;

    // Approximate number of bytes used by the edges and adjacency lists (for the report's
    // memory comparison with the DP tables).
    long long memory_bytes() const;

    int node_count() const;
    // Number of directed edges added with add_edge (reverse edges are not counted).
    int edge_count() const;
    // Number of augmenting paths used by the last max_flow() call (for the benchmarks).
    long long augmentation_count() const;

private:
    struct ResidualEdge {
        int to;                       // node this edge points to
        long long residual_capacity;  // how much more flow this edge can still take
    };

    // Edges are stored in pairs: edge 2k is a real edge and edge 2k+1 is its reverse.
    // So the partner of edge e is always (e XOR 1).
    std::vector<ResidualEdge> edges_;
    // outgoing_[node] lists the indices (into edges_) of the edges leaving that node.
    std::vector<std::vector<int>> outgoing_;
    long long augmentation_count_ = 0;

    // Index of the partner of an edge: the reverse of a real edge, or the real edge of a
    // reverse edge. Because partners are stored at indices 2k and 2k+1, flipping the last
    // bit (XOR with 1) switches between them: 6 <-> 7, 8 <-> 9, ...
    static int partner_of(int edge_index);

    // Node that an edge starts from. An edge only stores where it points TO, but its
    // partner points the opposite way, so the partner's `to` is this edge's start.
    int start_node_of(int edge_index) const;

    // BFS from the source over edges with residual capacity > 0.
    // Fills edge_into[node] with the index of the edge used to reach `node` (-1 if not
    // reached). Returns true if the sink was reached.
    bool find_shortest_augmenting_path(int source, int sink, std::vector<int>& edge_into) const;
};

}  // namespace seam
