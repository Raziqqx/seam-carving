// graphcut.hpp - seam finding as a minimum s-t cut (spec item 4).
//
// Graph for a VERTICAL seam on an image with `rows` x `cols` pixels:
//   * one node per pixel, plus a source node and a sink node;
//   * source -> every pixel of the first column, capacity infinity;
//   * forward edge  (i, j) -> (i, j+1), capacity e[i][j]  (the cost of removing (i, j));
//   * backward edge (i, j+1) -> (i, j), capacity infinity;
//   * diagonal edges (i, j) -> (i-1, j-1) and (i, j) -> (i+1, j-1), capacity infinity;
//   * last column -> sink: see SinkLink below.
//
// Why the infinite edges: a minimum cut can never cut an infinite edge, so if a pixel is
// on the source side S, every pixel its infinite edges point to must be in S too.
//   - backward edges  => S is a "prefix" of every row: columns 0 .. c_i for some c_i.
//                        So the cut crosses each row exactly once, at forward edge c_i.
//   - diagonal edges  => c_(i-1) >= c_i - 1 and c_(i+1) >= c_i - 1, i.e. neighbouring
//                        rows differ by at most one column: the cut is a connected seam.
// The capacity of the cut is the sum of e[i][c_i] = the energy of that seam, so the
// MINIMUM cut is the MINIMUM-energy seam, and the rightmost S pixel of row i is c_i.
#pragma once

#include <vector>

#include "seam/maxflow.hpp"
#include "seam/types.hpp"

namespace seam {

// How the last column is connected to the sink.
enum class SinkLink {
    // Default. Last-column pixel (i, cols-1) -> sink with capacity e[i][cols-1].
    // This is the spec's construction applied to the image padded with one imaginary
    // extra column on the right: the forward edge out of the last real pixel then
    // carries that pixel's removal cost, and the imaginary column is merged into the sink.
    // With this link the cut can choose ANY column, so the result equals the DP optimum.
    EnergyToSink,
    // The spec's text read literally: last-column pixel -> sink with INFINITE capacity.
    // The last column then can never be on the source side, so the seam can never
    // remove a last-column pixel and e[i][cols-1] is never used. The result is the
    // optimum over columns 0 .. cols-2 only (see the report, section 3.4, and the tests).
    InfiniteToSink
};

// Result of one graph-cut seam search.
struct GraphCutResult {
    Seam seam;                // seam[row] = column to remove
    long long max_flow;       // max-flow value = capacity of the minimum cut
    int node_count;           // V of the flow network
    int edge_count;           // E of the flow network (reverse residual edges not counted)
    long long augmentations;  // number of augmenting paths Edmonds-Karp needed
};

// Node id of pixel (row, col) in the flow network. Pixels are numbered row by row.
int pixel_node(int row, int col, int cols);

// One directed edge of the seam graph.
struct SeamGraphEdge {
    int from;
    int to;
    long long capacity;
};

// Lists every edge of the graph described at the top of this file.
// Node ids: pixels 0 .. rows*cols-1, then source = rows*cols, sink = rows*cols + 1.
// The graph has V = rows*cols + 2 nodes and E = 4*rows*cols - 2*rows - 2*cols + 2 edges.
// Time O(rows * cols), space O(rows * cols).
std::vector<SeamGraphEdge> list_seam_graph_edges(const EnergyMap& energy, SinkLink sink_link);

// Builds the flow network from that edge list.
// Time O(rows * cols), space O(rows * cols).
FlowNetwork build_seam_graph(const EnergyMap& energy, SinkLink sink_link);

// Reads the seam off the source side of the cut: in every row, the rightmost pixel that
// is still reachable from the source is the pixel to remove.
// Time O(rows * cols), space O(rows).
Seam extract_seam_from_cut(const std::vector<bool>& in_source_side, int rows, int cols);

// Full graph-cut seam search with statistics: build graph, run our Edmonds-Karp, BFS for
// the source side, read off the seam.
// Time O(V * E^2) worst case with V = rows*cols and E = O(rows*cols), i.e.
// O((rows*cols)^3); space O(rows * cols).
GraphCutResult find_vertical_seam_graphcut_detailed(const EnergyMap& energy,
                                                    SinkLink sink_link = SinkLink::EnergyToSink);

// Convenience wrapper that returns only the seam (default sink link).
Seam find_vertical_seam_graphcut(const EnergyMap& energy);

}  // namespace seam
