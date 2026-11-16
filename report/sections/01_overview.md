# Overview

Seam carving (Avidan and Shamir, 2007) resizes an image by repeatedly removing a *seam*: a
connected path of pixels, one per row, that crosses the least important part of the picture.
Unlike scaling it does not squash the content, and unlike cropping it does not throw away
one side of the picture.

This report covers everything the assignment asks for:

| Assignment item | Where in this report | Where in the code |
|---|---|---|
| 1a energy map | [3.1](#energy-map-item-1a) | `src/energy.cpp` |
| 1b DP seam, vertical and horizontal | [3.2](#dynamic-programming-item-1b), [3.5](#remove-and-repeat-item-1c) | `src/dp.cpp`, `src/carve.cpp` |
| 1c remove, recompute, repeat | [3.5](#remove-and-repeat-item-1c) | `src/carve.cpp` |
| 2 questions (a-i, a-ii, b) | [2](#answers-to-item-2) | `tests/test_seam_count.cpp` |
| 3 greedy and comparison | [3.3](#greedy-item-3), [5](#results), [6.3](#comparison-of-the-three-methods-item-5c) | `src/greedy.cpp` |
| 4 graph cut and comparison | [3.4](#graph-cut-item-4), [5](#results), [6.3](#comparison-of-the-three-methods-item-5c) | `src/maxflow.cpp`, `src/graphcut.cpp` |
| 5 analysis and discussion | [6](#analysis-and-discussion) | `src/improvements.cpp`, `benchmarks/` |
| 7a step-by-step visualisation | [4.4](#step-by-step-visualisation-bonus-7a) | `src/visualize.cpp` |
| 7b interactive app | [4.5](#interactive-app-bonus-7b) | `src/app.cpp` |
| 7c CI/CD | [4.6](#continuous-integration-bonus-7c) | `.github/workflows/ci.yml` |

**What we built.** A C++17 library with three seam finders (dynamic programming, greedy and
a minimum cut solved by our own Edmonds-Karp max-flow), all of which work for vertical and
horizontal seams; seam insertion for enlarging; forward energy; a command-line tool; an
interactive app; a step-by-step animation; a test suite of @@test_cases@@ test cases
(@@test_assertions@@ checks); and a benchmark program that regenerates every number, table
and figure below. OpenCV is used only for reading and writing images, colour conversion,
the Sobel filter, plain scaling for the baselines, and video output. No content-aware
resizing function from any library is used.

**Main findings.**

- DP and graph cut both return a minimum-energy seam; in our tests their costs are equal on
  every random grid and equal to the max-flow value. Greedy is never cheaper and removes
  @@cost_ratio_min@@ to @@cost_ratio_max@@ times as much energy on our test images.
- Finding one seam in a @@size_dp_500@@ image takes @@seam_ms_dp_500@@ ms with DP and
  about @@gc_500_seconds@@ s with graph cut (Edmonds-Karp). Graph cut is correct but
  impractical for this problem.
- The graph exactly as worded in the brief can never remove a pixel of the last column. We
  explain why, test it, and give the one-line fix that makes the cut equal the DP optimum
  ([3.4](#graph-cut-item-4)).
- For item 5d: the DP does not need its whole table (rolling rows cut table memory by
  @@mem_ratio@@x); the energy map parallelises well but the DP table does not; and updating
  only the energies near the removed seam makes the energy step @@inc_energy_speedup@@x
  faster. (These two speed-ups are measured against our own serial Sobel loop, not against
  OpenCV's optimised filter; see [6.4](#improvements-item-5d).)

**Notation.** An image has $m$ rows and $n$ columns. Pixels are addressed as $(i, j)$ with
row $i$ and column $j$, both starting at 0 in the code (the brief counts from 1). $e[i][j]$
is the energy of a pixel, $k$ is the number of seams removed, and $N = mn$ is the number of
pixels.
