#include "seam/carve.hpp"

#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

#include "seam/dp.hpp"
#include "seam/energy.hpp"
#include "seam/graphcut.hpp"
#include "seam/greedy.hpp"

namespace seam {

namespace {

// Value stored in an index map for a pixel that was created by seam insertion and
// therefore has no position in the original image.
const int kInsertedPixel = -1;

// An image together with its index map.
// The index map has the same size as the image; each entry says where that pixel was in
// the ORIGINAL image, encoded as row * original_cols + col (or kInsertedPixel). Every
// seam is removed from / inserted into both at once, so at any time we can ask
// "which original pixel is this?". That is what lets us draw all seams on the original.
struct TrackedImage {
    cv::Mat image;
    cv::Mat_<int> index_map;
    int original_cols = 0;
};

// Starts tracking: every pixel is at its own original position.
// Time O(rows * cols), space O(rows * cols).
TrackedImage make_tracked(const cv::Mat& image) {
    TrackedImage state;
    state.image = image.clone();
    state.index_map = cv::Mat_<int>(image.rows, image.cols);
    state.original_cols = image.cols;
    for (int row = 0; row < image.rows; ++row) {
        for (int col = 0; col < image.cols; ++col) {
            state.index_map(row, col) = row * image.cols + col;
        }
    }
    return state;
}

// Swaps rows and columns of the image and of its index map. The VALUES of the index map
// do not change, so they still point at the right original pixels.
void transpose_tracked(TrackedImage& state) {
    state.image = transpose_image(state.image);
    state.index_map = cv::Mat_<int>(transpose_image(state.index_map));
}

// Original-image positions (x = column, y = row) of the pixels in the given column of
// each row. Pixels created by an earlier insertion are skipped.
// Time O(rows).
std::vector<cv::Point> original_pixels_at(const TrackedImage& state, const Seam& cols) {
    std::vector<cv::Point> pixels;
    for (int row = 0; row < state.index_map.rows; ++row) {
        const int encoded = state.index_map(row, cols[row]);
        if (encoded != kInsertedPixel) {
            pixels.emplace_back(encoded % state.original_cols, encoded / state.original_cols);
        }
    }
    return pixels;
}

// Calls the step callback (if there is one). The seam code works on a transposed image
// for horizontal seams, but the callback always gets the picture the right way up.
void notify_step(const StepCallback& on_step, const cv::Mat& image, const SeamRecord& record) {
    if (!on_step) {
        return;
    }
    if (record.direction == Direction::Horizontal) {
        on_step(transpose_image(image), record);
    } else {
        on_step(image, record);
    }
}

// Copy of `matrix` with one extra column: in each row the entries from column
// new_cols[row] onwards are shifted one step right. The entry AT new_cols[row] is left
// for the caller to fill in.
// Time O(rows * cols), space O(rows * cols).
cv::Mat widen_by_one(const cv::Mat& matrix, const Seam& new_cols) {
    cv::Mat wider(matrix.rows, matrix.cols + 1, matrix.type());
    for (int row = 0; row < matrix.rows; ++row) {
        const int new_col = new_cols[row];
        if (new_col < 1 || new_col > matrix.cols) {
            throw std::invalid_argument("insert_vertical_seam: new column out of range");
        }
        // Entries left of the new pixel keep their column.
        matrix.row(row).colRange(0, new_col).copyTo(wider.row(row).colRange(0, new_col));
        // Entries from the new pixel's position onwards shift one column to the right.
        if (new_col < matrix.cols) {
            matrix.row(row)
                .colRange(new_col, matrix.cols)
                .copyTo(wider.row(row).colRange(new_col + 1, wider.cols));
        }
    }
    return wider;
}

// The index map after an insertion: the new entries are marked kInsertedPixel.
cv::Mat_<int> insert_marker_seam(const cv::Mat_<int>& index_map, const Seam& new_cols) {
    cv::Mat_<int> wider(widen_by_one(index_map, new_cols));
    for (int row = 0; row < wider.rows; ++row) {
        wider(row, new_cols[row]) = kInsertedPixel;
    }
    return wider;
}

// Removes `count` vertical seams from the tracked image and returns one record per seam.
// `direction` says what these seams are in the upright image: the caller transposes the
// image first when it wants horizontal seams, so here they always run top to bottom.
// Time: count * (one seam search + O(rows * cols) for the removal).
std::vector<SeamRecord> remove_vertical_seams_tracked(TrackedImage& state, int count,
                                                      Direction direction,
                                                      const CarveOptions& options,
                                                      const StepCallback& on_step) {
    if (count < 0 || count >= state.image.cols) {
        throw std::invalid_argument("remove_seams: count must be in [0, size - 1]");
    }
    std::vector<SeamRecord> records;
    for (int removed = 0; removed < count; ++removed) {
        // The energy map is recomputed from the current image inside find_vertical_seam,
        // so every seam is chosen on up-to-date energies (spec item 1c).
        SeamRecord record;
        record.direction = direction;
        record.seam = find_vertical_seam(state.image, options, &record.cost);
        record.original_pixels = original_pixels_at(state, record.seam);
        notify_step(on_step, state.image, record);

        // Remove the seam from the image AND from the index map, so they stay in step.
        state.image = remove_vertical_seam(state.image, record.seam);
        state.index_map = cv::Mat_<int>(remove_vertical_seam(state.index_map, record.seam));
        records.push_back(record);
    }
    return records;
}

// Column in which the value `wanted` is stored in the given row of `map`.
int column_of_value(const cv::Mat_<int>& map, int row, int wanted) {
    for (int col = 0; col < map.cols; ++col) {
        if (map(row, col) == wanted) {
            return col;
        }
    }
    throw std::logic_error("column_of_value: value not found");
}

// Inserts `count` vertical seams into the tracked image and returns one record per seam.
// Time: finding the seams costs the same as removing `count` seams; each insertion is
// O(rows * cols).
std::vector<SeamRecord> insert_vertical_seams_tracked(TrackedImage& state, int count,
                                                      Direction direction,
                                                      const CarveOptions& options,
                                                      const StepCallback& on_step) {
    // Three maps are in play here, each with its own frame of reference:
    //   scratch.index_map : for the shrinking scratch copy, where each pixel was in the
    //                       image as it is NOW, before any insertion;
    //   position_map      : for the growing image, which of those pre-insertion pixels
    //                       sits at each place (new pixels are marked kInsertedPixel);
    //   state.index_map   : for the growing image, where each pixel was in the ORIGINAL
    //                       image (this is what the seam overlay needs).
    // column_of_value scans a row of position_map, so finding one seam's pixels costs
    // O(rows * cols); each insertion is therefore O(rows * cols) in total, the same order
    // as a removal.
    //
    // Step 1: find the `count` cheapest seams by removing them from a scratch copy.
    // Removing them one after another guarantees that no two seams share a pixel.
    // The scratch copy tracks positions in the image as it is NOW (before inserting),
    // so each record tells us which of the current pixels its seam goes through.
    TrackedImage scratch = make_tracked(state.image);
    std::vector<SeamRecord> records =
        remove_vertical_seams_tracked(scratch, count, direction, options, nullptr);

    // Step 2: insert one new pixel to the right of every seam pixel, seam by seam.
    // The image grows while we do this, so `position_map` remembers where each of the
    // pixels we started with is now (same encoding as an index map).
    const int start_cols = state.image.cols;
    cv::Mat_<int> position_map = make_tracked(state.image).index_map;

    for (SeamRecord& record : records) {
        Seam seam_cols(state.image.rows);  // where the seam pixel is in the grown image
        Seam new_cols(state.image.rows);   // where the new pixel goes: just right of it
        for (int row = 0; row < state.image.rows; ++row) {
            const int start_col = record.original_pixels[row].x;
            seam_cols[row] = column_of_value(position_map, row, row * start_cols + start_col);
            new_cols[row] = seam_cols[row] + 1;
        }

        record.inserted = true;
        record.seam = new_cols;
        record.original_pixels = original_pixels_at(state, seam_cols);

        state.image = insert_vertical_seam(state.image, new_cols);
        state.index_map = insert_marker_seam(state.index_map, new_cols);
        position_map = insert_marker_seam(position_map, new_cols);
        notify_step(on_step, state.image, record);
    }
    return records;
}

// Changes the size of the tracked image by `delta` pixels in one direction:
// delta < 0 removes seams, delta > 0 inserts seams, delta == 0 does nothing.
// Horizontal seams are handled by transposing, working on vertical seams, and
// transposing back.
std::vector<SeamRecord> resize_tracked(TrackedImage& state, int delta, Direction direction,
                                       const CarveOptions& options, const StepCallback& on_step) {
    std::vector<SeamRecord> records;
    if (delta == 0) {
        return records;
    }
    const bool horizontal = (direction == Direction::Horizontal);
    if (horizontal) {
        transpose_tracked(state);
    }
    if (delta < 0) {
        records = remove_vertical_seams_tracked(state, -delta, direction, options, on_step);
    } else {
        records = insert_vertical_seams_tracked(state, delta, direction, options, on_step);
    }
    if (horizontal) {
        transpose_tracked(state);
    }
    return records;
}

// Appends the records of one stage to the result and adds up their costs.
void append_records(CarveResult& result, const std::vector<SeamRecord>& records) {
    for (const SeamRecord& record : records) {
        result.seams.push_back(record);
        result.total_cost += record.cost;
    }
}

}  // namespace

// Finds one vertical seam with the chosen algorithm and energy (see carve.hpp).
// Time: O(rows * cols) for gray + energy, plus the chosen seam finder.
Seam find_vertical_seam(const cv::Mat& image, const CarveOptions& options, long long* cost) {
    const cv::Mat_<uchar> gray = to_grayscale(image);

    // Forward energy is the one mode that is not a per-pixel map; only the DP supports it.
    if (options.energy_mode == EnergyMode::Forward) {
        if (options.algorithm != Algorithm::DynamicProgramming) {
            throw std::invalid_argument(
                "forward energy is only implemented for the DP seam finder; "
                "use the 'neighbour' energy with greedy or graphcut");
        }
        const Seam seam = find_vertical_seam_dp_forward(gray);
        if (cost != nullptr) {
            *cost = forward_seam_cost(gray, seam);
        }
        return seam;
    }

    const EnergyMap energy = compute_energy(gray, options.energy_mode);
    Seam seam;
    switch (options.algorithm) {
        case Algorithm::DynamicProgramming:
            seam = find_vertical_seam_dp(energy);
            break;
        case Algorithm::Greedy:
            seam = find_vertical_seam_greedy(energy);
            break;
        case Algorithm::GraphCut:
            seam = find_vertical_seam_graphcut(energy);
            break;
    }
    if (cost != nullptr) {
        *cost = seam_cost(energy, seam);
    }
    return seam;
}

// Removes one pixel per row; the pixels right of it shift left (see carve.hpp).
// Time O(rows * cols), space O(rows * cols) for the new image.
cv::Mat remove_vertical_seam(const cv::Mat& image, const Seam& seam) {
    if (!is_valid_seam(seam, image.rows, image.cols)) {
        throw std::invalid_argument("remove_vertical_seam: seam is not valid for this image");
    }
    if (image.cols <= 1) {
        throw std::invalid_argument("remove_vertical_seam: image is only one column wide");
    }

    const int new_cols = image.cols - 1;
    cv::Mat output(image.rows, new_cols, image.type());
    for (int row = 0; row < image.rows; ++row) {
        const int seam_col = seam[row];
        // Pixels left of the seam keep their column.
        if (seam_col > 0) {
            image.row(row).colRange(0, seam_col).copyTo(output.row(row).colRange(0, seam_col));
        }
        // Pixels right of the seam shift one column to the left.
        if (seam_col < new_cols) {
            image.row(row)
                .colRange(seam_col + 1, image.cols)
                .copyTo(output.row(row).colRange(seam_col, new_cols));
        }
    }
    return output;
}

// Swaps rows and columns, so horizontal seams become vertical ones.
// Time O(rows * cols), space O(rows * cols).
cv::Mat transpose_image(const cv::Mat& image) {
    cv::Mat transposed;
    cv::transpose(image, transposed);
    return transposed;
}

// One seam in either direction: a horizontal seam is a vertical seam of the transpose.
Seam find_seam(const cv::Mat& image, Direction direction, const CarveOptions& options,
               long long* cost) {
    if (direction == Direction::Vertical) {
        return find_vertical_seam(image, options, cost);
    }
    return find_vertical_seam(transpose_image(image), options, cost);
}

// Removes one seam in either direction (horizontal: transpose, remove, transpose back).
// Time O(rows * cols).
cv::Mat remove_seam(const cv::Mat& image, const Seam& seam, Direction direction) {
    if (direction == Direction::Vertical) {
        return remove_vertical_seam(image, seam);
    }
    return transpose_image(remove_vertical_seam(transpose_image(image), seam));
}

// Removes `count` seams in one direction (see carve.hpp).
// Time: count * (one seam search + O(rows * cols)).
CarveResult remove_seams(const cv::Mat& image, int count, Direction direction,
                         const CarveOptions& options, const StepCallback& on_step) {
    const int available = (direction == Direction::Vertical) ? image.cols : image.rows;
    if (count < 0 || count >= available) {
        throw std::invalid_argument("remove_seams: count must be in [0, size - 1]");
    }
    TrackedImage state = make_tracked(image);
    CarveResult result;
    append_records(result, resize_tracked(state, -count, direction, options, on_step));
    result.image = state.image;
    return result;
}

// Makes the image one column wider by inserting one new pixel per row (see carve.hpp).
// Time O(rows * cols).
cv::Mat insert_vertical_seam(const cv::Mat& image, const Seam& new_cols) {
    if (static_cast<int>(new_cols.size()) != image.rows) {
        throw std::invalid_argument("insert_vertical_seam: need one column per row");
    }
    cv::Mat wider = widen_by_one(image, new_cols);
    for (int row = 0; row < wider.rows; ++row) {
        const int new_col = new_cols[row];
        // The new pixel is the average of its two neighbours, which hides the
        // duplication better than an exact copy. At the right border there is no right
        // neighbour, so the left one is used twice (a plain copy).
        const int left = new_col - 1;
        const int right = (new_col + 1 < wider.cols) ? new_col + 1 : left;
        cv::Mat new_pixel = wider.row(row).col(new_col);
        cv::addWeighted(wider.row(row).col(left), 0.5, wider.row(row).col(right), 0.5, 0.0,
                        new_pixel);
    }
    return wider;
}

// Enlarges the image by `count` seams in one direction (see carve.hpp).
CarveResult insert_seams(const cv::Mat& image, int count, Direction direction,
                         const CarveOptions& options, const StepCallback& on_step) {
    const int available = (direction == Direction::Vertical) ? image.cols : image.rows;
    if (count < 0 || count >= available) {
        throw std::invalid_argument("insert_seams: count must be in [0, size - 1]");
    }
    TrackedImage state = make_tracked(image);
    CarveResult result;
    append_records(result, resize_tracked(state, count, direction, options, on_step));
    result.image = state.image;
    return result;
}

// Resizes to an exact size: width first, then height (see carve.hpp).
// Time: the sum of the two stages.
CarveResult carve_to_size(const cv::Mat& image, int target_cols, int target_rows,
                          const CarveOptions& options, const StepCallback& on_step) {
    if (image.empty()) {
        throw std::invalid_argument("carve_to_size: image is empty");
    }
    if (target_cols < 1 || target_rows < 1) {
        throw std::invalid_argument("carve_to_size: target size must be at least 1 x 1");
    }
    if (target_cols >= 2 * image.cols || target_rows >= 2 * image.rows) {
        throw std::invalid_argument("carve_to_size: cannot enlarge to twice the size or more");
    }

    // One tracked image for the whole resize, so the seams of BOTH stages can be drawn
    // on the original image.
    TrackedImage state = make_tracked(image);
    CarveResult result;

    // Stage 1: width (vertical seams). Stage 2: height (horizontal seams).
    const int width_change = target_cols - state.image.cols;
    append_records(result,
                   resize_tracked(state, width_change, Direction::Vertical, options, on_step));
    const int height_change = target_rows - state.image.rows;
    append_records(result,
                   resize_tracked(state, height_change, Direction::Horizontal, options, on_step));

    result.image = state.image;
    return result;
}

}  // namespace seam
