// maxflow.cpp - PLACEHOLDER.
//
// Our own Edmonds-Karp max-flow solver (spec item 4) is implemented in
// milestone 3 (owner: Tan Ying Clarissa, branch m3-graphcut).
// Until that milestone is merged, every function below only reports that it is not there
// yet.
#include "seam/maxflow.hpp"

#include "seam/types.hpp"

namespace seam {

FlowNetwork::FlowNetwork(int) {
    throw NotImplementedYet("max-flow solver (milestone 3, Tan Ying Clarissa)");
}

void FlowNetwork::add_edge(int, int, long long) {
    throw NotImplementedYet("max-flow solver (milestone 3, Tan Ying Clarissa)");
}

long long FlowNetwork::max_flow(int, int) {
    throw NotImplementedYet("max-flow solver (milestone 3, Tan Ying Clarissa)");
}

std::vector<bool> FlowNetwork::reachable_from(int) const {
    throw NotImplementedYet("max-flow solver (milestone 3, Tan Ying Clarissa)");
}

long long FlowNetwork::memory_bytes() const {
    throw NotImplementedYet("max-flow solver (milestone 3, Tan Ying Clarissa)");
}

int FlowNetwork::node_count() const {
    throw NotImplementedYet("max-flow solver (milestone 3, Tan Ying Clarissa)");
}

int FlowNetwork::edge_count() const {
    throw NotImplementedYet("max-flow solver (milestone 3, Tan Ying Clarissa)");
}

long long FlowNetwork::augmentation_count() const {
    throw NotImplementedYet("max-flow solver (milestone 3, Tan Ying Clarissa)");
}

}  // namespace seam
