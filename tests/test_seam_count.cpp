// Counting check for report item 2a-i: the number of seams grows exponentially in m.
//
// Report claims, for an m x n image (m rows, n >= 2 columns), with S(m, n) = #seams:
//     n * 2^(m-1)  <=  S(m, n)  <=  n * 3^(m-1)
//     S(m, 2) = 2^m exactly
//     S(m, n) is given by the recurrence  ways[i][j] = ways[i-1][j-1] + ways[i-1][j] +
//     ways[i-1][j+1]
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include "test_support.hpp"

namespace {

// Number of seams by the counting recurrence (same shape as the DP, with + instead of min).
long long count_seams_by_recurrence(int rows, int cols) {
    std::vector<long long> ways(cols, 1);  // one seam ends at each pixel of the top row
    for (int row = 1; row < rows; ++row) {
        std::vector<long long> next(cols, 0);
        for (int col = 0; col < cols; ++col) {
            next[col] = ways[col];
            if (col - 1 >= 0) {
                next[col] += ways[col - 1];
            }
            if (col + 1 < cols) {
                next[col] += ways[col + 1];
            }
        }
        ways = next;
    }
    long long total = 0;
    for (long long value : ways) {
        total += value;
    }
    return total;
}

long long power(long long base, int exponent) {
    long long result = 1;
    for (int step = 0; step < exponent; ++step) {
        result *= base;
    }
    return result;
}

}  // namespace

TEST_CASE("seam count: brute-force enumeration matches the recurrence", "[count]") {
    for (int rows = 1; rows <= 7; ++rows) {
        for (int cols = 1; cols <= 6; ++cols) {
            const long long enumerated =
                static_cast<long long>(test_support::all_seams(rows, cols).size());
            INFO("size " << rows << " x " << cols);
            CHECK(enumerated == count_seams_by_recurrence(rows, cols));
        }
    }
}

TEST_CASE("seam count: every enumerated seam is valid and distinct", "[count]") {
    const std::vector<seam::Seam> seams = test_support::all_seams(5, 4);
    for (size_t first = 0; first < seams.size(); ++first) {
        REQUIRE(seam::is_valid_seam(seams[first], 5, 4));
        for (size_t second = first + 1; second < seams.size(); ++second) {
            REQUIRE(seams[first] != seams[second]);
        }
    }
}

TEST_CASE("seam count: n * 2^(m-1) <= count <= n * 3^(m-1) for n >= 2", "[count]") {
    for (int rows = 1; rows <= 7; ++rows) {
        for (int cols = 2; cols <= 6; ++cols) {
            const long long count =
                static_cast<long long>(test_support::all_seams(rows, cols).size());
            INFO("size " << rows << " x " << cols << ", count " << count);
            CHECK(count >= cols * power(2, rows - 1));
            CHECK(count <= cols * power(3, rows - 1));
        }
    }
}

TEST_CASE("seam count: exactly 2^m seams when n = 2, exactly 1 when n = 1", "[count]") {
    for (int rows = 1; rows <= 10; ++rows) {
        CHECK(static_cast<long long>(test_support::all_seams(rows, 2).size()) == power(2, rows));
        CHECK(test_support::all_seams(rows, 1).size() == 1);
    }
}

TEST_CASE("seam count: known small values", "[count]") {
    CHECK(test_support::all_seams(1, 5).size() == 5);
    CHECK(test_support::all_seams(2, 3).size() == 7);    // 2 + 3 + 2
    CHECK(test_support::all_seams(3, 3).size() == 17);   // 5 + 7 + 5
    CHECK(count_seams_by_recurrence(20, 2) == 1048576);  // 2^20: far too many to brute force
}

TEST_CASE("seam count: growth rate for n = 3 is 1 + sqrt(2)", "[count]") {
    // Report, item 2a-i: S(m, n) grows like (1 + 2 cos(pi / (n + 1)))^m. For n = 3 that
    // base is 1 + sqrt(2) = 2.414..., and the exact counts 3, 7, 17, 41, 99, ... obey
    // S(m) = 2 S(m-1) + S(m-2), whose growth rate is exactly 1 + sqrt(2).
    for (int rows = 3; rows <= 9; ++rows) {
        const long long count = static_cast<long long>(test_support::all_seams(rows, 3).size());
        const long long previous = count_seams_by_recurrence(rows - 1, 3);
        const long long before_previous = count_seams_by_recurrence(rows - 2, 3);
        CHECK(count == 2 * previous + before_previous);
    }
    const double ratio = static_cast<double>(count_seams_by_recurrence(30, 3)) /
                         static_cast<double>(count_seams_by_recurrence(29, 3));
    CHECK(std::abs(ratio - (1.0 + std::sqrt(2.0))) < 1e-9);
}
