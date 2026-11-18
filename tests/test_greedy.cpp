// Tests for the greedy seam finder (spec item 3).
//
// Each test checks one specific behaviour of find_vertical_seam_greedy. Tests marked "DP" compare
// the greedy seam with the optimal seam from dynamic programming (seam/dp.hpp). The energy grids
// are written as rows, top to bottom: energy(row, col) is the number in row `row`, column `col`.
#include <catch2/catch_test_macros.hpp>

#include "seam/dp.hpp"
#include "seam/greedy.hpp"
#include "test_support.hpp"

using seam::EnergyMap;
using seam::Seam;

TEST_CASE("greedy: follows the locally cheapest pixel", "[greedy]") {
    // Shows the basic rule: at each row, move to the cheapest neighbour.
    const EnergyMap energy = test_support::make_energy({
        {5, 1, 9},
        {4, 6, 2},
        {9, 9, 3},
    });
    // Start at column 1 (energy 1). Below: 4, 6, 2 -> column 2. Below: 9, 3 -> column 2.
    CHECK(seam::find_vertical_seam_greedy(energy) == Seam{1, 2, 2});
}

TEST_CASE("greedy: hand-built grid where greedy is strictly worse than DP", "[greedy]") {
    // The 3x3 counter-example: greedy is not optimal.
    // Greedy is lured by the 1 in the top row and then trapped among expensive pixels.
    // Greedy's seam costs 19, DP's costs 4, so greedy is worse by 15.
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
    // Runs 500 random grids of varying sizes (1 to 10 rows and columns).
    // Checks two things: the seam is always valid, and its cost is never below DP's cost
    // (DP is optimal, so greedy cannot beat it).
    int strictly_worse = 0;
    const int trials = 500;
    for (unsigned seed = 0; seed < static_cast<unsigned>(trials); ++seed) {
        // The sizes are derived from the seed so every run covers the same set of grids.
        const int rows = 1 + static_cast<int>(seed % 10);
        const int cols = 1 + static_cast<int>((seed * 3) % 10);
        const EnergyMap energy = test_support::random_energy(rows, cols, 50, seed);
        const Seam greedy = seam::find_vertical_seam_greedy(energy);
        const Seam optimal = seam::find_vertical_seam_dp(energy);

        // INFO prints the seed and size only if this trial fails, so the failure is reproducible.
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
    // Edge cases with only one row or one column, where there is no choice to make.
    // One row: the seam is just the cheapest pixel, so greedy must pick the leftmost minimum.
    const EnergyMap one_row = test_support::make_energy({{7, 3, 9, 3}});
    CHECK(seam::find_vertical_seam_greedy(one_row) == Seam{1});  // leftmost minimum

    // One column: the only possible seam goes straight down the single column.
    const EnergyMap one_col = test_support::make_energy({{7}, {3}, {9}});
    CHECK(seam::find_vertical_seam_greedy(one_col) == Seam{0, 0, 0});
}

TEST_CASE("greedy: ties go straight down", "[greedy]") {
    // Checks the tie-break rule: when moving and staying cost the same, the seam stays put.
    // A constant grid makes every choice a tie, so the seam should stay in column 0 (the
    // leftmost top pixel).
    const EnergyMap energy(6, 5, 4);  // constant
    CHECK(seam::find_vertical_seam_greedy(energy) == Seam(6, 0));

    // Here one pixel in row 0 is cheaper, so the seam starts at column 3. Everything else is a
    // tie, so it stays in column 3 for every row.
    EnergyMap cheap_start(4, 5, 4);
    cheap_start(0, 3) = 0;  // start in column 3, then all ties -> stay in column 3
    CHECK(seam::find_vertical_seam_greedy(cheap_start) == Seam(4, 3));
}

TEST_CASE("greedy: optimal when a straight zero-energy column exists", "[greedy]") {
    // Greedy matches DP for this class of input: if a whole column has zero energy, the
    // cheapest seam is that column (cost 0), and greedy finds it PROVIDED it starts at the
    // top of that column, i.e. that column holds the leftmost minimum of the top row
    // (greedy starts at the leftmost minimum; another zero further left would start it
    // elsewhere).
    // Column 1 is all zeros, the rest are 5. Greedy starts there and every step is a tie
    // that goes straight down, so it stays in column 1 and matches DP's zero-cost seam.
    EnergyMap energy(4, 3, 5);
    for (int row = 0; row < 4; ++row) {
        energy(row, 1) = 0;
    }
    const Seam greedy = seam::find_vertical_seam_greedy(energy);
    const Seam optimal = seam::find_vertical_seam_dp(energy);
    CHECK(greedy == Seam(4, 1));
    CHECK(seam::seam_cost(energy, greedy) == seam::seam_cost(energy, optimal));
}

TEST_CASE("greedy: matches DP on a 2-column image", "[greedy]") {
    // A small hand-checked case with only two columns, where greedy happens to find the optimum.
    // Hand-checked: DP's best seam is {1, 0, 1} with cost 1 + 2 + 0 = 3.
    // Greedy: start in column 1 (energy 1), move left to column 0 (2 beats 4),
    // then move right to column 1 (0 beats 5).
    const EnergyMap energy = test_support::make_energy({
        {3, 1},
        {2, 4},
        {5, 0},
    });
    const Seam greedy = seam::find_vertical_seam_greedy(energy);
    CHECK(greedy == Seam{1, 0, 1});
    CHECK(seam::seam_cost(energy, greedy) == 3);
    CHECK(seam::seam_cost(energy, seam::find_vertical_seam_dp(energy)) == 3);
}

TEST_CASE("greedy: how often it is optimal on random grids", "[greedy]") {
    // Measures how often greedy matches DP on random square grids of three sizes.
    // This is a measurement, not a strict check, the bounds are loose and only
    // require that greedy is sometimes optimal and sometimes not.
    const int trials = 200;
    const int sizes[] = {4, 6, 10};
    for (int size : sizes) {
        int optimal_count = 0;
        for (unsigned seed = 0; seed < static_cast<unsigned>(trials); ++seed) {
            const EnergyMap energy = test_support::random_energy(size, size, 50, seed);
            const Seam greedy = seam::find_vertical_seam_greedy(energy);
            const Seam optimal = seam::find_vertical_seam_dp(energy);
            // Compare costs rather than seams: two different seams can have the same cost.
            if (seam::seam_cost(energy, greedy) == seam::seam_cost(energy, optimal)) {
                ++optimal_count;
            }
        }
        INFO("grid " << size << " x " << size << ": optimal in " << optimal_count << " of "
                     << trials);
        CHECK(optimal_count > 0);
        CHECK(optimal_count < trials);
    }
}

TEST_CASE("greedy: empty input is rejected", "[greedy]") {
    // An empty image must throw rather than return a meaningless seam.
    CHECK_THROWS_AS(seam::find_vertical_seam_greedy(EnergyMap()), std::invalid_argument);
}
