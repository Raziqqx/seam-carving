// dp.hpp - dynamic-programming seam finding (spec items 1b and 2a-ii).
//
// Recurrence (M = cumulative energy, e = energy):
//     M[0][j] = e[0][j]                                             (base case: top row)
//     M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])  (i >= 1)
// A parent that would fall outside the image (j-1 < 0 or j+1 >= cols) is simply skipped.
// M[i][j] is the cost of the cheapest seam that starts anywhere in the top row and ends
// at pixel (i, j). The cheapest full seam ends at the smallest entry of the bottom row.
//
// All functions find VERTICAL seams. Horizontal seams are found by transposing the image
// first (see carve.cpp).
#pragma once

#include "seam/types.hpp"

namespace seam {

// The two tables filled by the DP.
struct CumulativeTable {
    // cumulative(i, j): cost of the cheapest seam from the top row down to pixel (i, j).
    // Stored as int: fine while rows * (largest energy) < 2^31, e.g. Sobel energy
    // (at most 2040) allows about one million rows.
    cv::Mat_<int> cumulative;
    // parent_col(i, j): column in row i-1 that the cheapest seam to (i, j) comes from.
    // Row 0 has no parent and stores -1.
    cv::Mat_<int> parent_col;
};

// The "min over 3 parents" of the recurrence; every DP variant calls it.
// `previous_row_costs` holds the cumulative costs of the row above. Among the (up to
// three) cells that touch column `col` (up-left, up, up-right) it returns the column with
// the smallest cost. A neighbour outside the row is skipped.
// Ties: straight up first, then up-left, then up-right.
// Time O(1), space O(1).
int best_parent_col(const std::vector<int>& previous_row_costs, int col);

// Copies one row of an integer matrix into a std::vector. The DP reads the previous row
// from such a copy: the copy costs O(cols) per row, which does not change the O(rows *
// cols) total, and it tells the compiler that the row being read and the row being
// written are different memory, which lets it generate faster code for the inner loop.
std::vector<int> row_as_vector(const cv::Mat_<int>& matrix, int row);

// Fills the cumulative-energy table and the parent table, row by row from the top.
// Ties between parents are broken in a fixed order: straight up first, then up-left,
// then up-right.
// Time O(rows * cols): every cell looks at no more than 3 parents.
// Space O(rows * cols) for the two tables.
CumulativeTable build_cumulative_energy(const EnergyMap& energy);

// Recovers the optimal seam from a filled table: start at the smallest entry of the
// bottom row (leftmost on ties) and follow the parent pointers up to the top row.
// Time O(cols + rows), extra space O(rows) for the seam.
Seam backtrack_seam(const CumulativeTable& table);

// Minimum-energy vertical seam by dynamic programming (build table, then backtrack).
// Time O(rows * cols), space O(rows * cols).
Seam find_vertical_seam_dp(const EnergyMap& energy);

// Memory-optimised DP (spec item 5d). Returns exactly the same seam as
// find_vertical_seam_dp, but keeps only TWO rows of cumulative costs and stores each
// parent as a one-byte step (-1, 0 or +1) instead of a 4-byte column index.
// Time O(rows * cols). Space: 2 * cols integers + rows * cols bytes, about 8x less
// than the full tables (which use 8 bytes per pixel).
Seam find_vertical_seam_dp_rolling(const EnergyMap& energy);

// Forward-energy DP (Rubinstein, Shamir, Avidan 2008). Instead of the energy that a
// pixel HAS, it minimises the energy that removing the seam ADDS: the new edges between
// pixels that become neighbours after the removal. Works directly on the gray image.
// Time O(rows * cols), space O(rows * cols).
Seam find_vertical_seam_dp_forward(const cv::Mat_<uchar>& gray);

// Forward-energy cost of a given seam: the sum of the new-edge costs it creates.
// Used to report the cost of forward seams and to test the forward DP by brute force.
// Time O(rows), space O(1).
long long forward_seam_cost(const cv::Mat_<uchar>& gray, const Seam& seam);

// Forward-energy cost of stepping onto pixel (row, col) from column `parent_col` in the
// row above. For row 0 (no parent) pass parent_col = col.
// Time O(1).
int forward_step_cost(const cv::Mat_<uchar>& gray, int row, int col, int parent_col);

}  // namespace seam
