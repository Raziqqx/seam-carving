#include "seam/greedy.hpp"

#include <stdexcept>

namespace seam {

namespace {

// Column of the smallest energy in the top row (leftmost on ties).
int cheapest_col_in_top_row(const EnergyMap& energy) {
    int best_col = 0;
    for (int col = 1; col < energy.cols; ++col) {
        if (energy(0, col) < energy(0, best_col)) {
            best_col = col;
        }
    }
    return best_col;
}

// Given the seam is at column `col` in the row above, returns the cheapest of the
// neighbouring columns col-1, col, col+1 in `row`.
// Tie order: straight down, then down-left, then down-right.
int cheapest_col_below(const EnergyMap& energy, int row, int col) {
    int best_col = col;  // straight down always exists
    if (col - 1 >= 0 && energy(row, col - 1) < energy(row, best_col)) {
        best_col = col - 1;
    }
    if (col + 1 < energy.cols && energy(row, col + 1) < energy(row, best_col)) {
        best_col = col + 1;
    }
    return best_col;
}

}  // namespace

// Greedy vertical seam: cheapest top pixel, then the cheapest neighbour below, row by row.
// Time O(cols + rows), space O(rows). Valid but not always optimal.
Seam find_vertical_seam_greedy(const EnergyMap& energy) {
    if (energy.empty()) {
        throw std::invalid_argument("find_vertical_seam_greedy: input is empty");
    }

    Seam seam(energy.rows);
    seam[0] = cheapest_col_in_top_row(energy);

    // Each step commits to the locally cheapest pixel and never reconsiders it.
    // That is what makes it greedy, and also why it can walk into an expensive region.
    for (int row = 1; row < energy.rows; ++row) {
        seam[row] = cheapest_col_below(energy, row, seam[row - 1]);
    }
    return seam;
}

}  // namespace seam
