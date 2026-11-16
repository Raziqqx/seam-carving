// Optional cross-check against Boost Graph Library (only built when Boost is found).
// Boost is used ONLY here, as an independent referee for our own Edmonds-Karp. The
// library never links against it.
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/edmonds_karp_max_flow.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>

#include "seam/graphcut.hpp"
#include "seam/maxflow.hpp"
#include "test_support.hpp"

namespace {

using Traits = boost::adjacency_list_traits<boost::vecS, boost::vecS, boost::directedS>;
using BoostGraph = boost::adjacency_list<
    boost::vecS, boost::vecS, boost::directedS, boost::no_property,
    boost::property<
        boost::edge_capacity_t, long long,
        boost::property<boost::edge_residual_capacity_t, long long,
                        boost::property<boost::edge_reverse_t, Traits::edge_descriptor>>>>;

// Max-flow value of the given edge list according to Boost's Edmonds-Karp.
long long boost_max_flow(int node_count, const std::vector<seam::SeamGraphEdge>& edges, int source,
                         int sink) {
    BoostGraph graph(node_count);
    auto capacity = boost::get(boost::edge_capacity, graph);
    auto reverse = boost::get(boost::edge_reverse, graph);
    for (const seam::SeamGraphEdge& edge : edges) {
        // Boost needs every edge to be paired with an explicit reverse edge.
        const auto forward = boost::add_edge(edge.from, edge.to, graph).first;
        const auto backward = boost::add_edge(edge.to, edge.from, graph).first;
        capacity[forward] = edge.capacity;
        capacity[backward] = 0;
        reverse[forward] = backward;
        reverse[backward] = forward;
    }
    return boost::edmonds_karp_max_flow(graph, source, sink);
}

long long our_max_flow(int node_count, const std::vector<seam::SeamGraphEdge>& edges, int source,
                       int sink) {
    seam::FlowNetwork network(node_count);
    for (const seam::SeamGraphEdge& edge : edges) {
        network.add_edge(edge.from, edge.to, edge.capacity);
    }
    return network.max_flow(source, sink);
}

}  // namespace

TEST_CASE("boost cross-check: seam graphs give the same max flow", "[graphcut][boost]") {
    for (seam::SinkLink link : {seam::SinkLink::EnergyToSink, seam::SinkLink::InfiniteToSink}) {
        for (unsigned seed = 0; seed < 60; ++seed) {
            const int rows = 1 + static_cast<int>(seed % 9);
            const int cols = 2 + static_cast<int>((seed * 5) % 9);
            const seam::EnergyMap energy = test_support::random_energy(rows, cols, 50, seed);
            const std::vector<seam::SeamGraphEdge> edges =
                seam::list_seam_graph_edges(energy, link);
            const int source = rows * cols;
            const int sink = rows * cols + 1;

            INFO("seed " << seed << ", size " << rows << " x " << cols);
            CHECK(our_max_flow(rows * cols + 2, edges, source, sink) ==
                  boost_max_flow(rows * cols + 2, edges, source, sink));
        }
    }
}

TEST_CASE("boost cross-check: random general graphs give the same max flow", "[maxflow][boost]") {
    std::mt19937 generator(2024);
    for (int trial = 0; trial < 100; ++trial) {
        const int node_count = 2 + static_cast<int>(generator() % 10);
        const int edge_count = static_cast<int>(generator() % 40);
        std::vector<seam::SeamGraphEdge> edges;
        for (int index = 0; index < edge_count; ++index) {
            const int from = static_cast<int>(generator() % node_count);
            const int to = static_cast<int>(generator() % node_count);
            if (from != to) {
                edges.push_back({from, to, static_cast<long long>(generator() % 20)});
            }
        }
        INFO("trial " << trial);
        CHECK(our_max_flow(node_count, edges, 0, node_count - 1) ==
              boost_max_flow(node_count, edges, 0, node_count - 1));
    }
}
