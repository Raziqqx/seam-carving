// Tests for seam removal, transposition, the resize loop and seam insertion (item 1c).
#include <catch2/catch_test_macros.hpp>

#include "seam/carve.hpp"
#include "seam/dp.hpp"
#include "seam/energy.hpp"
#include "test_support.hpp"

using seam::CarveOptions;
using seam::CarveResult;
using seam::Direction;
using seam::Seam;

namespace {

bool same_image(const cv::Mat& a, const cv::Mat& b) {
    if (a.size() != b.size() || a.type() != b.type()) {
        return false;
    }
    cv::Mat difference;
    cv::absdiff(a, b, difference);
    return cv::countNonZero(difference.reshape(1)) == 0;
}

}  // namespace

TEST_CASE("carve: remove_vertical_seam shifts the right part left", "[carve]") {
    cv::Mat_<uchar> image(3, 4);
    image << 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12;
    const cv::Mat removed = seam::remove_vertical_seam(image, Seam{0, 1, 2});

    cv::Mat_<uchar> expected(3, 3);
    expected << 2, 3, 4, 5, 7, 8, 9, 10, 12;
    CHECK(same_image(removed, expected));
}

TEST_CASE("carve: removing the first or the last column works", "[carve]") {
    cv::Mat_<uchar> image(2, 3);
    image << 1, 2, 3, 4, 5, 6;

    cv::Mat_<uchar> without_first(2, 2);
    without_first << 2, 3, 5, 6;
    CHECK(same_image(seam::remove_vertical_seam(image, Seam{0, 0}), without_first));

    cv::Mat_<uchar> without_last(2, 2);
    without_last << 1, 2, 4, 5;
    CHECK(same_image(seam::remove_vertical_seam(image, Seam{2, 2}), without_last));
}

TEST_CASE("carve: remove_vertical_seam keeps colour pixels intact", "[carve]") {
    const cv::Mat image = test_support::random_colour(5, 6, 3);
    const Seam seam{2, 3, 3, 2, 1};
    const cv::Mat removed = seam::remove_vertical_seam(image, seam);

    REQUIRE(removed.rows == 5);
    REQUIRE(removed.cols == 5);
    REQUIRE(removed.type() == CV_8UC3);
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            const int source_col = (col < seam[row]) ? col : col + 1;
            CHECK(removed.at<cv::Vec3b>(row, col) == image.at<cv::Vec3b>(row, source_col));
        }
    }
}

TEST_CASE("carve: invalid seams are rejected", "[carve]") {
    const cv::Mat_<uchar> image(3, 4, static_cast<uchar>(0));
    CHECK_THROWS_AS(seam::remove_vertical_seam(image, Seam{0, 1}), std::invalid_argument);  // short
    CHECK_THROWS_AS(seam::remove_vertical_seam(image, Seam{0, 2, 2}),
                    std::invalid_argument);  // jump
    CHECK_THROWS_AS(seam::remove_vertical_seam(image, Seam{3, 4, 3}),
                    std::invalid_argument);  // range
    const cv::Mat_<uchar> one_column(3, 1, static_cast<uchar>(0));
    CHECK_THROWS_AS(seam::remove_vertical_seam(one_column, Seam{0, 0, 0}), std::invalid_argument);
}

TEST_CASE("carve: horizontal == transpose(vertical(transpose))", "[carve]") {
    const cv::Mat image = test_support::random_colour(9, 12, 11);
    {
        // DP here; tests/test_edge_cases.cpp repeats this for greedy and graph cut.
        const CarveOptions options;

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

TEST_CASE("carve: output shape is exactly (rows, cols - k) or (rows - k, cols)", "[carve]") {
    const cv::Mat image = test_support::random_colour(10, 14, 5);
    {
        // DP here; tests/test_edge_cases.cpp repeats this for greedy and graph cut.
        const CarveOptions options;
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

TEST_CASE("carve: the energy map is recomputed after every seam", "[carve]") {
    // Re-run the loop by hand, recomputing the energy from the current image each time,
    // and require that remove_seams made exactly the same choices.
    const cv::Mat image = test_support::random_colour(8, 10, 21);
    const CarveResult result = seam::remove_seams(image, 5, Direction::Vertical, CarveOptions());

    cv::Mat current = image.clone();
    for (int step = 0; step < 5; ++step) {
        const seam::EnergyMap energy = seam::compute_gradient_energy(seam::to_grayscale(current));
        const Seam expected = seam::find_vertical_seam_dp(energy);
        CHECK(result.seams[step].seam == expected);
        CHECK(result.seams[step].cost == seam::seam_cost(energy, expected));
        current = seam::remove_vertical_seam(current, expected);
    }
    CHECK(same_image(result.image, current));
}

TEST_CASE("carve: carve_to_size reaches the exact target in both directions", "[carve]") {
    const cv::Mat image = test_support::random_colour(12, 16, 8);
    const CarveResult result = seam::carve_to_size(image, 11, 7, CarveOptions());
    CHECK(result.image.cols == 11);
    CHECK(result.image.rows == 7);
    CHECK(result.seams.size() == 5 + 5);

    long long sum = 0;
    for (const seam::SeamRecord& record : result.seams) {
        sum += record.cost;
    }
    CHECK(result.total_cost == sum);

    // Vertical seams come first, then horizontal ones.
    CHECK(result.seams.front().direction == Direction::Vertical);
    CHECK(result.seams.back().direction == Direction::Horizontal);
}

TEST_CASE("carve: target equal to the current size changes nothing", "[carve]") {
    const cv::Mat image = test_support::random_colour(6, 7, 2);
    const CarveResult result = seam::carve_to_size(image, 7, 6, CarveOptions());
    CHECK(same_image(result.image, image));
    CHECK(result.seams.empty());
    CHECK(result.total_cost == 0);
}

TEST_CASE("carve: original_pixels are distinct pixels of the original image", "[carve]") {
    const cv::Mat image = test_support::random_colour(9, 11, 4);
    const CarveResult result = seam::carve_to_size(image, 6, 5, CarveOptions());

    // Every removed pixel must be a different original pixel, and the number of removed
    // pixels plus the number of remaining pixels must equal the original pixel count.
    cv::Mat_<int> times_removed = cv::Mat_<int>::zeros(image.rows, image.cols);
    int removed_pixels = 0;
    for (const seam::SeamRecord& record : result.seams) {
        for (const cv::Point& pixel : record.original_pixels) {
            REQUIRE(pixel.x >= 0);
            REQUIRE(pixel.x < image.cols);
            REQUIRE(pixel.y >= 0);
            REQUIRE(pixel.y < image.rows);
            times_removed(pixel.y, pixel.x) += 1;
            ++removed_pixels;
        }
    }
    double most = 0.0;
    cv::minMaxLoc(times_removed, nullptr, &most);
    CHECK(most == 1.0);
    CHECK(removed_pixels + result.image.rows * result.image.cols == image.rows * image.cols);
}

TEST_CASE("carve: the step callback sees every seam before it is removed", "[carve]") {
    const cv::Mat image = test_support::random_colour(7, 9, 6);
    int calls = 0;
    int expected_cols = 9;
    int expected_rows = 7;
    const seam::StepCallback on_step = [&](const cv::Mat& before, const seam::SeamRecord& record) {
        ++calls;
        CHECK(before.cols == expected_cols);
        CHECK(before.rows == expected_rows);
        if (record.direction == Direction::Vertical) {
            --expected_cols;
        } else {
            --expected_rows;
        }
    };
    seam::carve_to_size(image, 6, 5, CarveOptions(), on_step);
    CHECK(calls == 3 + 2);
}

TEST_CASE("carve: seam insertion enlarges and keeps every original pixel", "[carve][insert]") {
    const cv::Mat image = test_support::random_colour(6, 8, 13);
    const CarveResult wider = seam::insert_seams(image, 3, Direction::Vertical, CarveOptions());
    REQUIRE(wider.image.rows == 6);
    REQUIRE(wider.image.cols == 11);
    CHECK(wider.seams.size() == 3);

    // Each row of the result must contain the original row as a subsequence (in order).
    for (int row = 0; row < image.rows; ++row) {
        int matched = 0;
        for (int col = 0; col < wider.image.cols && matched < image.cols; ++col) {
            if (wider.image.at<cv::Vec3b>(row, col) == image.at<cv::Vec3b>(row, matched)) {
                ++matched;
            }
        }
        CHECK(matched == image.cols);
    }

    const CarveResult taller = seam::insert_seams(image, 2, Direction::Horizontal, CarveOptions());
    CHECK(taller.image.rows == 8);
    CHECK(taller.image.cols == 8);
}

TEST_CASE("carve: carve_to_size can enlarge, and mix shrinking with enlarging", "[carve][insert]") {
    const cv::Mat image = test_support::random_colour(8, 10, 17);
    const CarveResult bigger = seam::carve_to_size(image, 13, 10, CarveOptions());
    CHECK(bigger.image.cols == 13);
    CHECK(bigger.image.rows == 10);

    const CarveResult mixed = seam::carve_to_size(image, 12, 5, CarveOptions());
    CHECK(mixed.image.cols == 12);
    CHECK(mixed.image.rows == 5);
}

TEST_CASE("carve: inserting into a constant image keeps it constant", "[carve][insert]") {
    const cv::Mat image(5, 6, CV_8UC3, cv::Scalar(40, 80, 120));
    const CarveResult wider = seam::insert_seams(image, 4, Direction::Vertical, CarveOptions());
    const cv::Mat expected(5, 10, CV_8UC3, cv::Scalar(40, 80, 120));
    CHECK(same_image(wider.image, expected));
}

TEST_CASE("carve: invalid targets are rejected", "[carve]") {
    const cv::Mat image = test_support::random_colour(6, 6, 1);
    const CarveOptions options;
    CHECK_THROWS_AS(seam::carve_to_size(image, 0, 6, options), std::invalid_argument);
    CHECK_THROWS_AS(seam::carve_to_size(image, 6, -2, options), std::invalid_argument);
    CHECK_THROWS_AS(seam::carve_to_size(image, 12, 6, options), std::invalid_argument);
    CHECK_THROWS_AS(seam::carve_to_size(cv::Mat(), 3, 3, options), std::invalid_argument);
    CHECK_THROWS_AS(seam::remove_seams(image, 6, Direction::Vertical, options),
                    std::invalid_argument);
    CHECK_THROWS_AS(seam::remove_seams(image, -1, Direction::Vertical, options),
                    std::invalid_argument);
}

TEST_CASE("carve: forward energy is DP-only", "[carve][forward]") {
    const cv::Mat image = test_support::random_colour(6, 8, 30);
    CarveOptions options;
    options.energy_mode = seam::EnergyMode::Forward;
    const CarveResult result = seam::remove_seams(image, 3, Direction::Vertical, options);
    CHECK(result.image.cols == 5);

    options.algorithm = seam::Algorithm::Greedy;
    CHECK_THROWS_AS(seam::remove_seams(image, 1, Direction::Vertical, options),
                    std::invalid_argument);
    options.algorithm = seam::Algorithm::GraphCut;
    CHECK_THROWS_AS(seam::remove_seams(image, 1, Direction::Vertical, options),
                    std::invalid_argument);
}

TEST_CASE("carve: insert_vertical_seam is the opposite of remove_vertical_seam",
          "[carve][insert]") {
    cv::Mat_<uchar> image(2, 3);
    image << 10, 20, 40, 50, 60, 100;
    // Row 0: new pixel at column 1, between 10 and 20 -> 15.
    // Row 1: new pixel at column 2, between 60 and 100 -> 80.
    const cv::Mat wider = seam::insert_vertical_seam(image, Seam{1, 2});
    cv::Mat_<uchar> expected(2, 4);
    expected << 10, 15, 20, 40, 50, 60, 80, 100;
    CHECK(same_image(wider, expected));

    // Removing the new pixels again gives back the original.
    CHECK(same_image(seam::remove_vertical_seam(wider, Seam{1, 2}), image));

    // At the right border there is no right neighbour: the new pixel copies the left one.
    const cv::Mat at_border = seam::insert_vertical_seam(image, Seam{3, 3});
    cv::Mat_<uchar> expected_border(2, 4);
    expected_border << 10, 20, 40, 40, 50, 60, 100, 100;
    CHECK(same_image(at_border, expected_border));

    CHECK_THROWS_AS(seam::insert_vertical_seam(image, Seam{0, 1}), std::invalid_argument);  // 0
    CHECK_THROWS_AS(seam::insert_vertical_seam(image, Seam{1, 4}),
                    std::invalid_argument);                                              // > cols
    CHECK_THROWS_AS(seam::insert_vertical_seam(image, Seam{1}), std::invalid_argument);  // short
}

TEST_CASE("carve: inserted seams are reported in original coordinates", "[carve][insert]") {
    // Shrink the width, then enlarge the height: the duplicated pixels of the second stage
    // must be reported where they were in the ORIGINAL image, not in the narrowed one.
    cv::Mat image(6, 12, CV_8UC3, cv::Scalar(200, 200, 200));
    image.colRange(0, 4).setTo(cv::Scalar(0, 0, 255));  // a noisy block on the left...
    cv::randu(image.colRange(0, 4), 0, 255);            // ...so removals happen on the right
    const CarveResult result = seam::carve_to_size(image, 8, 8, CarveOptions());
    REQUIRE(result.image.size() == cv::Size(8, 8));
    REQUIRE(result.seams.size() == 4 + 2);

    cv::Mat_<int> removed = cv::Mat_<int>::zeros(image.rows, image.cols);
    for (const seam::SeamRecord& record : result.seams) {
        if (!record.inserted) {
            CHECK(record.direction == Direction::Vertical);
            for (const cv::Point& pixel : record.original_pixels) {
                removed(pixel.y, pixel.x) = 1;
            }
        }
    }
    int duplicated_pixels = 0;
    for (const seam::SeamRecord& record : result.seams) {
        if (record.inserted) {
            CHECK(record.direction == Direction::Horizontal);
            for (const cv::Point& pixel : record.original_pixels) {
                REQUIRE(pixel.x >= 0);
                REQUIRE(pixel.x < image.cols);
                REQUIRE(pixel.y >= 0);
                REQUIRE(pixel.y < image.rows);
                // A pixel that was removed in stage 1 cannot be duplicated in stage 2.
                CHECK(removed(pixel.y, pixel.x) == 0);
                ++duplicated_pixels;
            }
        }
    }
    CHECK(duplicated_pixels > 0);
}

TEST_CASE("carve: the step callback is called for inserted seams too", "[carve][insert]") {
    const cv::Mat image = test_support::random_colour(7, 9, 40);
    int calls = 0;
    int expected_cols = 9;
    const seam::StepCallback on_step = [&](const cv::Mat& picture, const seam::SeamRecord& record) {
        ++calls;
        ++expected_cols;  // the picture is the image just AFTER the insertion
        CHECK(record.inserted);
        CHECK(picture.cols == expected_cols);
        REQUIRE(static_cast<int>(record.seam.size()) == picture.rows);
        for (int row = 0; row < picture.rows; ++row) {
            CHECK(record.seam[row] >= 1);
            CHECK(record.seam[row] < picture.cols);
        }
    };
    const CarveResult wider = seam::carve_to_size(image, 13, 7, CarveOptions(), on_step);
    CHECK(calls == 4);
    CHECK(wider.image.cols == 13);
}
