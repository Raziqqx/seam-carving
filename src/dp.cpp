#include "seam/dp.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include "seam/energy.hpp"

namespace seam {

namespace {

// Throws if the energy map has no pixels: there is no seam to find in an empty image.
void require_non_empty(const cv::Mat& matrix, const char* function_name) {
    if (matrix.empty()) {
        throw std::invalid_argument(std::string(function_name) + ": input is empty");
    }
}

// Returns the column of the smallest value in `values` (the leftmost one on ties).
int index_of_minimum(const std::vector<int>& values) {
    int best_col = 0;
    for (int col = 1; col < static_cast<int>(values.size()); ++col) {
        if (values[col] < values[best_col]) {
            best_col = col;
        }
    }
    return best_col;
}

}  // namespace

// Copies one row of an integer matrix into a std::vector. Time O(cols).
std::vector<int> row_as_vector(const cv::Mat_<int>& matrix, int row) {
    std::vector<int> values(matrix.cols);
    for (int col = 0; col < matrix.cols; ++col) {
        values[col] = matrix(row, col);
    }
    return values;
}

// Column of the cheapest of the (up to three) cells above column `col`; see dp.hpp.
// Tie order: straight up, then up-left, then up-right (strict '<' keeps the earlier one).
// Time O(1), space O(1).
int best_parent_col(const std::vector<int>& previous_row_costs, int col) {
    const int cols = static_cast<int>(previous_row_costs.size());
    int best_col = col;  // straight up always exists
    if (col - 1 >= 0 && previous_row_costs[col - 1] < previous_row_costs[best_col]) {
        best_col = col - 1;  // up-left exists only when we are not in the first column
    }
    if (col + 1 < cols && previous_row_costs[col + 1] < previous_row_costs[best_col]) {
        best_col = col + 1;  // up-right exists only when we are not in the last column
    }
    return best_col;
}

// Fills the cumulative-energy table M and the parent table, top row first.
// Time O(rows * cols), space O(rows * cols).
CumulativeTable build_cumulative_energy(const EnergyMap& energy) {
    require_non_empty(energy, "build_cumulative_energy");
    const int rows = energy.rows;
    const int cols = energy.cols;

    CumulativeTable table;
    table.cumulative = cv::Mat_<int>(rows, cols);
    table.parent_col = cv::Mat_<int>(rows, cols);

    // Base case: a seam that ends in the top row is just that one pixel.
    for (int col = 0; col < cols; ++col) {
        table.cumulative(0, col) = energy(0, col);
        table.parent_col(0, col) = -1;
    }

    // Rows must be filled top to bottom, because row i needs the finished row i-1.
    for (int row = 1; row < rows; ++row) {
        // A copy of the finished row above (see row_as_vector in dp.hpp for why a copy).
        const std::vector<int> previous_costs = row_as_vector(table.cumulative, row - 1);
        for (int col = 0; col < cols; ++col) {
            // Why the min over 3 parents: a seam reaching (row, col) must come from one of
            // the pixels directly above or diagonally above it. Whichever it comes from,
            // the part above must itself be a cheapest seam to that parent (optimal
            // substructure), so we only need the cheapest of the three parents.
            const int parent = best_parent_col(previous_costs, col);
            table.cumulative(row, col) = energy(row, col) + previous_costs[parent];
            table.parent_col(row, col) = parent;
        }
    }
    return table;
}

// Recovers the seam: start at the bottom-row minimum, follow the parents upwards.
// Time O(cols + rows), space O(rows).
Seam backtrack_seam(const CumulativeTable& table) {
    const int rows = table.cumulative.rows;
    Seam seam(rows);

    // The cheapest complete seam ends at the smallest value of the bottom row.
    int col = index_of_minimum(row_as_vector(table.cumulative, rows - 1));

    // Walk upwards. parent_col tells us which pixel of the row above the seam came from.
    for (int row = rows - 1; row >= 0; --row) {
        seam[row] = col;
        col = table.parent_col(row, col);
    }
    return seam;
}

// Optimal vertical seam: build the table, then backtrack.
// Time O(rows * cols), space O(rows * cols).
Seam find_vertical_seam_dp(const EnergyMap& energy) {
    const CumulativeTable table = build_cumulative_energy(energy);
    return backtrack_seam(table);
}

// Same seam as find_vertical_seam_dp with about 8x less table memory (item 5d).
// Time O(rows * cols), space 2 * cols ints + rows * cols bytes.
Seam find_vertical_seam_dp_rolling(const EnergyMap& energy) {
    require_non_empty(energy, "find_vertical_seam_dp_rolling");
    const int rows = energy.rows;
    const int cols = energy.cols;

    // Only two rows of costs are ever needed: row i depends on row i-1 and nothing older.
    std::vector<int> previous_row(cols);
    std::vector<int> current_row(cols);

    // We still need the parents of EVERY row to backtrack, but a parent is always one of
    // three neighbours, so one signed byte per pixel (-1 = up-left, 0 = up, +1 = up-right)
    // is enough. steps[row * cols + col] belongs to pixel (row, col).
    std::vector<signed char> steps(static_cast<size_t>(rows) * cols, 0);

    for (int col = 0; col < cols; ++col) {
        previous_row[col] = energy(0, col);
    }

    for (int row = 1; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const int parent = best_parent_col(previous_row, col);
            current_row[col] = energy(row, col) + previous_row[parent];
            steps[static_cast<size_t>(row) * cols + col] = static_cast<signed char>(parent - col);
        }
        // The row we just finished becomes "previous" for the next iteration.
        // Swapping two vectors only exchanges their internal pointers: O(1), no copying.
        std::swap(previous_row, current_row);
    }

    // After the loop, previous_row holds the cumulative costs of the bottom row.
    Seam seam(rows);
    int col = index_of_minimum(previous_row);
    for (int row = rows - 1; row >= 0; --row) {
        seam[row] = col;
        col += steps[static_cast<size_t>(row) * cols + col];
    }
    return seam;
}

// New-edge cost of stepping onto (row, col) from `parent_col` in the row above.
// Time O(1).
int forward_step_cost(const cv::Mat_<uchar>& gray, int row, int col, int parent_col) {
    const int left = clamp_index(col - 1, gray.cols);
    const int right = clamp_index(col + 1, gray.cols);

    // After pixel (row, col) is removed, its left and right neighbours become adjacent.
    // This new horizontal edge is paid for every step, whatever the parent is.
    int cost = std::abs(gray(row, right) - gray(row, left));

    if (row == 0 || parent_col == col) {
        return cost;  // straight step (or top row): no extra vertical edge is created
    }

    // A diagonal step also puts the pixel above (row-1, col) next to a new pixel of this
    // row: the left neighbour if we came from the up-left, the right one from the up-right.
    const int above = gray(row - 1, col);
    if (parent_col == col - 1) {
        cost += std::abs(above - gray(row, left));
    } else {
        cost += std::abs(above - gray(row, right));
    }
    return cost;
}

// Sum of the new-edge costs along a seam. Time O(rows).
long long forward_seam_cost(const cv::Mat_<uchar>& gray, const Seam& seam) {
    long long total = 0;
    for (int row = 0; row < gray.rows; ++row) {
        const int parent_col = (row == 0) ? seam[0] : seam[row - 1];
        total += forward_step_cost(gray, row, seam[row], parent_col);
    }
    return total;
}

// DP on forward energy: the step cost depends on which parent the seam comes from.
// Time O(rows * cols), space O(rows * cols).
Seam find_vertical_seam_dp_forward(const cv::Mat_<uchar>& gray) {
    require_non_empty(gray, "find_vertical_seam_dp_forward");
    const int rows = gray.rows;
    const int cols = gray.cols;

    CumulativeTable table;
    table.cumulative = cv::Mat_<int>(rows, cols);
    table.parent_col = cv::Mat_<int>(rows, cols);

    for (int col = 0; col < cols; ++col) {
        table.cumulative(0, col) = forward_step_cost(gray, 0, col, col);
        table.parent_col(0, col) = -1;
    }

    for (int row = 1; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            // Same DP as before, but here the cost of a cell depends on WHICH parent we
            // come from, so the step cost is added inside the minimum.
            // Tie order is the same: straight up, then up-left, then up-right.
            int best_parent = col;
            int best_cost = table.cumulative(row - 1, col) + forward_step_cost(gray, row, col, col);
            for (int parent : {col - 1, col + 1}) {
                if (parent < 0 || parent >= cols) {
                    continue;  // this parent is outside the image
                }
                const int cost =
                    table.cumulative(row - 1, parent) + forward_step_cost(gray, row, col, parent);
                if (cost < best_cost) {
                    best_cost = cost;
                    best_parent = parent;
                }
            }
            table.cumulative(row, col) = best_cost;
            table.parent_col(row, col) = best_parent;
        }
    }
    return backtrack_seam(table);
}

}  // namespace seam
