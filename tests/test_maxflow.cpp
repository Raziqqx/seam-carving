// Tests for our Edmonds-Karp max-flow solver on small graphs with known answers.
#include <catch2/catch_test_macros.hpp>

#include "seam/maxflow.hpp"

using seam::FlowNetwork;

TEST_CASE("maxflow: single edge", "[maxflow]") {
    FlowNetwork network(2);
    network.add_edge(0, 1, 7);
    CHECK(network.max_flow(0, 1) == 7);
    CHECK(network.augmentation_count() == 1);
}

TEST_CASE("maxflow: a path is limited by its smallest edge (bottleneck)", "[maxflow]") {
    FlowNetwork network(4);
    network.add_edge(0, 1, 5);
    network.add_edge(1, 2, 2);
    network.add_edge(2, 3, 9);
    CHECK(network.max_flow(0, 3) == 2);
}

TEST_CASE("maxflow: parallel paths add up", "[maxflow]") {
    FlowNetwork network(4);
    network.add_edge(0, 1, 3);
    network.add_edge(1, 3, 3);
    network.add_edge(0, 2, 4);
    network.add_edge(2, 3, 2);
    CHECK(network.max_flow(0, 3) == 5);
}

TEST_CASE("maxflow: textbook network (CLRS figure 26.1) has max flow 23", "[maxflow]") {
    // Nodes: 0 = s, 1 = v1, 2 = v2, 3 = v3, 4 = v4, 5 = t.
    FlowNetwork network(6);
    network.add_edge(0, 1, 16);
    network.add_edge(0, 2, 13);
    network.add_edge(1, 3, 12);
    network.add_edge(2, 1, 4);
    network.add_edge(2, 4, 14);
    network.add_edge(3, 2, 9);
    network.add_edge(3, 5, 20);
    network.add_edge(4, 3, 7);
    network.add_edge(4, 5, 4);
    CHECK(network.max_flow(0, 5) == 23);

    // Min cut of this network: S = {s, v1, v2, v4}, T = {v3, t}.
    const std::vector<bool> in_source_side = network.reachable_from(0);
    CHECK(in_source_side == std::vector<bool>{true, true, true, false, true, false});
}

TEST_CASE("maxflow: the reverse edge lets a later path undo an earlier choice", "[maxflow]") {
    // Classic diamond with a cross edge 1 -> 2. If flow first goes s-1-2-t, the only way
    // to reach the optimum of 2 is to push flow BACK over 1 -> 2 using its reverse edge.
    FlowNetwork network(4);
    network.add_edge(0, 1, 1);
    network.add_edge(0, 2, 1);
    network.add_edge(1, 2, 1);
    network.add_edge(1, 3, 1);
    network.add_edge(2, 3, 1);
    CHECK(network.max_flow(0, 3) == 2);
}

TEST_CASE("maxflow: disconnected sink gives zero flow", "[maxflow]") {
    FlowNetwork network(3);
    network.add_edge(0, 1, 10);
    CHECK(network.max_flow(0, 2) == 0);
    CHECK(network.reachable_from(0) == std::vector<bool>{true, true, false});
}

TEST_CASE("maxflow: zero-capacity edges carry nothing", "[maxflow]") {
    FlowNetwork network(3);
    network.add_edge(0, 1, 0);
    network.add_edge(1, 2, 5);
    CHECK(network.max_flow(0, 2) == 0);
    CHECK(network.reachable_from(0) == std::vector<bool>{true, false, false});
}

TEST_CASE("maxflow: infinite edges are never the bottleneck", "[maxflow]") {
    FlowNetwork network(4);
    network.add_edge(0, 1, seam::kInfiniteCapacity);
    network.add_edge(1, 2, 6);
    network.add_edge(2, 3, seam::kInfiniteCapacity);
    CHECK(network.max_flow(0, 3) == 6);
}

TEST_CASE("maxflow: bookkeeping and invalid input", "[maxflow]") {
    FlowNetwork network(3);
    network.add_edge(0, 1, 1);
    network.add_edge(1, 2, 1);
    CHECK(network.node_count() == 3);
    CHECK(network.edge_count() == 2);
    CHECK_THROWS_AS(network.add_edge(0, 1, -1), std::invalid_argument);
    CHECK_THROWS_AS(network.max_flow(1, 1), std::invalid_argument);
}
