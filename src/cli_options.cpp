#include "seam/cli_options.hpp"

#include <stdexcept>

namespace seam {

Algorithm parse_algorithm(const std::string& text) {
    if (text == "dp") {
        return Algorithm::DynamicProgramming;
    }
    if (text == "greedy") {
        return Algorithm::Greedy;
    }
    if (text == "graphcut") {
        return Algorithm::GraphCut;
    }
    throw std::invalid_argument("unknown algorithm '" + text + "' (use dp, greedy or graphcut)");
}

EnergyMode parse_energy_mode(const std::string& text) {
    if (text == "gradient") {
        return EnergyMode::Gradient;
    }
    if (text == "neighbour") {
        return EnergyMode::NeighbourDifference;
    }
    if (text == "forward") {
        return EnergyMode::Forward;
    }
    throw std::invalid_argument("unknown energy '" + text +
                                "' (use gradient, neighbour or forward)");
}

}  // namespace seam
