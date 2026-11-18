#include "test_support.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace test_support {

seam::EnergyMap random_energy(int rows, int cols, int max_value, unsigned seed) {
    std::mt19937 generator(seed);
    std::uniform_int_distribution<int> pick(0, max_value);
    seam::EnergyMap energy(rows, cols);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            energy(row, col) = pick(generator);
        }
    }
    return energy;
}

cv::Mat_<uchar> random_gray(int rows, int cols, unsigned seed) {
    std::mt19937 generator(seed);
    std::uniform_int_distribution<int> pick(0, 255);
    cv::Mat_<uchar> gray(rows, cols);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            gray(row, col) = static_cast<uchar>(pick(generator));
        }
    }
    return gray;
}

cv::Mat random_colour(int rows, int cols, unsigned seed) {
    std::mt19937 generator(seed);
    std::uniform_int_distribution<int> pick(0, 255);
    cv::Mat colour(rows, cols, CV_8UC3);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            colour.at<cv::Vec3b>(row, col) =
                cv::Vec3b(static_cast<uchar>(pick(generator)), static_cast<uchar>(pick(generator)),
                          static_cast<uchar>(pick(generator)));
        }
    }
    return colour;
}

seam::EnergyMap make_energy(const std::vector<std::vector<int>>& rows) {
    const int row_count = static_cast<int>(rows.size());
    const int col_count = static_cast<int>(rows.front().size());
    seam::EnergyMap energy(row_count, col_count);
    for (int row = 0; row < row_count; ++row) {
        for (int col = 0; col < col_count; ++col) {
            energy(row, col) = rows[row][col];
        }
    }
    return energy;
}

namespace {

// Extends the partial seam (rows 0 .. partial.size()-1 already chosen) in every possible
// way and appends each finished seam to `output`.
void extend_seam(seam::Seam& partial, int rows, int cols, std::vector<seam::Seam>& output) {
    if (static_cast<int>(partial.size()) == rows) {
        output.push_back(partial);
        return;
    }
    const int first = partial.empty() ? 0 : std::max(partial.back() - 1, 0);
    const int last = partial.empty() ? cols - 1 : std::min(partial.back() + 1, cols - 1);
    for (int col = first; col <= last; ++col) {
        partial.push_back(col);
        extend_seam(partial, rows, cols, output);
        partial.pop_back();
    }
}

}  // namespace

std::vector<seam::Seam> all_seams(int rows, int cols) {
    std::vector<seam::Seam> seams;
    seam::Seam partial;
    extend_seam(partial, rows, cols, seams);
    return seams;
}

long long brute_force_min_cost(const seam::EnergyMap& energy) {
    long long best = std::numeric_limits<long long>::max();
    for (const seam::Seam& candidate : all_seams(energy.rows, energy.cols)) {
        best = std::min(best, seam::seam_cost(energy, candidate));
    }
    return best;
}

}  // namespace test_support
