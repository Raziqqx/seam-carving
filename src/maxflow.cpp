#include "seam/maxflow.hpp"

#include <algorithm>
#include <queue>
#include <stdexcept>

namespace seam {

// Empty network with the given number of nodes.
FlowNetwork::FlowNetwork(int node_count) : outgoing_(node_count) {}

// Adds a real edge and its (initially empty) reverse edge. Time O(1) amortised.
void FlowNetwork::add_edge(int from, int to, long long capacity) {
    if (capacity < 0) {
        throw std::invalid_argument("FlowNetwork::add_edge: capacity must not be negative");
    }
    const int forward_index = static_cast<int>(edges_.size());
    edges_.push_back({to, capacity});  // real edge (even index)
    edges_.push_back({from, 0});       // reverse edge (odd index), starts empty
    outgoing_[from].push_back(forward_index);
    outgoing_[to].push_back(forward_index + 1);
}

// Partner of an edge (real <-> reverse): flip the last bit of the index.
int FlowNetwork::partner_of(int edge_index) {
    return edge_index ^ 1;
}

// Node an edge starts from = where its partner points to.
int FlowNetwork::start_node_of(int edge_index) const {
    return edges_[partner_of(edge_index)].to;
}

// BFS in the residual graph; records the edge used to reach each node.
// Time O(V + E), space O(V).
bool FlowNetwork::find_shortest_augmenting_path(int source, int sink,
                                                std::vector<int>& edge_into) const {
    std::fill(edge_into.begin(), edge_into.end(), -1);
    std::vector<bool> visited(outgoing_.size(), false);
    std::queue<int> frontier;
    frontier.push(source);
    visited[source] = true;

    while (!frontier.empty()) {
        const int node = frontier.front();
        frontier.pop();
        for (int edge_index : outgoing_[node]) {
            const ResidualEdge& edge = edges_[edge_index];
            // Only edges that can still carry flow exist in the residual graph.
            if (edge.residual_capacity <= 0 || visited[edge.to]) {
                continue;
            }
            visited[edge.to] = true;
            edge_into[edge.to] = edge_index;
            if (edge.to == sink) {
                return true;  // BFS reaches the sink first along a shortest path
            }
            frontier.push(edge.to);
        }
    }
    return false;
}

// Edmonds-Karp: augment along BFS shortest paths until the sink is unreachable.
// Time O(V * E^2): at most O(V * E) augmentations, each O(E). Space O(V + E).
long long FlowNetwork::max_flow(int source, int sink) {
    if (source == sink) {
        throw std::invalid_argument("FlowNetwork::max_flow: source and sink must differ");
    }
    long long total_flow = 0;
    augmentation_count_ = 0;
    std::vector<int> edge_into(outgoing_.size(), -1);

    while (find_shortest_augmenting_path(source, sink, edge_into)) {
        // Pass 1: walk back from the sink to the source along the BFS path and find the
        // bottleneck, the smallest residual capacity on the path.
        long long bottleneck = kInfiniteCapacity;
        for (int node = sink; node != source; node = start_node_of(edge_into[node])) {
            bottleneck = std::min(bottleneck, edges_[edge_into[node]].residual_capacity);
        }

        // Pass 2: push `bottleneck` units along the path. Each edge loses that much
        // residual capacity and its partner (the reverse edge) gains it.
        for (int node = sink; node != source; node = start_node_of(edge_into[node])) {
            const int edge_index = edge_into[node];
            edges_[edge_index].residual_capacity -= bottleneck;
            edges_[partner_of(edge_index)].residual_capacity += bottleneck;
        }

        total_flow += bottleneck;
        ++augmentation_count_;
    }
    return total_flow;
}

// Source side of the minimum cut: nodes reachable through residual capacity > 0.
// Time O(V + E), space O(V).
std::vector<bool> FlowNetwork::reachable_from(int source) const {
    std::vector<bool> reachable(outgoing_.size(), false);
    std::queue<int> frontier;
    frontier.push(source);
    reachable[source] = true;

    while (!frontier.empty()) {
        const int node = frontier.front();
        frontier.pop();
        for (int edge_index : outgoing_[node]) {
            const ResidualEdge& edge = edges_[edge_index];
            if (edge.residual_capacity > 0 && !reachable[edge.to]) {
                reachable[edge.to] = true;
                frontier.push(edge.to);
            }
        }
    }
    return reachable;
}

int FlowNetwork::node_count() const {
    return static_cast<int>(outgoing_.size());
}

int FlowNetwork::edge_count() const {
    return static_cast<int>(edges_.size() / 2);
}

// Approximate memory held by the network: the residual edges plus the adjacency lists.
long long FlowNetwork::memory_bytes() const {
    long long bytes = static_cast<long long>(edges_.capacity() * sizeof(ResidualEdge));
    for (const std::vector<int>& edge_list : outgoing_) {
        bytes += static_cast<long long>(sizeof(edge_list) + edge_list.capacity() * sizeof(int));
    }
    return bytes;
}

long long FlowNetwork::augmentation_count() const {
    return augmentation_count_;
}

}  // namespace seam
