// Tests for the graph-cut seam finder (spec item 4).
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "seam/dp.hpp"
#include "seam/graphcut.hpp"
#include "test_support.hpp"

using seam::EnergyMap;
using seam::GraphCutResult;
using seam::Seam;
using seam::SinkLink;

TEST_CASE("graphcut: graph has the documented number of nodes and edges", "[graphcut]") {
    for (int rows = 1; rows <= 5; ++rows) {
        for (int cols = 1; cols <= 5; ++cols) {
            const EnergyMap energy = test_support::random_energy(rows, cols, 9, 1);
            const seam::FlowNetwork network =
                seam::build_seam_graph(energy, SinkLink::EnergyToSink);
            INFO("size " << rows << " x " << cols);
            CHECK(network.node_count() == rows * cols + 2);
            CHECK(network.edge_count() == 4 * rows * cols - 2 * rows - 2 * cols + 2);
        }
    }
}

TEST_CASE("graphcut: edges of a 2x2 image are exactly the ones in the spec", "[graphcut]") {
    const EnergyMap energy = test_support::make_energy({{5, 6}, {7, 8}});
    // Pixel nodes: (0,0)=0, (0,1)=1, (1,0)=2, (1,1)=3. Source = 4, sink = 5.
    const std::vector<seam::SeamGraphEdge> edges =
        seam::list_seam_graph_edges(energy, SinkLink::InfiniteToSink);
    const long long inf = seam::kInfiniteCapacity;

    // Returns the capacity of edge from -> to, or -1 if there is no such edge.
    const auto capacity_of = [&edges](int from, int to) {
        for (const seam::SeamGraphEdge& edge : edges) {
            if (edge.from == from && edge.to == to) {
                return edge.capacity;
            }
        }
        return -1LL;
    };

    CHECK(edges.size() == 10);
    CHECK(capacity_of(4, 0) == inf);  // source -> first column
    CHECK(capacity_of(4, 2) == inf);
    CHECK(capacity_of(1, 5) == inf);  // last column -> sink
    CHECK(capacity_of(3, 5) == inf);
    CHECK(capacity_of(0, 1) == 5);  // forward edges carry e[i][j]
    CHECK(capacity_of(2, 3) == 7);
    CHECK(capacity_of(1, 0) == inf);  // backward edges
    CHECK(capacity_of(3, 2) == inf);
    CHECK(capacity_of(1, 2) == inf);  // diagonal (0,1) -> (1,0)
    CHECK(capacity_of(3, 0) == inf);  // diagonal (1,1) -> (0,0)

    // With the default link, the sink edges carry the last column's energy instead.
    const std::vector<seam::SeamGraphEdge> padded =
        seam::list_seam_graph_edges(energy, SinkLink::EnergyToSink);
    bool found_six = false;
    bool found_eight = false;
    for (const seam::SeamGraphEdge& edge : padded) {
        found_six = found_six || (edge.from == 1 && edge.to == 5 && edge.capacity == 6);
        found_eight = found_eight || (edge.from == 3 && edge.to == 5 && edge.capacity == 8);
    }
    CHECK(found_six);
    CHECK(found_eight);
}

TEST_CASE("graphcut: seam cost == DP cost == max-flow value on random grids", "[graphcut]") {
    // The central correctness claim: min cut = minimum-energy seam.
    for (int rows = 1; rows <= 8; ++rows) {
        for (int cols = 1; cols <= 8; ++cols) {
            for (unsigned seed = 0; seed < 12; ++seed) {
                // Alternate between a wide value range and a tiny one (many ties, zeros).
                const int max_value = (seed % 2 == 0) ? 100 : 2;
                const EnergyMap energy = test_support::random_energy(rows, cols, max_value, seed);
                const GraphCutResult cut = seam::find_vertical_seam_graphcut_detailed(energy);
                const long long dp_cost =
                    seam::seam_cost(energy, seam::find_vertical_seam_dp(energy));

                INFO("size " << rows << " x " << cols << ", seed " << seed);
                REQUIRE(seam::is_valid_seam(cut.seam, rows, cols));
                CHECK(seam::seam_cost(energy, cut.seam) == dp_cost);
                CHECK(cut.max_flow == dp_cost);
            }
        }
    }
}

TEST_CASE("graphcut: agrees with DP on larger random grids", "[graphcut]") {
    for (unsigned seed = 0; seed < 10; ++seed) {
        const EnergyMap energy = test_support::random_energy(25, 30, 255, seed);
        const GraphCutResult cut = seam::find_vertical_seam_graphcut_detailed(energy);
        const long long dp_cost = seam::seam_cost(energy, seam::find_vertical_seam_dp(energy));
        REQUIRE(seam::is_valid_seam(cut.seam, 25, 30));
        CHECK(seam::seam_cost(energy, cut.seam) == dp_cost);
        CHECK(cut.max_flow == dp_cost);
    }
}

TEST_CASE("graphcut: brute force confirms optimality on tiny grids", "[graphcut]") {
    for (unsigned seed = 0; seed < 40; ++seed) {
        const int rows = 1 + static_cast<int>(seed % 5);
        const int cols = 1 + static_cast<int>((seed * 3) % 5);
        const EnergyMap energy = test_support::random_energy(rows, cols, 9, seed);
        const Seam seam = seam::find_vertical_seam_graphcut(energy);
        CHECK(seam::seam_cost(energy, seam) == test_support::brute_force_min_cost(energy));
    }
}

TEST_CASE("graphcut: returns the leftmost of all optimal seams", "[graphcut]") {
    // The BFS from the source finds the SMALLEST possible source side among all minimum
    // cuts. So when several seams share the minimum cost, graph cut returns, in every
    // row, the smallest column that any optimal seam uses in that row. (DP breaks ties
    // differently, which is why the two can return different seams of equal cost.)
    for (unsigned seed = 0; seed < 60; ++seed) {
        const int rows = 1 + static_cast<int>(seed % 5);
        const int cols = 1 + static_cast<int>((seed * 3) % 5);
        // Values 0..2 make ties very common.
        const EnergyMap energy = test_support::random_energy(rows, cols, 2, seed);
        const long long best = test_support::brute_force_min_cost(energy);

        Seam leftmost(rows, cols);
        for (const Seam& candidate : test_support::all_seams(rows, cols)) {
            if (seam::seam_cost(energy, candidate) != best) {
                continue;
            }
            for (int row = 0; row < rows; ++row) {
                leftmost[row] = std::min(leftmost[row], candidate[row]);
            }
        }
        INFO("seed " << seed << ", size " << rows << " x " << cols);
        CHECK(seam::find_vertical_seam_graphcut(energy) == leftmost);
    }
}

TEST_CASE("graphcut: literal spec graph can never remove the last column", "[graphcut][finding]") {
    // FINDING (documented in the report, section 3.4): when the last column is tied to the
    // sink with INFINITE capacity, exactly as the spec's text says, the last column is
    // always on the sink side. The seam is then the best seam inside columns 0 .. cols-2,
    // which can cost more than the true optimum.
    const EnergyMap energy = test_support::make_energy({
        {9, 9, 1},
        {9, 9, 1},
        {9, 9, 1},
    });
    const GraphCutResult literal =
        seam::find_vertical_seam_graphcut_detailed(energy, SinkLink::InfiniteToSink);
    const GraphCutResult padded =
        seam::find_vertical_seam_graphcut_detailed(energy, SinkLink::EnergyToSink);
    const Seam optimal = seam::find_vertical_seam_dp(energy);

    CHECK(optimal == Seam{2, 2, 2});
    CHECK(seam::seam_cost(energy, optimal) == 3);

    CHECK(padded.seam == Seam{2, 2, 2});  // default link: finds the optimum
    CHECK(padded.max_flow == 3);

    CHECK(seam::seam_cost(energy, literal.seam) == 27);  // literal link: misses it
    CHECK(literal.max_flow == 27);
    for (int column : literal.seam) {
        CHECK(column < energy.cols - 1);
    }
}

TEST_CASE("graphcut: literal spec graph == DP restricted to the first cols-1 columns",
          "[graphcut][finding]") {
    for (int rows = 1; rows <= 7; ++rows) {
        for (int cols = 2; cols <= 7; ++cols) {
            for (unsigned seed = 0; seed < 10; ++seed) {
                const EnergyMap energy = test_support::random_energy(rows, cols, 20, seed);
                const GraphCutResult literal =
                    seam::find_vertical_seam_graphcut_detailed(energy, SinkLink::InfiniteToSink);

                // DP on the image without its last column.
                const EnergyMap without_last = energy.colRange(0, cols - 1).clone();
                const long long restricted_cost =
                    seam::seam_cost(without_last, seam::find_vertical_seam_dp(without_last));
                const long long full_cost =
                    seam::seam_cost(energy, seam::find_vertical_seam_dp(energy));

                INFO("size " << rows << " x " << cols << ", seed " << seed);
                REQUIRE(seam::is_valid_seam(literal.seam, rows, cols));
                CHECK(literal.max_flow == restricted_cost);
                CHECK(seam::seam_cost(energy, literal.seam) == restricted_cost);
                CHECK(literal.max_flow >= full_cost);
            }
        }
    }
}

TEST_CASE("graphcut: the source side is a prefix of every row", "[graphcut]") {
    // Direct check of the structural claim behind "rightmost pixel in S".
    const int rows = 6;
    const int cols = 7;
    for (unsigned seed = 0; seed < 20; ++seed) {
        const EnergyMap energy = test_support::random_energy(rows, cols, 5, seed);
        seam::FlowNetwork network = seam::build_seam_graph(energy, SinkLink::EnergyToSink);
        const int source = rows * cols;
        const int sink = rows * cols + 1;
        network.max_flow(source, sink);
        const std::vector<bool> in_source_side = network.reachable_from(source);

        CHECK_FALSE(in_source_side[sink]);
        for (int row = 0; row < rows; ++row) {
            CHECK(in_source_side[seam::pixel_node(row, 0, cols)]);  // column 0 always in S
            for (int col = 0; col + 1 < cols; ++col) {
                // If a pixel is in S, its left neighbour must be in S as well.
                if (in_source_side[seam::pixel_node(row, col + 1, cols)]) {
                    CHECK(in_source_side[seam::pixel_node(row, col, cols)]);
                }
            }
        }
    }
}

TEST_CASE("graphcut: edge cases", "[graphcut]") {
    SECTION("one column: the only seam, in both link modes") {
        const EnergyMap energy = test_support::make_energy({{4}, {5}, {6}});
        for (SinkLink link : {SinkLink::EnergyToSink, SinkLink::InfiniteToSink}) {
            const GraphCutResult cut = seam::find_vertical_seam_graphcut_detailed(energy, link);
            CHECK(cut.seam == Seam{0, 0, 0});
            CHECK(cut.max_flow == 15);
        }
    }
    SECTION("one row: the cheapest pixel") {
        const EnergyMap energy = test_support::make_energy({{4, 2, 9, 3}});
        const GraphCutResult cut = seam::find_vertical_seam_graphcut_detailed(energy);
        CHECK(cut.seam == Seam{1});
        CHECK(cut.max_flow == 2);
    }
    SECTION("all-zero energy: flow 0 and still a valid seam") {
        const EnergyMap energy(5, 6, 0);
        const GraphCutResult cut = seam::find_vertical_seam_graphcut_detailed(energy);
        CHECK(cut.max_flow == 0);
        CHECK(seam::is_valid_seam(cut.seam, 5, 6));
    }
    SECTION("constant non-zero energy: every seam costs rows * value") {
        const EnergyMap energy(4, 5, 3);
        const GraphCutResult cut = seam::find_vertical_seam_graphcut_detailed(energy);
        CHECK(cut.max_flow == 12);
        CHECK(seam::is_valid_seam(cut.seam, 4, 5));
        CHECK(seam::seam_cost(energy, cut.seam) == 12);
    }
    SECTION("empty input is rejected") {
        CHECK_THROWS_AS(seam::find_vertical_seam_graphcut(EnergyMap()), std::invalid_argument);
    }
}

TEST_CASE("graphcut: extract_seam_from_cut takes the rightmost source-side pixel", "[graphcut]") {
    // 2 rows x 4 cols, plus source and sink flags at the end.
    //            row 0: S S S T     row 1: S S T T     source, sink
    const std::vector<bool> in_source_side = {true, true,  true,  false, true,
                                              true, false, false, true,  false};
    CHECK(seam::extract_seam_from_cut(in_source_side, 2, 4) == Seam{2, 1});
}
