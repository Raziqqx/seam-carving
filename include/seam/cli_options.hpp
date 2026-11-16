// cli_options.hpp - text <-> enum helpers shared by the command-line tool, the benchmark
// program and the interactive app.
#pragma once

#include <string>

#include "seam/types.hpp"

namespace seam {

// Parses "dp", "greedy" or "graphcut". Throws std::invalid_argument on anything else.
Algorithm parse_algorithm(const std::string& text);

// Parses "gradient", "neighbour" or "forward". Throws std::invalid_argument otherwise.
EnergyMode parse_energy_mode(const std::string& text);

}  // namespace seam
