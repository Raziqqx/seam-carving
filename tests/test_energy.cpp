// Tests for the energy maps (spec item 1a).
#include <catch2/catch_test_macros.hpp>

#include "seam/energy.hpp"
#include "test_support.hpp"

using seam::EnergyMap;

namespace {

// True if two integer matrices have the same size and the same values.
bool same_matrix(const cv::Mat_<int>& a, const cv::Mat_<int>& b) {
    return a.size() == b.size() && cv::countNonZero(a != b) == 0;
}

}  // namespace

TEST_CASE("energy: a constant image has zero energy everywhere", "[energy]") {
    const cv::Mat_<uchar> gray(5, 7, static_cast<uchar>(123));
    CHECK(cv::countNonZero(seam::compute_gradient_energy(gray)) == 0);
    CHECK(cv::countNonZero(seam::compute_gradient_energy_manual(gray)) == 0);
    CHECK(cv::countNonZero(seam::compute_neighbour_difference_energy(gray)) == 0);
}

TEST_CASE("energy: a vertical step edge has high energy only next to the edge", "[energy]") {
    // Left half black (0), right half white (255): columns 0..2 = 0, columns 3..5 = 255.
    cv::Mat_<uchar> gray(4, 6, static_cast<uchar>(0));
    gray.colRange(3, 6).setTo(255);

    const EnergyMap energy = seam::compute_gradient_energy(gray);
    for (int row = 0; row < gray.rows; ++row) {
        CHECK(energy(row, 0) == 0);
        CHECK(energy(row, 1) == 0);
        // Sobel X on a full step: (1 + 2 + 1) * 255 = 1020, and Gy = 0.
        CHECK(energy(row, 2) == 1020);
        CHECK(energy(row, 3) == 1020);
        CHECK(energy(row, 4) == 0);
        CHECK(energy(row, 5) == 0);
    }
}

TEST_CASE("energy: hand-computed Sobel value in the middle of a 3x3 image", "[energy]") {
    cv::Mat_<uchar> gray(3, 3);
    gray << 10, 20, 30, 40, 50, 60, 70, 80, 100;
    // Gx = (30 + 2*60 + 100) - (10 + 2*40 + 70) = 250 - 160 = 90
    // Gy = (70 + 2*80 + 100) - (10 + 2*20 + 30) = 330 - 80 = 250
    CHECK(seam::sobel_energy_at(gray, 1, 1) == 90 + 250);
    CHECK(seam::compute_gradient_energy(gray)(1, 1) == 340);
}

TEST_CASE("energy: borders replicate the edge pixel", "[energy]") {
    cv::Mat_<uchar> gray(1, 3);
    gray << 10, 50, 200;
    // One row only: every "row above/below" is the row itself, so Gy = 0 and
    // Gx = 4 * (right - left) with clamped neighbours.
    CHECK(seam::sobel_energy_at(gray, 0, 0) == 4 * (50 - 10));
    CHECK(seam::sobel_energy_at(gray, 0, 1) == 4 * (200 - 10));
    CHECK(seam::sobel_energy_at(gray, 0, 2) == 4 * (200 - 50));

    const EnergyMap neighbour = seam::compute_neighbour_difference_energy(gray);
    CHECK(neighbour(0, 0) == 40);   // |I[0,1] - I[0,0]|  (left neighbour clamped)
    CHECK(neighbour(0, 1) == 190);  // |I[0,2] - I[0,0]|
    CHECK(neighbour(0, 2) == 150);  // |I[0,2] - I[0,1]|  (right neighbour clamped)
}

TEST_CASE("energy: our manual Sobel equals OpenCV's Sobel on random images", "[energy]") {
    for (unsigned seed = 0; seed < 50; ++seed) {
        const int rows = 1 + static_cast<int>(seed % 9);
        const int cols = 1 + static_cast<int>((seed * 7) % 11);
        const cv::Mat_<uchar> gray = test_support::random_gray(rows, cols, seed);
        INFO("seed " << seed << ", size " << rows << " x " << cols);
        CHECK(same_matrix(seam::compute_gradient_energy(gray),
                          seam::compute_gradient_energy_manual(gray)));
    }
}

TEST_CASE("energy: values are never negative and have the image's size", "[energy]") {
    const cv::Mat_<uchar> gray = test_support::random_gray(13, 17, 99);
    for (seam::EnergyMode mode :
         {seam::EnergyMode::Gradient, seam::EnergyMode::NeighbourDifference}) {
        const EnergyMap energy = seam::compute_energy(gray, mode);
        CHECK(energy.rows == 13);
        CHECK(energy.cols == 17);
        double smallest = 0.0;
        cv::minMaxLoc(energy, &smallest);
        CHECK(smallest >= 0.0);
    }
}

TEST_CASE("energy: grayscale conversion accepts gray, BGR and BGRA", "[energy]") {
    const cv::Mat_<uchar> gray = test_support::random_gray(4, 5, 1);
    CHECK(cv::countNonZero(seam::to_grayscale(gray) != gray) == 0);

    // A colour image whose three channels are equal converts back to the same gray.
    cv::Mat colour;
    cv::merge(std::vector<cv::Mat>{gray, gray, gray}, colour);
    CHECK(cv::countNonZero(seam::to_grayscale(colour) != gray) == 0);

    cv::Mat with_alpha;
    cv::merge(std::vector<cv::Mat>{gray, gray, gray, gray}, with_alpha);
    CHECK(seam::to_grayscale(with_alpha).channels() == 1);
}

TEST_CASE("energy: invalid inputs are rejected", "[energy]") {
    CHECK_THROWS_AS(seam::to_grayscale(cv::Mat()), std::invalid_argument);
    const cv::Mat floats(3, 3, CV_32FC1, cv::Scalar(0));
    CHECK_THROWS_AS(seam::to_grayscale(floats), std::invalid_argument);
    const cv::Mat_<uchar> gray(3, 3, static_cast<uchar>(0));
    CHECK_THROWS_AS(seam::compute_energy(gray, seam::EnergyMode::Forward), std::invalid_argument);
}
