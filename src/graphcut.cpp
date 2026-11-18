#include "seam/graphcut.hpp"

#include <stdexcept>

namespace seam {

// Node number of pixel (row, col): pixels are numbered row by row.
int pixel_node(int row, int col, int cols) {
    return row * cols + col;
}

// Every edge of the seam graph of spec item 4 (see graphcut.hpp for the picture).
// Time O(rows * cols); the list has 4*rows*cols - 2*rows - 2*cols + 2 edges.
std::vector<SeamGraphEdge> list_seam_graph_edges(const EnergyMap& energy, SinkLink sink_link) {
    const int rows = energy.rows;
    const int cols = energy.cols;
    const int source = rows * cols;
    const int sink = rows * cols + 1;
    std::vector<SeamGraphEdge> edges;

    for (int row = 0; row < rows; ++row) {
        // The first column is always on the source side.
        edges.push_back({source, pixel_node(row, 0, cols), kInfiniteCapacity});

        // The last column is tied to the sink.
        const long long sink_capacity = (sink_link == SinkLink::EnergyToSink)
                                            ? static_cast<long long>(energy(row, cols - 1))
                                            : kInfiniteCapacity;
        edges.push_back({pixel_node(row, cols - 1, cols), sink, sink_capacity});

        for (int col = 0; col < cols; ++col) {
            const int here = pixel_node(row, col, cols);

            if (col + 1 < cols) {
                const int right = pixel_node(row, col + 1, cols);
                // Forward edge: cutting it means "the seam removes pixel (row, col)".
                edges.push_back({here, right, energy(row, col)});
                // Backward edge, infinite: if (row, col+1) is on the source side then
                // (row, col) must be too. This stops a cut from crossing a row twice.
                edges.push_back({right, here, kInfiniteCapacity});
            }

            if (col - 1 >= 0) {
                // Diagonal edges, infinite: if (row, col) is on the source side then the
                // pixels up-left and down-left of it must be too. This keeps the cut
                // positions of neighbouring rows within one column of each other.
                if (row - 1 >= 0) {
                    edges.push_back({here, pixel_node(row - 1, col - 1, cols), kInfiniteCapacity});
                }
                if (row + 1 < rows) {
                    edges.push_back({here, pixel_node(row + 1, col - 1, cols), kInfiniteCapacity});
                }
            }
        }
    }
    return edges;
}

// Flow network built from the edge list. Time O(rows * cols).
FlowNetwork build_seam_graph(const EnergyMap& energy, SinkLink sink_link) {
    FlowNetwork network(energy.rows * energy.cols + 2);
    for (const SeamGraphEdge& edge : list_seam_graph_edges(energy, sink_link)) {
        network.add_edge(edge.from, edge.to, edge.capacity);
    }
    return network;
}

// Seam = the rightmost source-side pixel of every row. Time O(rows * cols).
Seam extract_seam_from_cut(const std::vector<bool>& in_source_side, int rows, int cols) {
    Seam seam(rows);
    for (int row = 0; row < rows; ++row) {
        // Column 0 is always in S (infinite edge from the source), so start there and
        // move right while the next pixel is still in S.
        int rightmost = 0;
        while (rightmost + 1 < cols && in_source_side[pixel_node(row, rightmost + 1, cols)]) {
            ++rightmost;
        }
        seam[row] = rightmost;
    }
    return seam;
}

// Optimal seam by min cut: build graph, max-flow, residual BFS, read off the seam.
// Time O(V * E^2) worst case (Edmonds-Karp), space O(rows * cols).
GraphCutResult find_vertical_seam_graphcut_detailed(const EnergyMap& energy, SinkLink sink_link) {
    if (energy.empty()) {
        throw std::invalid_argument("find_vertical_seam_graphcut: input is empty");
    }
    const int rows = energy.rows;
    const int cols = energy.cols;

    // Special case: with a single column and an infinite sink link the pixel would be
    // tied to BOTH terminals by infinite edges, so no finite cut exists. The only
    // possible seam is column 0 in every row, so we return it directly.
    if (cols == 1 && sink_link == SinkLink::InfiniteToSink) {
        const Seam only_seam(rows, 0);
        return {only_seam, seam_cost(energy, only_seam), 0, 0, 0};
    }

    const int source = rows * cols;
    const int sink = rows * cols + 1;
    FlowNetwork network = build_seam_graph(energy, sink_link);

    GraphCutResult result;
    result.max_flow = network.max_flow(source, sink);
    // S = nodes still reachable from the source in the residual graph.
    const std::vector<bool> in_source_side = network.reachable_from(source);
    result.seam = extract_seam_from_cut(in_source_side, rows, cols);
    result.node_count = network.node_count();
    result.edge_count = network.edge_count();
    result.augmentations = network.augmentation_count();
    return result;
}

// Graph-cut seam with the default sink link.
Seam find_vertical_seam_graphcut(const EnergyMap& energy) {
    return find_vertical_seam_graphcut_detailed(energy).seam;
}

}  // namespace seam
