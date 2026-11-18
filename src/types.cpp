#include "seam/types.hpp"

#include <cstdlib>

namespace seam {

long long seam_cost(const EnergyMap& energy, const Seam& seam) {
    long long total = 0;
    for (int row = 0; row < energy.rows; ++row) {
        total += energy(row, seam[row]);
    }
    return total;
}

bool is_valid_seam(const Seam& seam, int rows, int cols) {
    if (static_cast<int>(seam.size()) != rows) {
        return false;
    }
    for (int row = 0; row < rows; ++row) {
        const bool in_bounds = seam[row] >= 0 && seam[row] < cols;
        if (!in_bounds) {
            return false;
        }
        // Connectivity: the pixel in this row must touch the pixel in the row above
        // (directly below it, or diagonally below it).
        if (row > 0 && std::abs(seam[row] - seam[row - 1]) > 1) {
            return false;
        }
    }
    return true;
}

std::string to_string(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::DynamicProgramming:
            return "dp";
        case Algorithm::Greedy:
            return "greedy";
        case Algorithm::GraphCut:
            return "graphcut";
    }
    return "unknown";
}

std::string to_string(EnergyMode mode) {
    switch (mode) {
        case EnergyMode::Gradient:
            return "gradient";
        case EnergyMode::NeighbourDifference:
            return "neighbour";
        case EnergyMode::Forward:
            return "forward";
    }
    return "unknown";
}

std::string to_string(Direction direction) {
    switch (direction) {
        case Direction::Vertical:
            return "vertical";
        case Direction::Horizontal:
            return "horizontal";
    }
    return "unknown";
}

}  // namespace seam
