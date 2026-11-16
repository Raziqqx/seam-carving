// Tests that every optimised variant (spec item 5d) agrees exactly with its plain version.
#include <catch2/catch_test_macros.hpp>

#include "seam/carve.hpp"
#include "seam/dp.hpp"
#include "seam/energy.hpp"
#include "seam/improvements.hpp"
#include "test_support.hpp"

using seam::EnergyMap;
using seam::Seam;

namespace {

bool same_matrix(const cv::Mat_<int>& a, const cv::Mat_<int>& b) {
    return a.size() == b.size() && cv::countNonZero(a != b) == 0;
}

}  // namespace

TEST_CASE("improvement: parallel energy == serial energy", "[improvement]") {
    for (int threads : {1, 2, 4, 8}) {
        for (unsigned seed = 0; seed < 10; ++seed) {
            const int rows = 1 + static_cast<int>(seed * 7 % 40);
            const int cols = 1 + static_cast<int>(seed * 11 % 50);
            const cv::Mat_<uchar> gray = test_support::random_gray(rows, cols, seed);
            INFO("threads " << threads << ", seed " << seed);
            CHECK(same_matrix(seam::compute_gradient_energy_parallel(gray, threads),
                              seam::compute_gradient_energy(gray)));
        }
    }
}

TEST_CASE("improvement: row-parallel DP table == plain DP table", "[improvement]") {
    for (int threads : {1, 3, 8}) {
        for (unsigned seed = 0; seed < 10; ++seed) {
            const int rows = 1 + static_cast<int>(seed * 5 % 30);
            const int cols = 1 + static_cast<int>(seed * 13 % 60);
            // Values 0..3 give many ties: the tie-breaking order must match too.
            const EnergyMap energy = test_support::random_energy(rows, cols, 3, seed);
            const seam::CumulativeTable plain = seam::build_cumulative_energy(energy);
            const seam::CumulativeTable parallel =
                seam::build_cumulative_energy_parallel(energy, threads);
            INFO("threads " << threads << ", seed " << seed);
            CHECK(same_matrix(plain.cumulative, parallel.cumulative));
            CHECK(same_matrix(plain.parent_col, parallel.parent_col));
            CHECK(seam::backtrack_seam(plain) == seam::backtrack_seam(parallel));
        }
    }
}

TEST_CASE("improvement: incremental energy update == full recomputation", "[improvement]") {
    for (unsigned seed = 0; seed < 30; ++seed) {
        const int rows = 1 + static_cast<int>(seed % 12);
        const int cols = 2 + static_cast<int>(seed * 3 % 14);
        cv::Mat_<uchar> gray = test_support::random_gray(rows, cols, seed);
        EnergyMap energy = seam::compute_gradient_energy(gray);

        // Remove seams one after another, updating the energy incrementally each time.
        while (gray.cols > 1) {
            const Seam seam = seam::find_vertical_seam_dp(energy);
            gray = cv::Mat_<uchar>(seam::remove_vertical_seam(gray, seam));
            energy = seam::update_gradient_energy_after_removal(energy, gray, seam);
            INFO("seed " << seed << ", " << rows << " x " << gray.cols << " left");
            REQUIRE(same_matrix(energy, seam::compute_gradient_energy(gray)));
        }
    }
}

TEST_CASE("improvement: incremental update handles seams on the image border", "[improvement]") {
    const cv::Mat_<uchar> gray = test_support::random_gray(6, 7, 5);
    const EnergyMap energy = seam::compute_gradient_energy(gray);
    for (const Seam& seam :
         {Seam(6, 0), Seam(6, 6), Seam{0, 1, 0, 1, 0, 1}, Seam{6, 5, 6, 5, 6, 5}}) {
        const cv::Mat_<uchar> smaller(seam::remove_vertical_seam(gray, seam));
        CHECK(same_matrix(seam::update_gradient_energy_after_removal(energy, smaller, seam),
                          seam::compute_gradient_energy(smaller)));
    }
}
