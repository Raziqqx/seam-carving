// graphcut.cpp - PLACEHOLDER.
//
// The graph-cut seam finder (spec item 4) is implemented in
// milestone 3 (owner: Tan Ying Clarissa, branch m3-graphcut).
// Until that milestone is merged, every function below only reports that it is not there
// yet: programs print "not implemented yet" and tests that need it are skipped.
#include "seam/graphcut.hpp"

namespace seam {

int pixel_node(int, int, int) {
    throw NotImplementedYet("graph-cut seam finder (milestone 3, Tan Ying Clarissa)");
}

std::vector<SeamGraphEdge> list_seam_graph_edges(const EnergyMap&, SinkLink) {
    throw NotImplementedYet("graph-cut seam finder (milestone 3, Tan Ying Clarissa)");
}

FlowNetwork build_seam_graph(const EnergyMap&, SinkLink) {
    throw NotImplementedYet("graph-cut seam finder (milestone 3, Tan Ying Clarissa)");
}

Seam extract_seam_from_cut(const std::vector<bool>&, int, int) {
    throw NotImplementedYet("graph-cut seam finder (milestone 3, Tan Ying Clarissa)");
}

GraphCutResult find_vertical_seam_graphcut_detailed(const EnergyMap&, SinkLink) {
    throw NotImplementedYet("graph-cut seam finder (milestone 3, Tan Ying Clarissa)");
}

Seam find_vertical_seam_graphcut(const EnergyMap&) {
    throw NotImplementedYet("graph-cut seam finder (milestone 3, Tan Ying Clarissa)");
}

}  // namespace seam
