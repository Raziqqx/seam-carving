#include "seam/energy.hpp"

#include <algorithm>
#include <cstdlib>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace seam {

// 8-bit grayscale copy of a gray, BGR or BGRA image. Time O(rows * cols).
cv::Mat_<uchar> to_grayscale(const cv::Mat& image) {
    if (image.empty()) {
        throw std::invalid_argument("to_grayscale: image is empty");
    }
    if (image.depth() != CV_8U) {
        throw std::invalid_argument("to_grayscale: only 8-bit images are supported");
    }
    cv::Mat gray;
    if (image.channels() == 1) {
        gray = image.clone();
    } else if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    } else {
        throw std::invalid_argument("to_grayscale: expected 1, 3 or 4 channels");
    }
    return gray;
}

// Boundary rule: an index outside [0, size - 1] is moved to the nearest valid index.
int clamp_index(int index, int size) {
    return std::min(std::max(index, 0), size - 1);
}

// |Gx| + |Gy| of the 3x3 Sobel kernels at one pixel. Time O(1).
int sobel_energy_at(const cv::Mat_<uchar>& gray, int row, int col) {
    // Row and column indices of the 3x3 neighbourhood, clamped at the image border.
    const int up = clamp_index(row - 1, gray.rows);
    const int down = clamp_index(row + 1, gray.rows);
    const int left = clamp_index(col - 1, gray.cols);
    const int right = clamp_index(col + 1, gray.cols);

    // Sobel X kernel:  -1 0 +1      Sobel Y kernel:  -1 -2 -1
    //                  -2 0 +2                        0  0  0
    //                  -1 0 +1                       +1 +2 +1
    const int gradient_x = (gray(up, right) + 2 * gray(row, right) + gray(down, right)) -
                           (gray(up, left) + 2 * gray(row, left) + gray(down, left));
    const int gradient_y = (gray(down, left) + 2 * gray(down, col) + gray(down, right)) -
                           (gray(up, left) + 2 * gray(up, col) + gray(up, right));

    // L1 gradient magnitude. It stays an exact integer (no square root needed).
    return std::abs(gradient_x) + std::abs(gradient_y);
}

// Energy map with OpenCV's Sobel filter (the default). Time O(rows * cols).
EnergyMap compute_gradient_energy(const cv::Mat_<uchar>& gray) {
    // 16-bit signed output: the Sobel response of 8-bit data lies in [-1020, 1020],
    // which does not fit in 8 bits.
    cv::Mat gradient_x;
    cv::Mat gradient_y;
    cv::Sobel(gray, gradient_x, CV_16S, 1, 0, 3, 1.0, 0.0, cv::BORDER_REPLICATE);
    cv::Sobel(gray, gradient_y, CV_16S, 0, 1, 3, 1.0, 0.0, cv::BORDER_REPLICATE);

    EnergyMap energy(gray.rows, gray.cols);
    for (int row = 0; row < gray.rows; ++row) {
        for (int col = 0; col < gray.cols; ++col) {
            energy(row, col) =
                std::abs(gradient_x.at<short>(row, col)) + std::abs(gradient_y.at<short>(row, col));
        }
    }
    return energy;
}

// The same energy map with our own loops. Time O(rows * cols), 9 reads per pixel.
EnergyMap compute_gradient_energy_manual(const cv::Mat_<uchar>& gray) {
    EnergyMap energy(gray.rows, gray.cols);
    for (int row = 0; row < gray.rows; ++row) {
        for (int col = 0; col < gray.cols; ++col) {
            energy(row, col) = sobel_energy_at(gray, row, col);
        }
    }
    return energy;
}

// |I[i][j+1] - I[i][j-1]| for every pixel (spec item 4). Time O(rows * cols).
EnergyMap compute_neighbour_difference_energy(const cv::Mat_<uchar>& gray) {
    EnergyMap energy(gray.rows, gray.cols);
    for (int row = 0; row < gray.rows; ++row) {
        for (int col = 0; col < gray.cols; ++col) {
            const int left = clamp_index(col - 1, gray.cols);
            const int right = clamp_index(col + 1, gray.cols);
            energy(row, col) = std::abs(gray(row, right) - gray(row, left));
        }
    }
    return energy;
}

// Chooses the per-pixel energy map for a mode.
EnergyMap compute_energy(const cv::Mat_<uchar>& gray, EnergyMode mode) {
    switch (mode) {
        case EnergyMode::Gradient:
            return compute_gradient_energy(gray);
        case EnergyMode::NeighbourDifference:
            return compute_neighbour_difference_energy(gray);
        case EnergyMode::Forward:
            break;
    }
    throw std::invalid_argument(
        "compute_energy: forward energy is not a per-pixel map, use find_vertical_seam_dp_forward");
}

}  // namespace seam
