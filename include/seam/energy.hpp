// energy.hpp - energy maps (spec item 1a).
//
// The energy of a pixel says how "important" it is: a high value means the pixel sits on
// an edge or a texture, a low value means it looks like its neighbours and can be removed
// without anyone noticing.
//
// Boundary handling (used by EVERY function in this file): pixels outside the image are
// treated as copies of the nearest border pixel ("replicate" / clamp-to-edge). So a
// neighbour index of -1 is read as 0, and an index of `cols` is read as `cols - 1`.
#pragma once

#include "seam/types.hpp"

namespace seam {

// Converts an 8-bit image with 1 (gray), 3 (BGR) or 4 (BGRA) channels to 8-bit grayscale.
// All energies are computed on this grayscale version.
// Time O(rows * cols), space O(rows * cols) for the result.
cv::Mat_<uchar> to_grayscale(const cv::Mat& image);

// Clamps `index` into [0, size - 1]. This is our boundary rule in one place.
int clamp_index(int index, int size);

// Sobel gradient energy of ONE pixel: |Gx| + |Gy|, where Gx and Gy are the responses of
// the 3x3 Sobel kernels. Reads the 8 neighbours with clamped indices.
// Time O(1) (9 pixel reads), space O(1).
int sobel_energy_at(const cv::Mat_<uchar>& gray, int row, int col);

// Gradient energy map using OpenCV's Sobel filter: energy = |Gx| + |Gy|.
// This is the default energy (e1 in Avidan & Shamir 2007).
// Time O(rows * cols), space O(rows * cols).
EnergyMap compute_gradient_energy(const cv::Mat_<uchar>& gray);

// The same energy map computed by our own double loop (no OpenCV filter).
// Kept next to the OpenCV version so the tests can check they agree exactly, and so the
// report can discuss the cost of the convolution (item 2b) on code we wrote ourselves.
// Time O(rows * cols * 9) = O(rows * cols), space O(rows * cols).
EnergyMap compute_gradient_energy_manual(const cv::Mat_<uchar>& gray);

// Neighbour-difference energy from spec item 4: cost(i, j) = |I[i, j+1] - I[i, j-1]|.
// Removing pixel (i, j) makes its left and right neighbours touch, so their difference
// is the new edge that the removal creates. This is the simple form of forward energy.
// Time O(rows * cols), space O(rows * cols).
EnergyMap compute_neighbour_difference_energy(const cv::Mat_<uchar>& gray);

// Picks one of the per-pixel energy maps above.
// EnergyMode::Forward is NOT a per-pixel map (its cost depends on the seam direction of
// travel), so it is handled inside the DP in dp.cpp; asking for it here throws.
EnergyMap compute_energy(const cv::Mat_<uchar>& gray, EnergyMode mode);

}  // namespace seam
