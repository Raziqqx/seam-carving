// Tests for the seam visualisation helpers (bonus 7a).
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "seam/carve.hpp"
#include "seam/visualize.hpp"
#include "test_support.hpp"

using seam::CarveOptions;
using seam::CarveResult;
using seam::Direction;
using seam::Seam;

namespace {

cv::Vec3b as_pixel(const cv::Scalar& colour) {
    return cv::Vec3b(static_cast<uchar>(colour[0]), static_cast<uchar>(colour[1]),
                     static_cast<uchar>(colour[2]));
}

// Number of pixels of `image` that have exactly this colour.
int count_colour(const cv::Mat& image, const cv::Vec3b& colour) {
    int count = 0;
    for (int row = 0; row < image.rows; ++row) {
        for (int col = 0; col < image.cols; ++col) {
            if (image.at<cv::Vec3b>(row, col) == colour) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

TEST_CASE("visualize: draw_seam paints exactly the seam pixels", "[visualize]") {
    const cv::Mat image(4, 5, CV_8UC3, cv::Scalar(255, 255, 255));

    const cv::Mat vertical = seam::draw_seam(image, Seam{1, 2, 2, 3}, Direction::Vertical);
    const cv::Vec3b red = as_pixel(seam::kVerticalSeamColour);
    CHECK(count_colour(vertical, red) == 4);
    CHECK(vertical.at<cv::Vec3b>(0, 1) == red);
    CHECK(vertical.at<cv::Vec3b>(3, 3) == red);

    // Horizontal seam: one entry per COLUMN, giving the row.
    const cv::Mat horizontal = seam::draw_seam(image, Seam{0, 1, 1, 2, 3}, Direction::Horizontal);
    const cv::Vec3b green = as_pixel(seam::kHorizontalSeamColour);
    CHECK(count_colour(horizontal, green) == 5);
    CHECK(horizontal.at<cv::Vec3b>(3, 4) == green);
}

TEST_CASE("visualize: draw_seam accepts gray images and does not modify its input", "[visualize]") {
    const cv::Mat_<uchar> gray(3, 3, static_cast<uchar>(200));
    const cv::Mat drawn = seam::draw_seam(gray, Seam{0, 0, 0}, Direction::Vertical);
    CHECK(drawn.type() == CV_8UC3);
    CHECK(cv::countNonZero(gray != 200) == 0);
}

TEST_CASE("visualize: draw_all_seams marks every removed pixel on the original", "[visualize]") {
    // A white image has no pure red or pure green pixels of its own.
    const cv::Mat image(8, 10, CV_8UC3, cv::Scalar(255, 255, 255));
    const CarveResult result = seam::carve_to_size(image, 7, 6, CarveOptions());
    const cv::Mat overlay = seam::draw_all_seams(image, result.seams);

    CHECK(overlay.size() == image.size());
    // 3 vertical seams of 8 pixels each; 2 horizontal seams of 7 pixels each (the image
    // was already 7 wide when they were removed). No pixel is removed twice.
    CHECK(count_colour(overlay, as_pixel(seam::kVerticalSeamColour)) == 3 * 8);
    CHECK(count_colour(overlay, as_pixel(seam::kHorizontalSeamColour)) == 2 * 7);
}

TEST_CASE("visualize: the recorder stores one frame per seam plus the final image", "[visualize]") {
    const cv::Mat image = test_support::random_colour(8, 10, 3);
    seam::AnimationRecorder recorder(image.size());
    const CarveResult result =
        seam::carve_to_size(image, 7, 6, CarveOptions(), recorder.callback());
    recorder.add_final_frame(result.image);

    REQUIRE(recorder.frames().size() == 5 + 1);
    for (const cv::Mat& frame : recorder.frames()) {
        CHECK(frame.size() == image.size());  // every frame is padded to the original size
        CHECK(frame.type() == CV_8UC3);
    }
    // The last frame shows the result in the top-left corner and black elsewhere.
    const cv::Mat& last = recorder.frames().back();
    CHECK(last.at<cv::Vec3b>(0, 0) == result.image.at<cv::Vec3b>(0, 0));
    CHECK(last.at<cv::Vec3b>(7, 9) == cv::Vec3b(0, 0, 0));
}

TEST_CASE("visualize: the recorder handles enlarging (frames as large as the target)",
          "[visualize]") {
    // Regression test: recording an ENLARGE used to fail because the frames had the size
    // of the original image and the grown image did not fit.
    const cv::Mat image = test_support::random_colour(8, 10, 9);
    for (const cv::Size target :
         {cv::Size(14, 8), cv::Size(10, 11), cv::Size(13, 12), cv::Size(7, 11), cv::Size(13, 5)}) {
        seam::AnimationRecorder recorder(
            seam::animation_canvas_size(image.size(), target.width, target.height));
        const CarveResult result = seam::carve_to_size(image, target.width, target.height,
                                                       CarveOptions(), recorder.callback());
        recorder.add_final_frame(result.image);

        INFO("target " << target.width << " x " << target.height);
        CHECK(result.image.size() == target);
        CHECK(recorder.frames().size() == result.seams.size() + 1);
        for (const cv::Mat& frame : recorder.frames()) {
            CHECK(frame.cols == std::max(10, target.width));
            CHECK(frame.rows == std::max(8, target.height));
        }
    }
}

TEST_CASE("visualize: a picture larger than the canvas is cropped, not an error", "[visualize]") {
    const cv::Mat big(9, 12, CV_8UC3, cv::Scalar(1, 2, 3));
    seam::AnimationRecorder recorder(cv::Size(5, 4));
    recorder.add_final_frame(big);
    REQUIRE(recorder.frames().size() == 1);
    CHECK(recorder.frames().front().size() == cv::Size(5, 4));
    CHECK(recorder.frames().front().at<cv::Vec3b>(3, 4) == cv::Vec3b(1, 2, 3));
}

TEST_CASE("visualize: colourise_energy gives a colour image of the same size", "[visualize]") {
    const seam::EnergyMap energy = test_support::random_energy(5, 6, 100, 1);
    const cv::Mat coloured = seam::colourise_energy(energy);
    CHECK(coloured.size() == energy.size());
    CHECK(coloured.type() == CV_8UC3);
}

TEST_CASE("visualize: write_video refuses an empty frame list", "[visualize]") {
    CHECK_FALSE(seam::write_video("unused.mp4", {}, 30.0));
}
