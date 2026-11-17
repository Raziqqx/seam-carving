// greedy.cpp - Greedy seam finder (spec item 3).
//
// A "seam" is one pixel per row, where each pixel is at most one column away from the pixel
// above it. The cost of a seam is the sum of the energies of its pixels; a cheap seam is a good
// place to remove a pixel from. This file finds a seam quickly by always taking the cheapest
// choice available right now, without looking ahead. That is the "greedy" idea.
#include "seam/greedy.hpp"

#include <stdexcept>

namespace seam {

namespace {

// Finds the first seam pixel: the column with the smallest energy in row 0.
// If several columns share the smallest energy, the leftmost one is returned.
//
// Why leftmost: a fixed, predictable rule means the same input always gives the same seam,
// which makes the tests reproducible.
//
// Time O(cols): we look at each top-row pixel once.
// Space O(1): only the best column index is kept.
int cheapest_col_in_top_row(const EnergyMap& energy) {
    int best_col = 0;  // start by assuming the leftmost column is the cheapest
    for (int col = 1; col < energy.cols; ++col) {
        // Strict "<" means a later column only replaces the current best if it is strictly
        // cheaper, so ties keep the earlier (leftmost) column.
        if (energy(0, col) < energy(0, best_col)) {
            best_col = col;
        }
    }
    return best_col;
}

// Given that the seam is at column `col` in the row above, picks the column in `row` that the
// seam should move to: the cheapest of col-1, col and col+1 (only those that exist).
//
// Tie order: straight down, then down-left, then down-right. Straight down is checked first and
// each neighbour must be strictly cheaper to replace it, so ties keep the current column. This
// matters: a tie between "stay" and "move" always means "stay", which keeps the seam from
// wandering when nothing is gained by moving.
//
// Valid seam: the returned column is always within one of `col`, so the seam never jumps.
//
// Time O(1): at most three energy lookups.
// Space O(1).
int cheapest_col_below(const EnergyMap& energy, int row, int col) {
    int best_col = col;  // straight down always exists, so it is the default choice
    // The left neighbour only exists if col is not already the first column.
    if (col - 1 >= 0 && energy(row, col - 1) < energy(row, best_col)) {
        best_col = col - 1;
    }
    // The right neighbour only exists if col is not already the last column. It is compared
    // with the best so far, so if left already won, right must beat left to take over.
    if (col + 1 < energy.cols && energy(row, col + 1) < energy(row, best_col)) {
        best_col = col + 1;
    }
    return best_col;
}

}  // namespace

// Greedy vertical seam: cheapest top pixel, then the cheapest neighbour below, row by row.
// Time O(cols + rows), space O(rows). Valid but not always optimal.
//
// Worked trace on the 3x3 counter-example (energies row by row: 1 2 9 / 9 9 1 / 9 9 1):
//   row 0: cheapest top pixel is column 0 (energy 1), so seam = {0}.
//   row 1: below column 0 we see 9 (straight) and 9 (right); no strict improvement,
//          so stay: seam = {0, 0}.
//   row 2: same tie again, stay in column 0: seam = {0, 0, 0}, cost 1 + 9 + 9 = 19 (DP finds 4).
Seam find_vertical_seam_greedy(const EnergyMap& energy) {
    // An empty image has no pixels to build a seam from, so we refuse it explicitly rather than
    // return an empty or invalid seam.
    if (energy.empty()) {
        throw std::invalid_argument("find_vertical_seam_greedy: input is empty");
    }

    // One entry per row: seam[r] is the column the seam passes through in row r.
    Seam seam(energy.rows);
    seam[0] = cheapest_col_in_top_row(energy);

    // Each step commits to the locally cheapest pixel and never reconsiders it.
    // That is what makes it greedy, and also why it can walk into an expensive region.
    // Each row only looks at the row above's choice, so no table of earlier results is kept.
    for (int row = 1; row < energy.rows; ++row) {
        seam[row] = cheapest_col_below(energy, row, seam[row - 1]);
    }
    return seam;
}

}  // namespace seam
