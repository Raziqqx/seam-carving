// greedy.hpp - greedy seam finding (spec item 3).
#pragma once

#include "seam/types.hpp"

namespace seam {

// Greedy vertical seam: start at the minimum-energy pixel of the top row, then in every
// following row move to the cheapest of the (up to three) pixels below the current one.
// The choice is local: it never looks further than one row ahead, so the seam is valid
// but NOT guaranteed to be the cheapest one (see tests/test_greedy.cpp for a grid where
// it is strictly worse than DP).
// Ties: the leftmost minimum in the top row; afterwards straight down, then down-left,
// then down-right.
// Time O(cols + rows): one scan of the top row, then 3 comparisons per row.
// Space O(rows) for the seam; no table is needed.
Seam find_vertical_seam_greedy(const EnergyMap& energy);

}  // namespace seam
