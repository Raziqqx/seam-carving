// Tests for the dynamic-programming seam finder (spec items 1b, 2a-ii).
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>

#include "seam/dp.hpp"
#include "test_support.hpp"

using seam::EnergyMap;
using seam::Seam;

TEST_CASE("dp: hand-worked 4x4 example (table, parents and seam)", "[dp]") {
    const EnergyMap energy = test_support::make_energy({
        {1, 4, 3, 5},
        {3, 2, 5, 2},
        {5, 2, 4, 2},
        {1, 6, 1, 3},
    });
    const seam::CumulativeTable table = seam::build_cumulative_energy(energy);

    // Row 0 is the energy itself.
    CHECK(table.cumulative(0, 0) == 1);
    CHECK(table.cumulative(0, 3) == 5);
    // Row 1: 3+min(1,4)=4, 2+min(1,4,3)=3, 5+min(4,3,5)=8, 2+min(3,5)=5
    CHECK(table.cumulative(1, 0) == 4);
    CHECK(table.cumulative(1, 1) == 3);
    CHECK(table.cumulative(1, 2) == 8);
    CHECK(table.cumulative(1, 3) == 5);
    // Row 2: 5+min(4,3)=8, 2+min(4,3,8)=5, 4+min(3,8,5)=7, 2+min(8,5)=7
    CHECK(table.cumulative(2, 0) == 8);
    CHECK(table.cumulative(2, 1) == 5);
    CHECK(table.cumulative(2, 2) == 7);
    CHECK(table.cumulative(2, 3) == 7);
    // Row 3: 1+min(8,5)=6, 6+min(8,5,7)=11, 1+min(5,7,7)=6, 3+min(7,7)=10
    CHECK(table.cumulative(3, 0) == 6);
    CHECK(table.cumulative(3, 1) == 11);
    CHECK(table.cumulative(3, 2) == 6);
    CHECK(table.cumulative(3, 3) == 10);

    CHECK(table.parent_col(0, 2) == -1);
    CHECK(table.parent_col(1, 1) == 0);
    CHECK(table.parent_col(3, 0) == 1);

    // Bottom row minimum is 6 at columns 0 and 2: the leftmost (column 0) wins.
    const Seam seam = seam::backtrack_seam(table);
    CHECK(seam == Seam{0, 1, 1, 0});
    CHECK(seam::seam_cost(energy, seam) == 6);
}

TEST_CASE("dp: seam cost equals the brute-force minimum on random grids", "[dp]") {
    // Every size from 1x1 to 6x6, several random grids each: 36 sizes * 15 seeds.
    for (int rows = 1; rows <= 6; ++rows) {
        for (int cols = 1; cols <= 6; ++cols) {
            for (unsigned seed = 0; seed < 15; ++seed) {
                const EnergyMap energy = test_support::random_energy(rows, cols, 9, seed);
                const Seam seam = seam::find_vertical_seam_dp(energy);
                INFO("size " << rows << " x " << cols << ", seed " << seed);
                REQUIRE(seam::is_valid_seam(seam, rows, cols));
                CHECK(seam::seam_cost(energy, seam) == test_support::brute_force_min_cost(energy));
            }
        }
    }
}

TEST_CASE("dp: bottom-row minimum of the table is the optimal cost", "[dp]") {
    const EnergyMap energy = test_support::random_energy(6, 5, 20, 7);
    const seam::CumulativeTable table = seam::build_cumulative_energy(energy);
    int smallest = std::numeric_limits<int>::max();
    for (int col = 0; col < energy.cols; ++col) {
        smallest = std::min(smallest, table.cumulative(energy.rows - 1, col));
    }
    CHECK(smallest == test_support::brute_force_min_cost(energy));
}

TEST_CASE("dp: an obvious cheap column is found", "[dp]") {
    EnergyMap energy(8, 9, 100);
    energy.col(6).setTo(1);
    CHECK(seam::find_vertical_seam_dp(energy) == Seam(8, 6));
}

TEST_CASE("dp: a cheap diagonal is followed", "[dp]") {
    EnergyMap energy(5, 5, 50);
    for (int index = 0; index < 5; ++index) {
        energy(index, index) = 0;
    }
    CHECK(seam::find_vertical_seam_dp(energy) == Seam{0, 1, 2, 3, 4});
}

TEST_CASE("dp: ties are broken deterministically (straight up, leftmost end)", "[dp]") {
    const EnergyMap energy(4, 5, 7);  // all equal: every seam costs 28
    const Seam seam = seam::find_vertical_seam_dp(energy);
    CHECK(seam == Seam(4, 0));
}

TEST_CASE("dp: rolling-row variant returns exactly the same seam", "[dp][improvement]") {
    for (unsigned seed = 0; seed < 200; ++seed) {
        const int rows = 1 + static_cast<int>(seed % 12);
        const int cols = 1 + static_cast<int>((seed * 5) % 13);
        // A small value range (0..3) creates many ties, the hardest case for agreement.
        const EnergyMap energy = test_support::random_energy(rows, cols, 3, seed);
        INFO("seed " << seed << ", size " << rows << " x " << cols);
        CHECK(seam::find_vertical_seam_dp_rolling(energy) == seam::find_vertical_seam_dp(energy));
    }
}

TEST_CASE("dp: forward-energy DP equals brute force over all seams", "[dp][forward]") {
    for (int rows = 1; rows <= 5; ++rows) {
        for (int cols = 1; cols <= 5; ++cols) {
            for (unsigned seed = 0; seed < 10; ++seed) {
                const cv::Mat_<uchar> gray = test_support::random_gray(rows, cols, seed);
                const Seam seam = seam::find_vertical_seam_dp_forward(gray);
                REQUIRE(seam::is_valid_seam(seam, rows, cols));

                long long best = std::numeric_limits<long long>::max();
                for (const Seam& candidate : test_support::all_seams(rows, cols)) {
                    best = std::min(best, seam::forward_seam_cost(gray, candidate));
                }
                INFO("size " << rows << " x " << cols << ", seed " << seed);
                CHECK(seam::forward_seam_cost(gray, seam) == best);
            }
        }
    }
}

TEST_CASE("dp: forward step cost matches the definition", "[dp][forward]") {
    cv::Mat_<uchar> gray(2, 3);
    gray << 10, 20, 40, 100, 50, 5;
    // Pixel (1, 1): left = 100, right = 5, above = 20.
    CHECK(seam::forward_step_cost(gray, 1, 1, 1) == 95);               // |5 - 100|
    CHECK(seam::forward_step_cost(gray, 1, 1, 0) == 95 + (100 - 20));  // + |above - left|
    CHECK(seam::forward_step_cost(gray, 1, 1, 2) == 95 + (20 - 5));    // + |above - right|
    // Top row has no parent: only the horizontal term.
    CHECK(seam::forward_step_cost(gray, 0, 1, 1) == 30);  // |40 - 10|
}

TEST_CASE("dp: empty input is rejected", "[dp]") {
    CHECK_THROWS_AS(seam::find_vertical_seam_dp(EnergyMap()), std::invalid_argument);
    CHECK_THROWS_AS(seam::find_vertical_seam_dp_rolling(EnergyMap()), std::invalid_argument);
    CHECK_THROWS_AS(seam::find_vertical_seam_dp_forward(cv::Mat_<uchar>()), std::invalid_argument);
}
