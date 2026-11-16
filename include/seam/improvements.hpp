// improvements.hpp - optimised variants for spec item 5d ("any improvement?").
//
// Every function here has a plain counterpart (energy.cpp / dp.cpp) that returns exactly
// the same result; tests/test_improvements.cpp checks that they agree. The plain versions
// stay the default so the main code path is the easy-to-read one.
//
//   * compute_gradient_energy_parallel  - multithreaded energy map (OpenMP)
//   * build_cumulative_energy_parallel  - DP table with each ROW filled by several threads
//   * update_gradient_energy_after_removal - recompute only the pixels near the seam
// (The memory-optimised rolling-row DP lives next to the plain DP in dp.cpp.)
#pragma once

#include "seam/dp.hpp"
#include "seam/types.hpp"

namespace seam {

// True if the library was compiled with OpenMP. Without it the "parallel" functions still
// work but run on one thread.
bool openmp_enabled();

// Sobel energy map computed with `thread_count` threads.
// Why it parallelises perfectly: the energy of a pixel depends only on the INPUT image,
// never on another pixel's energy, so rows can be computed in any order by any thread.
// Time O(rows * cols / threads) ideally, space O(rows * cols).
EnergyMap compute_gradient_energy_parallel(const cv::Mat_<uchar>& gray, int thread_count);

// Cumulative-energy table with the cells of one row shared among `thread_count` threads.
// Why only within a row: row i needs the finished row i-1 (a loop-carried dependency),
// so the rows themselves must still be processed one after another.
// Time O(rows * cols / threads) ideally, but every row pays a thread start/stop cost,
// which is why this is slower than the plain loop for small images.
CumulativeTable build_cumulative_energy_parallel(const EnergyMap& energy, int thread_count);

// Incremental energy update. Given the energy map of the image BEFORE a vertical seam was
// removed, and the gray image AFTER the removal, returns the energy map of the new image
// while recomputing only the pixels whose 3x3 neighbourhood changed.
// Why this is enough: a Sobel value depends on a 3x3 window. Pixels more than 2 columns
// left of the seam keep their window unchanged; pixels more than 1 column right of it
// keep their window too, just shifted one column left. So per row only the 4 columns
// seam[row]-2 .. seam[row]+1 of the new image need new values.
// Time O(rows * cols) for the shifted copy but only O(rows) Sobel evaluations instead
// of O(rows * cols).
EnergyMap update_gradient_energy_after_removal(const EnergyMap& old_energy,
                                               const cv::Mat_<uchar>& new_gray, const Seam& seam);

}  // namespace seam
