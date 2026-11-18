#include "seam/visualize.hpp"

#include <algorithm>
#include <cstdio>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

namespace seam {

namespace {

// Paints one pixel of a BGR image.
void paint_pixel(cv::Mat& bgr_image, int row, int col, const cv::Scalar& colour) {
    bgr_image.at<cv::Vec3b>(row, col) =
        cv::Vec3b(static_cast<uchar>(colour[0]), static_cast<uchar>(colour[1]),
                  static_cast<uchar>(colour[2]));
}

}  // namespace

// 3-channel copy, so coloured seams can be drawn on gray images too.
cv::Mat to_bgr(const cv::Mat& image) {
    cv::Mat bgr;
    if (image.channels() == 1) {
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
    } else {
        bgr = image.clone();
    }
    return bgr;
}

// Paints one seam on a copy of the image (red = vertical, green = horizontal).
// Time O(rows * cols) for the copy plus O(seam length).
cv::Mat draw_seam(const cv::Mat& image, const Seam& seam, Direction direction) {
    cv::Mat canvas = to_bgr(image);
    if (direction == Direction::Vertical) {
        for (int row = 0; row < canvas.rows; ++row) {
            paint_pixel(canvas, row, seam[row], kVerticalSeamColour);
        }
    } else {
        for (int col = 0; col < canvas.cols; ++col) {
            paint_pixel(canvas, seam[col], col, kHorizontalSeamColour);
        }
    }
    return canvas;
}

// Paints every recorded seam on a copy of the ORIGINAL image.
// Time O(rows * cols + total number of seam pixels).
cv::Mat draw_all_seams(const cv::Mat& original, const std::vector<SeamRecord>& seams) {
    cv::Mat canvas = to_bgr(original);
    for (const SeamRecord& record : seams) {
        const cv::Scalar colour =
            (record.direction == Direction::Vertical) ? kVerticalSeamColour : kHorizontalSeamColour;
        for (const cv::Point& pixel : record.original_pixels) {
            paint_pixel(canvas, pixel.y, pixel.x, colour);
        }
    }
    return canvas;
}

// Energy map as a colour picture (blue = low, red = high).
cv::Mat colourise_energy(const EnergyMap& energy) {
    // Stretch the energies to 0..255, then map them through a colour scale.
    cv::Mat normalised;
    cv::normalize(energy, normalised, 0, 255, cv::NORM_MINMAX, CV_8U);
    cv::Mat coloured;
    cv::applyColorMap(normalised, coloured, cv::COLORMAP_JET);
    return coloured;
}

// Size of a canvas that can hold every picture of a resize from `original` to the
// target: the larger of the two in each direction.
cv::Size animation_canvas_size(cv::Size original, int target_cols, int target_rows) {
    return cv::Size(std::max(original.width, target_cols), std::max(original.height, target_rows));
}

AnimationRecorder::AnimationRecorder(cv::Size canvas_size) : canvas_size_(canvas_size) {}

// Pads the picture to the canvas size (black background) and stores it as a frame.
// Time O(canvas pixels).
void AnimationRecorder::add_frame(const cv::Mat& image) {
    cv::Mat frame = cv::Mat::zeros(canvas_size_, CV_8UC3);
    const cv::Mat bgr = to_bgr(image);
    // Copy the picture into the top-left corner of the canvas. If the picture is larger
    // than the canvas in some direction, only the part that fits is shown.
    const cv::Rect visible(0, 0, std::min(bgr.cols, canvas_size_.width),
                           std::min(bgr.rows, canvas_size_.height));
    bgr(visible).copyTo(frame(visible));
    frames_.push_back(frame);
}

// The callback for carve_to_size: draw the seam on the picture and keep it as a frame.
StepCallback AnimationRecorder::callback() {
    return [this](const cv::Mat& image_with_seam, const SeamRecord& record) {
        add_frame(draw_seam(image_with_seam, record.seam, record.direction));
    };
}

void AnimationRecorder::add_final_frame(const cv::Mat& image) {
    add_frame(image);
}

const std::vector<cv::Mat>& AnimationRecorder::frames() const {
    return frames_;
}

// Writes the frames as an MP4 video. Returns false if the file could not be opened.
bool write_video(const std::string& path, const std::vector<cv::Mat>& frames, double fps) {
    if (frames.empty()) {
        return false;
    }
    cv::VideoWriter writer(path, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps,
                           frames.front().size());
    if (!writer.isOpened()) {
        return false;
    }
    for (const cv::Mat& frame : frames) {
        writer.write(frame);
    }
    return true;
}

// Writes every n-th frame (and the last one) as a numbered PNG file.
int write_frames(const std::string& prefix, const std::vector<cv::Mat>& frames, int every_nth) {
    // A step below 1 would make `index % every_nth` divide by zero (or keep nothing),
    // so it is treated as 1: keep every frame.
    every_nth = std::max(every_nth, 1);
    int written = 0;
    const int last = static_cast<int>(frames.size()) - 1;
    for (int index = 0; index <= last; ++index) {
        const bool keep = (index % every_nth == 0) || (index == last);
        if (!keep) {
            continue;
        }
        char name[32];
        std::snprintf(name, sizeof(name), "_%04d.png", index);
        if (cv::imwrite(prefix + name, frames[index])) {
            ++written;
        }
    }
    return written;
}

}  // namespace seam
