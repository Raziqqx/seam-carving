// visualize.hpp - seam visualisation (bonus 7a).
//
// Two kinds of pictures:
//   1. an OVERLAY: every removed seam drawn on the original image, so one picture shows
//      where the algorithm took pixels from;
//   2. an ANIMATION: one frame per seam, showing the current image with the seam that is
//      about to be removed (or that was just inserted) highlighted, saved as an MP4 video
//      and/or numbered PNG frames.
#pragma once

#include <string>
#include <vector>

#include "seam/carve.hpp"
#include "seam/types.hpp"

namespace seam {

// Colours (BGR order, as OpenCV stores them).
const cv::Scalar kVerticalSeamColour(0, 0, 255);    // red
const cv::Scalar kHorizontalSeamColour(0, 200, 0);  // green

// Returns a 3-channel BGR copy of an 8-bit image, so coloured seams can be drawn on
// gray images too.
cv::Mat to_bgr(const cv::Mat& image);

// Returns a copy of `image` with one seam painted on it. The seam is given in the
// coordinates of `image` (vertical: seam[row] = col, horizontal: seam[col] = row).
// Time O(rows * cols) for the copy.
cv::Mat draw_seam(const cv::Mat& image, const Seam& seam, Direction direction);

// Returns a copy of the ORIGINAL image with every recorded seam painted on it
// (vertical seams red, horizontal seams green), using SeamRecord::original_pixels.
// Time O(rows * cols + number of seam pixels).
cv::Mat draw_all_seams(const cv::Mat& original, const std::vector<SeamRecord>& seams);

// Turns an energy map into a colour picture (dark blue = low energy, red = high) for
// figures and for the app.
cv::Mat colourise_energy(const EnergyMap& energy);

// Size of a canvas that can hold every picture of a resize from `original` to the target
// size: the larger of the two in each direction (the target can be larger when enlarging).
cv::Size animation_canvas_size(cv::Size original, int target_cols, int target_rows);

// Collects the animation frames while carve_to_size / remove_seams runs.
// Usage:
//     AnimationRecorder recorder(animation_canvas_size(image.size(), w, h));
//     CarveResult result = carve_to_size(image, w, h, options, recorder.callback());
//     recorder.add_final_frame(result.image);
//     write_video("out.mp4", recorder.frames(), 30);
class AnimationRecorder {
public:
    // Every frame has `canvas_size`; the current image is drawn in the top-left corner
    // on a black background. Use animation_canvas_size() so that an enlarged image fits.
    explicit AnimationRecorder(cv::Size canvas_size);

    // The function to pass to carve_to_size / remove_seams as `on_step`.
    StepCallback callback();

    // Adds the finished image as the last frame.
    void add_final_frame(const cv::Mat& image);

    const std::vector<cv::Mat>& frames() const;

private:
    cv::Size canvas_size_;
    std::vector<cv::Mat> frames_;

    // Pads `image` to the canvas size and stores it.
    void add_frame(const cv::Mat& image);
};

// Writes frames to a video file (MP4 with the 'mp4v' codec). Returns false if OpenCV
// could not open the file (for example when it was built without a video backend).
bool write_video(const std::string& path, const std::vector<cv::Mat>& frames, double fps);

// Writes frames as numbered PNG files: <prefix>_0000.png, <prefix>_0001.png, ...
// `every_nth` >= 1 keeps only every n-th frame (plus the last one) to save disk space.
// Returns the number of files written.
int write_frames(const std::string& prefix, const std::vector<cv::Mat>& frames, int every_nth = 1);

}  // namespace seam
