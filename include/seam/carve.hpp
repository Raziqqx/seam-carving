// carve.hpp - the resize loop shared by all three seam finders (spec item 1c).
//
//   repeat until the image has the target size:
//       1. compute the energy map of the CURRENT image   (recomputed after every seam)
//       2. find one seam (DP, greedy or graph cut)
//       3. remove that seam
//
// Horizontal seams reuse the vertical code: transpose the image (rows <-> columns), carve
// vertical seams, and transpose back.
#pragma once

#include <functional>
#include <vector>

#include "seam/types.hpp"

namespace seam {

// What the user can choose for a resize.
struct CarveOptions {
    Algorithm algorithm = Algorithm::DynamicProgramming;
    EnergyMode energy_mode = EnergyMode::Gradient;
};

// One removed or inserted seam, kept so it can be drawn and its cost reported.
struct SeamRecord {
    Direction direction;
    // false: the seam was removed. true: the seam was inserted (enlarging).
    bool inserted = false;
    // Where to draw the seam in the picture handed to the step callback.
    // Vertical: seam[row] = column. Horizontal: seam[col] = row.
    //   * removed seam:  its position in the image just BEFORE the removal;
    //   * inserted seam: the position of the NEW pixels in the image just AFTER the insertion.
    Seam seam;
    // Energy of the seam at the moment it was chosen.
    long long cost = 0;
    // The seam's pixels in the coordinates of the ORIGINAL image (x = column, y = row):
    // the removed pixels, or for an inserted seam the pixels that were duplicated.
    // Needed to draw every seam on the original picture (bonus 7a). A pixel that was
    // itself created by an earlier insertion has no original position and is left out.
    std::vector<cv::Point> original_pixels;
};

// Result of a resize.
struct CarveResult {
    cv::Mat image;                  // the resized image
    std::vector<SeamRecord> seams;  // every seam, in the order it was processed
    long long total_cost = 0;       // sum of the seam costs
};

// Called once per seam with a picture to draw the seam on (see SeamRecord::seam):
// the image just before a removal, or the image just after an insertion.
// The visualiser uses it to build the step-by-step animation.
using StepCallback = std::function<void(const cv::Mat& image_with_seam, const SeamRecord& record)>;

// Finds one vertical seam in `image` with the chosen algorithm and energy.
// If `cost` is not null it receives the energy of the seam.
// Time: energy O(rows*cols) + the chosen seam finder (see dp.hpp, greedy.hpp, graphcut.hpp).
Seam find_vertical_seam(const cv::Mat& image, const CarveOptions& options,
                        long long* cost = nullptr);

// Returns a copy of `image` without the seam pixels: in every row the pixels to the right
// of the seam move one step left, so the result is one column narrower.
// Works for any pixel type (gray, colour, or our integer index maps).
// Time O(rows * cols), space O(rows * cols) for the result.
cv::Mat remove_vertical_seam(const cv::Mat& image, const Seam& seam);

// Swaps rows and columns: pixel (r, c) moves to (c, r). A horizontal seam of the image is
// a vertical seam of its transpose.
// Time O(rows * cols), space O(rows * cols).
cv::Mat transpose_image(const cv::Mat& image);

// Finds one seam in the given direction (horizontal = transpose, find vertical).
Seam find_seam(const cv::Mat& image, Direction direction, const CarveOptions& options,
               long long* cost = nullptr);

// Removes one seam in the given direction (horizontal = transpose, remove, transpose back).
cv::Mat remove_seam(const cv::Mat& image, const Seam& seam, Direction direction);

// Removes `count` seams in one direction.
// Time: count * (cost of one seam search + O(rows * cols)).
CarveResult remove_seams(const cv::Mat& image, int count, Direction direction,
                         const CarveOptions& options, const StepCallback& on_step = nullptr);

// Returns a copy of `image` that is one column wider: in every row a new pixel is placed
// at column new_cols[row] (1 <= new_cols[row] <= image.cols) and the pixels from there on
// shift one step right. The new pixel is the average of its left and right neighbours
// (just a copy of the left one at the right border). The opposite of remove_vertical_seam.
// Time O(rows * cols), space O(rows * cols) for the result.
cv::Mat insert_vertical_seam(const cv::Mat& image, const Seam& new_cols);

// Seam insertion (enlarging): finds the `count` cheapest seams by removing them from a
// scratch copy, then inserts one new pixel to the right of (or below) every pixel of
// those seams. Finding all seams first matters: inserting the single best seam `count`
// times would stretch one stripe of the picture.
// Requires count < current size in that direction.
// Time: same as removing `count` seams, plus O(count * rows * cols) for the insertions.
CarveResult insert_seams(const cv::Mat& image, int count, Direction direction,
                         const CarveOptions& options, const StepCallback& on_step = nullptr);

// Resizes `image` to exactly target_cols x target_rows.
// Width is handled first (vertical seams), then height (horizontal seams). A smaller
// target removes seams; a larger target inserts seams. The seams of both stages are
// reported in the coordinates of the original image (SeamRecord::original_pixels).
// Requires 1 <= target and target < 2 * current size in each direction.
CarveResult carve_to_size(const cv::Mat& image, int target_cols, int target_rows,
                          const CarveOptions& options, const StepCallback& on_step = nullptr);

}  // namespace seam
