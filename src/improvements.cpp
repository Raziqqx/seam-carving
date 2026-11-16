// improvements.cpp - PLACEHOLDER.
//
// The optimised variants for spec item 5d (OpenMP energy map, row-parallel DP table,
// incremental energy update) are implemented in
// milestone 6 (owner: Kimberly Charlotte Madlangbayan Ambrosio, branch m6-analysis-report).
// Until that milestone is merged, every function below only reports that it is not there
// yet.
#include "seam/improvements.hpp"

namespace seam {

bool openmp_enabled() {
    throw NotImplementedYet(
        "item 5d optimised variants (milestone 6, Kimberly Charlotte Madlangbayan Ambrosio)");
}

EnergyMap compute_gradient_energy_parallel(const cv::Mat_<uchar>&, int) {
    throw NotImplementedYet(
        "item 5d optimised variants (milestone 6, Kimberly Charlotte Madlangbayan Ambrosio)");
}

CumulativeTable build_cumulative_energy_parallel(const EnergyMap&, int) {
    throw NotImplementedYet(
        "item 5d optimised variants (milestone 6, Kimberly Charlotte Madlangbayan Ambrosio)");
}

EnergyMap update_gradient_energy_after_removal(const EnergyMap&, const cv::Mat_<uchar>&,
                                               const Seam&) {
    throw NotImplementedYet(
        "item 5d optimised variants (milestone 6, Kimberly Charlotte Madlangbayan Ambrosio)");
}

}  // namespace seam
