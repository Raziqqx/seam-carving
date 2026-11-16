// Tests for the greedy seam finder (spec item 3).
#include <catch2/catch_test_macros.hpp>

#include "seam/dp.hpp"
#include "seam/greedy.hpp"
#include "test_support.hpp"

using seam::EnergyMap;
using seam::Seam;

TEST_CASE("greedy: follows the locally cheapest pixel", "[greedy]") {
    const EnergyMap energy = test_support::make_energy({
        {5, 1, 9},
        {4, 6, 2},
        {9, 9, 3},
    });
    // Start at column 1 (energy 1). Below: 4, 6, 2 -> column 2. Below: 9, 3 -> column 2.
    CHECK(seam::find_vertical_seam_greedy(energy) == Seam{1, 2, 2});
}

TEST_CASE("greedy: hand-built grid where greedy is strictly worse than DP", "[greedy]") {
    // Greedy is lured by the 1 in the top row and then trapped among expensive pixels.
    const EnergyMap energy = test_support::make_energy({
        {1, 2, 9},
        {9, 9, 1},
        {9, 9, 1},
    });
    const Seam greedy = seam::find_vertical_seam_greedy(energy);
    const Seam optimal = seam::find_vertical_seam_dp(energy);

    CHECK(greedy == Seam{0, 0, 0});  // cost 1 + 9 + 9 = 19
    CHECK(seam::seam_cost(energy, greedy) == 19);
    CHECK(optimal == Seam{1, 2, 2});  // cost 2 + 1 + 1 = 4
    CHECK(seam::seam_cost(energy, optimal) == 4);
    CHECK(seam::seam_cost(energy, greedy) > seam::seam_cost(energy, optimal));
}

TEST_CASE("greedy: seam is valid and never cheaper than DP on random grids", "[greedy]") {
    int strictly_worse = 0;
    const int trials = 500;
    for (unsigned seed = 0; seed < static_cast<unsigned>(trials); ++seed) {
        const int rows = 1 + static_cast<int>(seed % 10);
        const int cols = 1 + static_cast<int>((seed * 3) % 10);
        const EnergyMap energy = test_support::random_energy(rows, cols, 50, seed);
        const Seam greedy = seam::find_vertical_seam_greedy(energy);
        const Seam optimal = seam::find_vertical_seam_dp(energy);

        INFO("seed " << seed << ", size " << rows << " x " << cols);
        REQUIRE(seam::is_valid_seam(greedy, rows, cols));
        CHECK(seam::seam_cost(energy, greedy) >= seam::seam_cost(energy, optimal));
        if (seam::seam_cost(energy, greedy) > seam::seam_cost(energy, optimal)) {
            ++strictly_worse;
        }
    }
    // Greedy must actually lose sometimes, otherwise this comparison would prove nothing.
    CHECK(strictly_worse > 0);
}

TEST_CASE("greedy: optimal on a single row and on a single column", "[greedy]") {
    const EnergyMap one_row = test_support::make_energy({{7, 3, 9, 3}});
    CHECK(seam::find_vertical_seam_greedy(one_row) == Seam{1});  // leftmost minimum

    const EnergyMap one_col = test_support::make_energy({{7}, {3}, {9}});
    CHECK(seam::find_vertical_seam_greedy(one_col) == Seam{0, 0, 0});
}

TEST_CASE("greedy: ties go straight down", "[greedy]") {
    const EnergyMap energy(6, 5, 4);  // constant
    CHECK(seam::find_vertical_seam_greedy(energy) == Seam(6, 0));

    EnergyMap cheap_start(4, 5, 4);
    cheap_start(0, 3) = 0;  // start in column 3, then all ties -> stay in column 3
    CHECK(seam::find_vertical_seam_greedy(cheap_start) == Seam(4, 3));
}

TEST_CASE("greedy: empty input is rejected", "[greedy]") {
    CHECK_THROWS_AS(seam::find_vertical_seam_greedy(EnergyMap()), std::invalid_argument);
}
