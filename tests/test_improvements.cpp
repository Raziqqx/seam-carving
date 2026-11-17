// Tests that every optimised variant (spec item 5d) agrees exactly with its plain version.
#include <catch2/catch_test_macros.hpp>
#include <vector>

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

namespace {

// Every valid vertical seam of an image with `rows` rows and `cols` columns: one column per
// row, and neighbouring rows at most one column apart. There are at most cols * 3^(rows-1)
// of them (item 2a), so only call this for tiny images.
std::vector<Seam> all_vertical_seams(int rows, int cols) {
    std::vector<Seam> seams;
    for (int col = 0; col < cols; ++col) {
        seams.push_back(Seam{col});
    }
    for (int row = 1; row < rows; ++row) {
        std::vector<Seam> longer;
        for (const Seam& seam : seams) {
            for (int step = -1; step <= 1; ++step) {
                const int col = seam.back() + step;
                if (col >= 0 && col < cols) {
                    Seam extended = seam;
                    extended.push_back(col);
                    longer.push_back(extended);
                }
            }
        }
        seams = longer;
    }
    return seams;
}

// True if the incremental update after removing `seam` from `gray` gives the same energy
// map as computing it from scratch on the smaller image.
bool incremental_update_is_exact(const cv::Mat_<uchar>& gray, const Seam& seam) {
    const cv::Mat_<uchar> smaller(seam::remove_vertical_seam(gray, seam));
    return same_matrix(seam::update_gradient_energy_after_removal(
                           seam::compute_gradient_energy(gray), smaller, seam),
                       seam::compute_gradient_energy(smaller));
}

}  // namespace

// A 1-row image has no row above or below, so every Sobel window is clamped vertically,
// and the band seam-2 .. seam+1 is clipped at the left border (seam at 0 or 1) and at the
// right border (seam at one of the last two columns).
// These two tests check the CLIPPING of the band at the borders. They cannot check its
// width: with 1 row (or 1 column left) a narrower band would also be enough, because the
// extra columns are only needed when the seam moves sideways between rows. The width is
// checked by the multi-row tests above.
TEST_CASE("improvement: incremental update on a 1-row image, every seam", "[improvement]") {
    for (unsigned seed = 0; seed < 5; ++seed) {
        const cv::Mat_<uchar> gray = test_support::random_gray(1, 7, seed);
        for (const Seam& seam : all_vertical_seams(1, 7)) {
            INFO("seed " << seed << ", seam at column " << seam[0]);
            CHECK(incremental_update_is_exact(gray, seam));
        }
    }
}

// A 2-column image leaves a single column, so the band is clipped on BOTH sides at once:
// only column 0 exists and it must be recomputed in every row (its old right neighbour
// was either removed or shifted onto it).
TEST_CASE("improvement: incremental update on a 2-column image, every seam", "[improvement]") {
    const int rows = 5;
    for (unsigned seed = 0; seed < 5; ++seed) {
        const cv::Mat_<uchar> gray = test_support::random_gray(rows, 2, seed);
        const std::vector<Seam> seams = all_vertical_seams(rows, 2);
        REQUIRE(seams.size() == 32);  // with 2 columns every row may pick either: 2^5
        for (const Seam& seam : seams) {
            INFO("seed " << seed);
            CHECK(incremental_update_is_exact(gray, seam));
        }
    }
}
