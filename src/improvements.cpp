#include "seam/improvements.hpp"

#include <algorithm>

#include "seam/carve.hpp"
#include "seam/energy.hpp"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace seam {

// True when compiled with OpenMP.
bool openmp_enabled() {
#ifdef _OPENMP
    return true;
#else
    return false;
#endif
}

// Sobel energy map with the rows shared among threads (item 5d).
// Time O(rows * cols / threads) at best, space O(rows * cols).
EnergyMap compute_gradient_energy_parallel(const cv::Mat_<uchar>& gray, int thread_count) {
    EnergyMap energy(gray.rows, gray.cols);
    // Each thread gets its own block of rows. No two threads ever write the same pixel
    // and the input is only read, so no locking is needed.
#pragma omp parallel for num_threads(thread_count) schedule(static)
    for (int row = 0; row < gray.rows; ++row) {
        for (int col = 0; col < gray.cols; ++col) {
            energy(row, col) = sobel_energy_at(gray, row, col);
        }
    }
    (void)thread_count;  // unused when compiled without OpenMP
    return energy;
}

// DP table with each row filled by several threads; rows still go one after another.
// Time O(rows * cols / threads) at best plus a thread start/stop cost per row.
CumulativeTable build_cumulative_energy_parallel(const EnergyMap& energy, int thread_count) {
    const int rows = energy.rows;
    const int cols = energy.cols;

    CumulativeTable table;
    table.cumulative = cv::Mat_<int>(rows, cols);
    table.parent_col = cv::Mat_<int>(rows, cols);

    for (int col = 0; col < cols; ++col) {
        table.cumulative(0, col) = energy(0, col);
        table.parent_col(0, col) = -1;
    }

    // The OUTER loop over rows must stay sequential: row i reads row i-1.
    for (int row = 1; row < rows; ++row) {
        const std::vector<int> previous_costs = row_as_vector(table.cumulative, row - 1);
        // The INNER loop is safe to split: every cell of this row reads only the previous
        // row (finished) and writes only its own cell.
#pragma omp parallel for num_threads(thread_count) schedule(static)
        for (int col = 0; col < cols; ++col) {
            // Exactly the same cell computation as the plain DP.
            const int parent = best_parent_col(previous_costs, col);
            table.cumulative(row, col) = energy(row, col) + previous_costs[parent];
            table.parent_col(row, col) = parent;
        }
    }
    (void)thread_count;  // unused when compiled without OpenMP
    return table;
}

// Energy map after a seam removal, recomputing only the 4 columns next to the seam.
// Time O(rows * cols) for the shifted copy but only 4 * rows Sobel evaluations.
EnergyMap update_gradient_energy_after_removal(const EnergyMap& old_energy,
                                               const cv::Mat_<uchar>& new_gray, const Seam& seam) {
    // Start from the old map with the seam removed: values left of the seam stay in
    // place, values right of it shift one column left, exactly like the image pixels.
    EnergyMap energy(remove_vertical_seam(old_energy, seam));

    // Recompute the narrow band of pixels whose neighbourhood contains the cut.
    for (int row = 0; row < energy.rows; ++row) {
        const int first_col = std::max(seam[row] - 2, 0);
        const int last_col = std::min(seam[row] + 1, energy.cols - 1);
        for (int col = first_col; col <= last_col; ++col) {
            energy(row, col) = sobel_energy_at(new_gray, row, col);
        }
    }
    return energy;
}

}  // namespace seam
