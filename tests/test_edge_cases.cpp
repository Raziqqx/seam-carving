// Edge cases that every algorithm must survive: tiny images, ties, gray vs colour,
// carving down to almost nothing, non-square shapes.
#include <catch2/catch_test_macros.hpp>
#include <opencv2/imgproc.hpp>

#include "seam/carve.hpp"
#include "seam/dp.hpp"
#include "seam/energy.hpp"
#include "seam/graphcut.hpp"
#include "seam/greedy.hpp"
#include "test_support.hpp"

using seam::Algorithm;
using seam::CarveOptions;
using seam::CarveResult;
using seam::Direction;
using seam::EnergyMap;
using seam::Seam;

namespace {

const Algorithm kAllAlgorithms[] = {Algorithm::DynamicProgramming, Algorithm::Greedy,
                                    Algorithm::GraphCut};

Seam find_with(Algorithm algorithm, const EnergyMap& energy) {
    switch (algorithm) {
        case Algorithm::DynamicProgramming:
            return seam::find_vertical_seam_dp(energy);
        case Algorithm::Greedy:
            return seam::find_vertical_seam_greedy(energy);
        case Algorithm::GraphCut:
            return seam::find_vertical_seam_graphcut(energy);
    }
    return {};
}

// True if two images have the same size, type and pixels.
bool same_image(const cv::Mat& a, const cv::Mat& b) {
    if (a.size() != b.size() || a.type() != b.type()) {
        return false;
    }
    cv::Mat difference;
    cv::absdiff(a, b, difference);
    return cv::countNonZero(difference.reshape(1)) == 0;
}

}  // namespace

TEST_CASE("edge: 1x1 image has exactly one seam", "[edge]") {
    test_support::require_all_seam_finders();
    const EnergyMap energy = test_support::make_energy({{42}});
    for (Algorithm algorithm : kAllAlgorithms) {
        CHECK(find_with(algorithm, energy) == Seam{0});
    }
    CHECK(seam::find_vertical_seam_dp_rolling(energy) == Seam{0});
}

TEST_CASE("edge: one-pixel-wide image (n = 1) has only the straight seam", "[edge]") {
    test_support::require_all_seam_finders();
    const EnergyMap energy = test_support::make_energy({{3}, {1}, {4}, {1}, {5}});
    for (Algorithm algorithm : kAllAlgorithms) {
        const Seam seam = find_with(algorithm, energy);
        CHECK(seam == Seam(5, 0));
        CHECK(seam::seam_cost(energy, seam) == 14);
    }
}

TEST_CASE("edge: two-pixel-wide image (n = 2)", "[edge]") {
    test_support::require_all_seam_finders();
    const EnergyMap energy = test_support::make_energy({{5, 1}, {1, 5}, {5, 1}, {1, 5}});
    // The zig-zag through all the 1s is a valid seam with cost 4.
    for (Algorithm algorithm : kAllAlgorithms) {
        const Seam seam = find_with(algorithm, energy);
        REQUIRE(seam::is_valid_seam(seam, 4, 2));
        CHECK(seam == Seam{1, 0, 1, 0});
        CHECK(seam::seam_cost(energy, seam) == 4);
    }
}

TEST_CASE("edge: one-row image picks the single cheapest pixel", "[edge]") {
    test_support::require_all_seam_finders();
    const EnergyMap energy = test_support::make_energy({{9, 4, 7, 2, 8}});
    for (Algorithm algorithm : kAllAlgorithms) {
        CHECK(find_with(algorithm, energy) == Seam{3});
    }
}

TEST_CASE("edge: constant image (all ties) still gives valid seams", "[edge]") {
    test_support::require_all_seam_finders();
    const EnergyMap energy(6, 7, 5);
    for (Algorithm algorithm : kAllAlgorithms) {
        const Seam seam = find_with(algorithm, energy);
        REQUIRE(seam::is_valid_seam(seam, 6, 7));
        CHECK(seam::seam_cost(energy, seam) == 30);
    }
}

TEST_CASE("edge: constant colour image can be carved by every algorithm", "[edge]") {
    test_support::require_all_seam_finders();
    const cv::Mat image(6, 8, CV_8UC3, cv::Scalar(10, 20, 30));
    for (Algorithm algorithm : kAllAlgorithms) {
        CarveOptions options;
        options.algorithm = algorithm;
        const CarveResult result = seam::carve_to_size(image, 5, 4, options);
        CHECK(result.image.cols == 5);
        CHECK(result.image.rows == 4);
        CHECK(result.total_cost == 0);
        // Whatever was removed, what is left is still the same constant colour.
        CHECK(result.image.at<cv::Vec3b>(0, 0) == cv::Vec3b(10, 20, 30));
        CHECK(result.image.at<cv::Vec3b>(3, 4) == cv::Vec3b(10, 20, 30));
    }
}

TEST_CASE("edge: grayscale and colour inputs both work and agree", "[edge]") {
    test_support::require_all_seam_finders();
    const cv::Mat_<uchar> gray = test_support::random_gray(9, 11, 77);
    cv::Mat colour;
    cv::cvtColor(gray, colour, cv::COLOR_GRAY2BGR);

    for (Algorithm algorithm : kAllAlgorithms) {
        CarveOptions options;
        options.algorithm = algorithm;
        const CarveResult from_gray = seam::carve_to_size(gray, 7, 6, options);
        const CarveResult from_colour = seam::carve_to_size(colour, 7, 6, options);

        CHECK(from_gray.image.type() == CV_8UC1);
        CHECK(from_colour.image.type() == CV_8UC3);
        CHECK(from_gray.image.size() == cv::Size(7, 6));
        CHECK(from_colour.image.size() == cv::Size(7, 6));

        // A gray picture stored as colour has the same energies, so the same seams.
        CHECK(from_gray.total_cost == from_colour.total_cost);
        cv::Mat colour_as_gray;
        cv::cvtColor(from_colour.image, colour_as_gray, cv::COLOR_BGR2GRAY);
        CHECK(cv::countNonZero(colour_as_gray != from_gray.image) == 0);
    }
}

TEST_CASE("edge: carving down to a single column and a single row", "[edge]") {
    test_support::require_all_seam_finders();
    const cv::Mat image = test_support::random_colour(7, 9, 55);
    for (Algorithm algorithm : kAllAlgorithms) {
        CarveOptions options;
        options.algorithm = algorithm;

        const CarveResult one_column = seam::carve_to_size(image, 1, 7, options);
        CHECK(one_column.image.size() == cv::Size(1, 7));

        const CarveResult one_row = seam::carve_to_size(image, 9, 1, options);
        CHECK(one_row.image.size() == cv::Size(9, 1));

        const CarveResult one_pixel = seam::carve_to_size(image, 1, 1, options);
        CHECK(one_pixel.image.size() == cv::Size(1, 1));
        CHECK(one_pixel.seams.size() == 8 + 6);
    }
}

TEST_CASE("edge: very non-square images (tall and wide)", "[edge]") {
    test_support::require_all_seam_finders();
    for (Algorithm algorithm : kAllAlgorithms) {
        CarveOptions options;
        options.algorithm = algorithm;

        const cv::Mat tall = test_support::random_colour(40, 3, 9);
        const CarveResult tall_result = seam::carve_to_size(tall, 2, 35, options);
        CHECK(tall_result.image.size() == cv::Size(2, 35));

        const cv::Mat wide = test_support::random_colour(3, 40, 9);
        const CarveResult wide_result = seam::carve_to_size(wide, 30, 2, options);
        CHECK(wide_result.image.size() == cv::Size(30, 2));
    }
}

TEST_CASE("edge: every energy mode works with every algorithm that supports it", "[edge]") {
    test_support::require_all_seam_finders();
    const cv::Mat image = test_support::random_colour(8, 9, 123);
    for (Algorithm algorithm : kAllAlgorithms) {
        for (seam::EnergyMode mode :
             {seam::EnergyMode::Gradient, seam::EnergyMode::NeighbourDifference}) {
            CarveOptions options;
            options.algorithm = algorithm;
            options.energy_mode = mode;
            const CarveResult result = seam::carve_to_size(image, 6, 6, options);
            CHECK(result.image.size() == cv::Size(6, 6));
        }
    }
}

TEST_CASE("edge: large energy values do not overflow the seam cost", "[edge]") {
    test_support::require_greedy();
    // 1000 rows of the largest Sobel value: cumulative cost 2,040,000 fits easily in int.
    const EnergyMap energy(1000, 3, 2040);
    const Seam seam = seam::find_vertical_seam_dp(energy);
    CHECK(seam::seam_cost(energy, seam) == 2040000);
    CHECK(seam::seam_cost(energy, seam::find_vertical_seam_greedy(energy)) == 2040000);
}

TEST_CASE("edge: is_valid_seam accepts and rejects the right things", "[edge]") {
    CHECK(seam::is_valid_seam(Seam{0, 1, 2, 1}, 4, 3));
    CHECK_FALSE(seam::is_valid_seam(Seam{0, 1, 2}, 4, 3));      // one entry missing
    CHECK_FALSE(seam::is_valid_seam(Seam{0, 2, 2, 2}, 4, 3));   // jumps two columns
    CHECK_FALSE(seam::is_valid_seam(Seam{0, 1, 2, 3}, 4, 3));   // column 3 out of range
    CHECK_FALSE(seam::is_valid_seam(Seam{-1, 0, 0, 0}, 4, 3));  // negative column
}

TEST_CASE("edge: horizontal == transpose(vertical(transpose)) for every algorithm", "[edge]") {
    test_support::require_all_seam_finders();
    const cv::Mat image = test_support::random_colour(9, 12, 11);
    for (Algorithm algorithm : kAllAlgorithms) {
        CarveOptions options;
        options.algorithm = algorithm;

        // Finding: the horizontal seam of the image is the vertical seam of the transpose.
        const Seam horizontal = seam::find_seam(image, Direction::Horizontal, options);
        const Seam vertical_of_transpose =
            seam::find_seam(seam::transpose_image(image), Direction::Vertical, options);
        CHECK(horizontal == vertical_of_transpose);
        CHECK(seam::is_valid_seam(horizontal, image.cols, image.rows));

        // Removing: same relation for the images.
        const cv::Mat direct = seam::remove_seam(image, horizontal, Direction::Horizontal);
        const cv::Mat via_transpose = seam::transpose_image(
            seam::remove_vertical_seam(seam::transpose_image(image), horizontal));
        CHECK(same_image(direct, via_transpose));
        CHECK(direct.rows == image.rows - 1);
        CHECK(direct.cols == image.cols);

        // Whole loop: k horizontal seams == k vertical seams on the transpose.
        const CarveResult rows_removed =
            seam::remove_seams(image, 4, Direction::Horizontal, options);
        const CarveResult cols_removed =
            seam::remove_seams(seam::transpose_image(image), 4, Direction::Vertical, options);
        CHECK(same_image(rows_removed.image, seam::transpose_image(cols_removed.image)));
        CHECK(rows_removed.total_cost == cols_removed.total_cost);
    }
}

TEST_CASE("edge: output shape is exactly (rows, cols - k) or (rows - k, cols) for every algorithm",
          "[edge]") {
    test_support::require_all_seam_finders();
    const cv::Mat image = test_support::random_colour(10, 14, 5);
    for (Algorithm algorithm : kAllAlgorithms) {
        CarveOptions options;
        options.algorithm = algorithm;
        for (int count : {0, 1, 5, 13}) {
            const CarveResult result =
                seam::remove_seams(image, count, Direction::Vertical, options);
            CHECK(result.image.rows == 10);
            CHECK(result.image.cols == 14 - count);
            CHECK(static_cast<int>(result.seams.size()) == count);
        }
        for (int count : {0, 1, 5, 9}) {
            const CarveResult result =
                seam::remove_seams(image, count, Direction::Horizontal, options);
            CHECK(result.image.rows == 10 - count);
            CHECK(result.image.cols == 14);
        }
    }
}
