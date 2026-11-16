// types.hpp - small shared types used by every module.
//
// Conventions used in the whole project:
//   * An image has `rows` (height, m in the report) and `cols` (width, n in the report).
//   * A pixel is addressed as (row, col), row 0 is the top, col 0 is the left.
//   * A VERTICAL seam runs from the top row to the bottom row and holds one pixel per row.
//   * A HORIZONTAL seam runs from the left column to the right column, one pixel per column.
//     We never write separate horizontal code: we transpose the image, find a vertical
//     seam, and transpose back (see carve.cpp).
#pragma once

#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace seam {

// Energy map: one integer per pixel. energy(row, col) is the cost of removing that pixel.
// We use integers (not floats) so that seam costs can be compared exactly in the tests
// and so that max-flow capacities are exact.
using EnergyMap = cv::Mat_<int>;

// A vertical seam: seam[row] is the column of the pixel to remove in that row.
// Valid seam: seam.size() == rows, 0 <= seam[row] < cols, |seam[row] - seam[row-1]| <= 1.
using Seam = std::vector<int>;

// Which way the seam runs.
enum class Direction {
    Vertical,   // top to bottom, removing it makes the image one column narrower
    Horizontal  // left to right, removing it makes the image one row shorter
};

// Which seam-finding algorithm to use.
enum class Algorithm {
    DynamicProgramming,  // optimal, O(rows * cols) per seam
    Greedy,              // local row-by-row choice, not optimal
    GraphCut             // optimal, min s-t cut solved with our own Edmonds-Karp
};

// Which per-pixel cost ("energy") the seam finder minimises.
enum class EnergyMode {
    Gradient,             // Sobel gradient magnitude |Gx| + |Gy| (backward energy)
    NeighbourDifference,  // |I[i, j+1] - I[i, j-1]|, the simple forward cost from spec item 4
    Forward               // full forward energy of Rubinstein et al. 2008 (DP only)
};

// Thrown by a function whose real implementation belongs to a milestone that has not been
// merged yet. The project was split into six milestones; until all of them are merged,
// some functions are placeholders that throw this. Programs report it as "not implemented
// yet", and tests that need such a function are skipped instead of failing.
class NotImplementedYet : public std::logic_error {
public:
    // `what_is_missing` names the feature and the milestone that brings it, for example
    // "greedy seam finder (milestone 2, Wong Shi Yu Regine)".
    explicit NotImplementedYet(const std::string& what_is_missing);
};

// Sum of the energies of the pixels on a vertical seam.
// Time O(rows), extra space O(1).
long long seam_cost(const EnergyMap& energy, const Seam& seam);

// True if `seam` is a valid vertical seam for an image with the given size:
// one entry per row, every column in bounds, neighbouring rows differ by at most one column.
// Time O(rows), extra space O(1).
bool is_valid_seam(const Seam& seam, int rows, int cols);

// Text names used by the command-line tools and the benchmark CSV files.
std::string to_string(Algorithm algorithm);
std::string to_string(EnergyMode mode);
std::string to_string(Direction direction);

}  // namespace seam
