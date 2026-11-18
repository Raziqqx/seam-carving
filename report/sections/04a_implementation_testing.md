# Implementation and testing

## Code structure

The code is C++17, built with CMake, in one small library plus three programs.

| File | Contents | Key functions |
|---|---|---|
| `include/seam/types.hpp`, `src/types.cpp` | Shared types and checks | `EnergyMap`, `Seam`, `seam_cost`, `is_valid_seam` |
| `src/energy.cpp` | Energy maps (1a) | `compute_gradient_energy`, `sobel_energy_at`, `compute_neighbour_difference_energy` |
| `src/dp.cpp` | Dynamic programming (1b) | `best_parent_col`, `build_cumulative_energy`, `backtrack_seam`, `find_vertical_seam_dp`, `find_vertical_seam_dp_rolling`, `find_vertical_seam_dp_forward` |
| `src/greedy.cpp` | Greedy (3) | `find_vertical_seam_greedy` |
| `src/maxflow.cpp` | Our Edmonds-Karp (4) | `FlowNetwork::add_edge`, `max_flow`, `reachable_from` |
| `src/graphcut.cpp` | Seam graph and min-cut seam (4) | `list_seam_graph_edges`, `extract_seam_from_cut`, `find_vertical_seam_graphcut` |
| `src/carve.cpp` | Remove / insert loop (1c) | `remove_vertical_seam`, `insert_vertical_seam`, `transpose_image`, `remove_seams`, `insert_seams`, `carve_to_size` |
| `src/improvements.cpp` | Item 5d experiments | `compute_gradient_energy_parallel`, `build_cumulative_energy_parallel`, `update_gradient_energy_after_removal` |
| `src/visualize.cpp` | Bonus 7a | `draw_all_seams`, `AnimationRecorder`, `write_video` |
| `src/main.cpp` | Command-line tool `seamcarve` | |
| `src/app.cpp` | Bonus 7b interactive app `seamcarve_app` | |
| `benchmarks/bench.cpp` | Benchmark program `seam_bench` | |

**Programming practice.**

- *Readable first.* Every core function has a comment stating its purpose, inputs, outputs
  and time and space complexity, and the comments inside explain *why* (why three parents,
  why backward edges are infinite). There are no raw `new`/`delete`, no pointer
  arithmetic and no hand-written SIMD; data lives in `std::vector` and `cv::Mat`.
- *Optimised variants sit next to the plain ones.* The rolling-row DP, the OpenMP
  functions and the incremental energy update are separate functions, and tests check
  that each returns exactly what its plain counterpart returns.
- *Integer energies.* All costs are integers, so tests compare costs with `==` and max-flow
  capacities are exact. Cumulative costs fit in a 32-bit `int` for any image with fewer
  than about one million rows ($2040 \cdot 10^6 < 2^{31}$).
- *Errors are reported, not ignored.* Empty images, invalid seams, impossible target sizes
  and unsupported combinations (forward energy with greedy or graph cut) throw
  `std::invalid_argument` with a message; tests cover each case.
- *Zero warnings.* The build uses `-Wall -Wextra -O2`; continuous integration adds
  `-Werror`, so a warning fails the build. Formatting is fixed by a committed
  `.clang-format` and checked in CI. CI builds and tests twice, with and without OpenMP,
  so OpenMP stays an optional speed-up; if a test fails, the full test log is kept.

## Testing strategy

The rubric asks for proof of detailed testing of all cases. Our approach is to test each
algorithm against an *independent* way of getting the same answer, on many random inputs,
and then to add hand-built edge cases. The suite uses Catch2 and has **@@test_cases@@ test
cases with @@test_assertions@@ individual checks, all passing**, plus one end-to-end test of
the command-line tool.

| What is checked | How | File |
|---|---|---|
| Energy is correct | Hand-computed Sobel values; step edge gives 1020; our loop equals OpenCV on 50 random images; border replication | `test_energy.cpp` |
| DP is optimal | Cost equals brute force over **all** seams on 540 random grids ($1 \times 1$ to $6 \times 6$); hand-worked $4 \times 4$ table and parents | `test_dp.cpp` |
| Forward-energy DP is optimal | Cost equals brute force over all seams on 250 random images | `test_dp.cpp` |
| Greedy is valid, never beats DP, and can be worse | 500 random grids; the hand-built $3 \times 3$ grid (19 against 4); optimal when a zero-energy column holds the top row's leftmost minimum; a hand-checked 2-column grid where it matches DP; how often it is optimal on 200 random grids of each of three sizes (sometimes, but not always) | `test_greedy.cpp` |
| Max-flow is correct | Textbook networks with known answers (CLRS Figure 26.1 = 23), a case that needs a reverse edge, zero and infinite capacities | `test_maxflow.cpp` |
| Graph cut is optimal | Seam cost = DP cost = max-flow value on 768 random grids, plus $25 \times 30$ grids and brute force | `test_graphcut.cpp` |
| Graph structure | Node and edge counts; every edge of a $2 \times 2$ graph; source side is a prefix of each row; leftmost optimal seam | `test_graphcut.cpp` |
| Max-flow traced by hand | A $2 \times 2$ max flow traced by hand; the $2 \times 3$ worked example of [3.4](#graph-cut-item-4); the number of augmentations is at most the flow value | `test_graphcut.cpp` |
| The brief's literal graph | Equals DP on the image without its last column (420 grids); the $3 \times 3$ counter-example | `test_graphcut.cpp` |
| Our max-flow against a library | Same value as Boost's `edmonds_karp_max_flow` on 120 seam graphs and 100 random graphs (optional, test only) | `test_graphcut_boost.cpp` |
| Seam removal and shapes | Exact pixels after removal; output is exactly $(m, n-k)$ or $(m-k, n)$; energy is recomputed every step | `test_carve.cpp` |
| Horizontal seams | `horizontal == transpose(vertical(transpose))` for all three methods | `test_carve.cpp` |
| Seam insertion | Size grows exactly; every original row is preserved in order; constant image stays constant; `insert_vertical_seam` undoes `remove_vertical_seam`; shrinking one way while enlarging the other reports duplicated pixels in original coordinates | `test_carve.cpp` |
| Seam counting (item 2a-i) | Brute-force count equals the recurrence and lies within the proven bounds | `test_seam_count.cpp` |
| Edge cases, all three methods | $1 \times 1$; $2 \times 2$ (all 4 seams, brute force); one column ($n = 1$); $n = 2$; one row; constant image (all ties); gray against colour input; 4-channel BGRA input keeps 4 channels and its alpha when shrinking and enlarging; carving down to $1 \times 1$; $40 \times 3$ and $3 \times 40$ images; no overflow at the maximum energy | `test_edge_cases.cpp` |
| A column every horizontal seam must cross | Column of cost 1000 with one gap of 100: DP and graph cut cross at the gap (cost 100); greedy cannot see the gap in time and pays 1000 | `test_edge_cases.cpp` |
| Optimised variants | Rolling-row DP = full DP (200 grids with many ties); OpenMP energy = serial; row-parallel DP = plain DP; incremental energy = full recomputation after every one of the seams, for seams on the left and right border, and for every possible seam of a 1-row and a 2-column image | `test_dp.cpp`, `test_improvements.cpp` |
| Visualisation | Exactly the seam pixels are painted; overlay marks each removed pixel once; one frame per seam, also when enlarging (frames as large as the target); `write_frames` keeps every $k$-th frame and always the last; `write_video` writes a small MP4 and refuses an empty frame list | `test_visualize.cpp` |
| Command-line tool | Runs every algorithm, forward energy, enlarging and frame export on a real image; `--help` exits 0; twelve bad command lines (unknown algorithm or option, missing output name or value, non-numeric width, width or height of 0 or below, a number with trailing characters such as `10abc`, too large width or height, missing input file) exit with exactly code 1 | `cli_end_to_end.cmake` |

**Why brute force.** For grids up to $6 \times 6$ we can list every seam (at most 950 of
them) and take the true minimum. This is slow but obviously correct, so agreement on
hundreds of random grids is strong evidence that the DP, including its edge-column handling
and tie-breaking, is right. Graph cut is then checked against DP on larger grids where
brute force is impossible.

**What review found.** An independent review of the first version found that recording an
animation while *enlarging* failed, because the frames had the size of the original image.
The fix (frames sized for the larger of original and target) came with a regression test, a
command-line test and an app self-test case, so the same mistake cannot return unnoticed.

**What the tests found.** The cross-check between graph cut and DP is what exposed the
last-column problem of the literal graph ([3.4](#graph-cut-item-4)). We kept both versions
and turned the discovery into tests. The command-line checks also found that `--width 0`
and negative widths were silently treated as "keep the current width"; the tool now rejects
them, like any `--width`, `--height` or `--frame-step` below 1 or with trailing characters
such as `10abc`, with exit code 1, and the end-to-end test checks this.

**Running the tests.** `cmake -S . -B build && cmake --build build -j && ctest --test-dir build`.
