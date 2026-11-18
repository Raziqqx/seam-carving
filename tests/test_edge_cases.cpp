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

// Random BGRA image: random colours, and every pixel has the same alpha value.
// A constant alpha lets the test see whether the alpha channel survived carving.
// Time and space O(rows * cols).
cv::Mat random_bgra(int rows, int cols, unsigned seed, uchar alpha) {
    const cv::Mat bgr = test_support::random_colour(rows, cols, seed);
    cv::Mat bgra(rows, cols, CV_8UC4);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const cv::Vec3b colour = bgr.at<cv::Vec3b>(row, col);
            bgra.at<cv::Vec4b>(row, col) = cv::Vec4b(colour[0], colour[1], colour[2], alpha);
        }
    }
    return bgra;
}

// Carves a BGRA image to (cols x rows) with one algorithm and checks that the result is
// still BGRA, has the right size, and that no alpha value was changed.
// Time: one carve_to_size call plus O(rows * cols) for the alpha check.
void check_bgra_carve(const cv::Mat& bgra, Algorithm algorithm, int cols, int rows) {
    CarveOptions options;
    options.algorithm = algorithm;
    const CarveResult result = seam::carve_to_size(bgra, cols, rows, options);
    CHECK(result.image.type() == CV_8UC4);
    CHECK(result.image.size() == cv::Size(cols, rows));

    // Whole pixels are removed or inserted (an inserted pixel is the average of two
    // neighbours), so with a constant alpha every output alpha must still be that value.
    cv::Mat alpha;
    cv::extractChannel(result.image, alpha, 3);
    const uchar expected_alpha = bgra.at<cv::Vec4b>(0, 0)[3];
    CHECK(cv::countNonZero(alpha != expected_alpha) == 0);
}

}  // namespace

TEST_CASE("edge: 1x1 image has exactly one seam", "[edge]") {
    const EnergyMap energy = test_support::make_energy({{42}});
    for (Algorithm algorithm : kAllAlgorithms) {
        CHECK(find_with(algorithm, energy) == Seam{0});
    }
    CHECK(seam::find_vertical_seam_dp_rolling(energy) == Seam{0});
}

TEST_CASE("edge: one-pixel-wide image (n = 1) has only the straight seam", "[edge]") {
    const EnergyMap energy = test_support::make_energy({{3}, {1}, {4}, {1}, {5}});
    for (Algorithm algorithm : kAllAlgorithms) {
        const Seam seam = find_with(algorithm, energy);
        CHECK(seam == Seam(5, 0));
        CHECK(seam::seam_cost(energy, seam) == 14);
    }
}

TEST_CASE("edge: two-pixel-wide image (n = 2)", "[edge]") {
    const EnergyMap energy = test_support::make_energy({{5, 1}, {1, 5}, {5, 1}, {1, 5}});
    // The zig-zag through all the 1s is a valid seam with cost 4.
    for (Algorithm algorithm : kAllAlgorithms) {
        const Seam seam = find_with(algorithm, energy);
        REQUIRE(seam::is_valid_seam(seam, 4, 2));
        CHECK(seam == Seam{1, 0, 1, 0});
        CHECK(seam::seam_cost(energy, seam) == 4);
    }
}

// 2 x 2 is the smallest image where the seam has a real choice in every row. There are
// exactly 4 seams, so the brute force checks all of them.
TEST_CASE("edge: 2x2 image (smallest image with a choice in every row)", "[edge]") {
    const EnergyMap energy = test_support::make_energy({{3, 1}, {2, 5}});
    // Seam costs: {0,0} = 5, {0,1} = 8, {1,0} = 3, {1,1} = 6, so {1,0} is the unique best.
    REQUIRE(test_support::all_seams(2, 2).size() == 4);
    REQUIRE(test_support::brute_force_min_cost(energy) == 3);
    CHECK(seam::find_vertical_seam_dp(energy) == Seam{1, 0});
    CHECK(seam::find_vertical_seam_dp_rolling(energy) == Seam{1, 0});

    // Carving a 2 x 2 colour image all the way down to 1 x 1 removes exactly 2 seams.
    const cv::Mat image = test_support::random_colour(2, 2, 21);
    const CarveResult dp_result = seam::carve_to_size(image, 1, 1, CarveOptions{});
    CHECK(dp_result.image.size() == cv::Size(1, 1));
    CHECK(dp_result.seams.size() == 2);

    for (Algorithm algorithm : kAllAlgorithms) {
        // Greedy also finds it here: the cheapest top pixel (col 1) leads straight to the
        // cheapest pixel below it (col 0).
        CHECK(find_with(algorithm, energy) == Seam{1, 0});

        CarveOptions options;
        options.algorithm = algorithm;
        const CarveResult result = seam::carve_to_size(image, 1, 1, options);
        CHECK(result.image.size() == cv::Size(1, 1));
        CHECK(result.seams.size() == 2);
    }
}

// A horizontal seam runs from the left edge to the right edge with one pixel in EVERY
// column, so it cannot go around an expensive column: it has to cross it, and the best
// seam crosses at the one cheap gap. The library finds a horizontal seam as the vertical
// seam of the transpose (carve.cpp, find_seam), so this test does the same.
TEST_CASE("edge: every horizontal seam must cross an expensive column", "[edge]") {
    // 7 rows x 5 cols. Column 2 costs 1000 in every row except row 5 (the gap, 100).
    // Everything else costs 0, so the cheapest horizontal seam costs exactly 100.
    EnergyMap energy(7, 5, 0);
    for (int row = 0; row < energy.rows; ++row) {
        energy(row, 2) = 1000;
    }
    energy(5, 2) = 100;

    // Transposed: 5 rows x 7 cols; old column 2 is now row 2 and the gap is at column 5.
    EnergyMap transposed;
    cv::transpose(energy, transposed);
    REQUIRE(test_support::brute_force_min_cost(transposed) == 100);

    const Seam dp = seam::find_vertical_seam_dp(transposed);
    REQUIRE(seam::is_valid_seam(dp, transposed.rows, transposed.cols));
    CHECK(dp[2] == 5);  // the horizontal seam crosses column 2 in row 5, the gap
    CHECK(seam::seam_cost(transposed, dp) == 100);

    const Seam graphcut = seam::find_vertical_seam_graphcut(transposed);
    REQUIRE(seam::is_valid_seam(graphcut, transposed.rows, transposed.cols));
    CHECK(graphcut[2] == 5);
    CHECK(seam::seam_cost(transposed, graphcut) == 100);

    // Greedy cannot see the gap coming. Every top-row pixel costs 0, so it starts at the
    // leftmost one (col 0) and goes straight down on ties (greedy.hpp). A seam moves at
    // most one column per row, so by row 2 it can only be at col 0 or 1: both cost 1000.
    // This is the "valid but not optimal" behaviour of greedy on a simple example.
    const Seam greedy = seam::find_vertical_seam_greedy(transposed);
    REQUIRE(seam::is_valid_seam(greedy, transposed.rows, transposed.cols));
    CHECK(seam::seam_cost(transposed, greedy) == 1000);
}

// PNG files can carry an alpha channel (BGRA, 4 channels). The energy is computed from
// the colour, but whole pixels, alpha included, are removed and inserted, so the output
// must still have 4 channels and an unchanged alpha. Shrinking and enlarging both checked.
TEST_CASE("edge: 4-channel (BGRA) image keeps 4 channels and the right size", "[edge]") {
    const cv::Mat bgra = random_bgra(8, 10, 31, 200);
    check_bgra_carve(bgra, Algorithm::DynamicProgramming, 7, 6);   // shrink both ways
    check_bgra_carve(bgra, Algorithm::DynamicProgramming, 13, 6);  // wider and shorter

    for (Algorithm algorithm : kAllAlgorithms) {
        check_bgra_carve(bgra, algorithm, 7, 6);
        check_bgra_carve(bgra, algorithm, 13, 6);
    }
}

TEST_CASE("edge: one-row image picks the single cheapest pixel", "[edge]") {
    const EnergyMap energy = test_support::make_energy({{9, 4, 7, 2, 8}});
    for (Algorithm algorithm : kAllAlgorithms) {
        CHECK(find_with(algorithm, energy) == Seam{3});
    }
}

TEST_CASE("edge: constant image (all ties) still gives valid seams", "[edge]") {
    const EnergyMap energy(6, 7, 5);
    for (Algorithm algorithm : kAllAlgorithms) {
        const Seam seam = find_with(algorithm, energy);
        REQUIRE(seam::is_valid_seam(seam, 6, 7));
        CHECK(seam::seam_cost(energy, seam) == 30);
    }
}

TEST_CASE("edge: constant colour image can be carved by every algorithm", "[edge]") {
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
