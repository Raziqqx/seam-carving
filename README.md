# Seam Carving: DP, Greedy and Graph Cut

[![CI](https://github.com/Raziqqx/seam-carving/actions/workflows/ci.yml/badge.svg)](https://github.com/Raziqqx/seam-carving/actions/workflows/ci.yml)

Content-aware image resizing (Avidan and Shamir, 2007) for Algorithms Assignment 2, Team 5.
Three seam finders are implemented from scratch and compared:

| Method | File | Optimal? | Time per seam |
|---|---|---|---|
| Dynamic programming | `src/dp.cpp` | yes | O(rows x cols) |
| Greedy | `src/greedy.cpp` | no | O(rows + cols) |
| Graph cut (own Edmonds-Karp max-flow) | `src/maxflow.cpp`, `src/graphcut.cpp` | yes | O(V E^2), slow |

All of them work for vertical and horizontal seams (horizontal = transpose, carve, transpose
back) and the energy map is recomputed after every removed seam. OpenCV is used only for
image I/O, colour conversion, the Sobel filter, plain scaling (baseline) and video output.
No library seam-carving or content-aware resizing function is used.

The report is [`report/report.pdf`](report/report.pdf); the slides are in [`slides/`](slides/).

## Repository layout

```
include/seam/   headers, one per module (start with types.hpp)
src/            energy, dp, greedy, maxflow, graphcut, carve, improvements, visualize,
                main.cpp (command-line tool), app.cpp (interactive app)
tests/          Catch2 tests, one file per module
benchmarks/     bench.cpp, run_all.sh, plot_results.py
data/inputs/    test images (sources and licences in SOURCES.md)
results/        benchmark CSV files and every output picture
report/         report sources (sections/*.md), figures, build script, report.pdf
slides/         presentation deck, speaker notes and demo script
.github/        CI workflow (ci.yml) and release workflow (release.yml)
```

## Milestones

The work was split into six milestones, one per team member. Milestone 1 (the scaffold) was
pushed to `main` directly; milestones 2 to 6 each lived on their own branch and were merged
into `main` through pull requests whose CI checks passed. All six are now merged.

| # | Owner | Branch | Contents |
|---|---|---|---|
| 1 | Muhammad Raziq | `main` | CMake, energy map, dynamic programming, carving loop, command-line tool, their tests |
| 2 | Wong Shi Yu Regine | `m2-greedy` | greedy seam finder and its tests |
| 3 | Tan Ying Clarissa | `m3-graphcut` | max-flow, graph-cut seam finder and their tests |
| 4 | Isaiah Low Jung | `m4-tests-ci` | edge-case, counting and end-to-end tests; full CI and release workflows |
| 5 | Yasmin Tazi | `m5-visual-app` | seam visualisation, animation and the interactive app |
| 6 | Kimberly Charlotte Madlangbayan Ambrosio | `m6-analysis-report` | benchmarks, results, item 5d experiments, analysis sections of the report |

## Requirements

- C++17 compiler (tested with g++ 13), CMake 3.16 or newer
- OpenCV 4, Catch2 v3
- Optional: Boost.Graph (one extra cross-check test), OpenMP (multithreading experiment),
  GLFW 3 and OpenGL (interactive app)

Ubuntu 24.04:

```bash
sudo apt-get install cmake g++ libopencv-dev catch2 libboost-graph-dev clang-format
sudo apt-get install libglfw3-dev libgl-dev      # only for the interactive app
```

macOS (Homebrew): `brew install cmake opencv catch2 boost glfw libomp`.
Windows: use WSL2 with Ubuntu and the commands above.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

This builds the library, the `seamcarve` tool, the tests and the benchmark program in
Release mode with `-O2 -Wall -Wextra`. Add `-DSEAM_WERROR=ON` to turn warnings into errors,
as CI does.

## Run the algorithms

```bash
# Dynamic programming: make the tower 150 pixels narrower
./build/seamcarve --algo dp --width 350 data/inputs/tower.png out_dp.png

# Greedy, same target
./build/seamcarve --algo greedy --width 350 data/inputs/tower.png out_greedy.png

# Graph cut (slow: use a small image or few seams)
./build/seamcarve --algo graphcut --width 140 data/inputs/tower_small.png out_gc.png

# Horizontal seams (reduce the height), both directions at once, and enlarging
./build/seamcarve --algo dp --height 240 data/inputs/tower.png out_short.png
./build/seamcarve --algo dp --width 400 --height 300 data/inputs/tower.png out_both.png
./build/seamcarve --algo dp --width 600 data/inputs/tower.png out_wide.png

# Forward energy (DP only)
./build/seamcarve --algo dp --energy forward --width 350 data/inputs/tower.png out_fwd.png
```

`./build/seamcarve --help` lists every option.

## Visualise the seams (bonus 7a)

```bash
./build/seamcarve --algo dp --width 350 \
    --seams-image seams.png --video steps.mp4 --frames frame --frame-step 10 \
    data/inputs/tower.png out.png
```

- `--seams-image`: the original with every removed seam drawn on it (red = vertical,
  green = horizontal)
- `--video`: step-by-step animation, one frame per seam
- `--frames`: the same animation as PNG files `frame_0000.png`, `frame_0010.png`, ...

## Interactive app (bonus 7b)

```bash
cmake -S . -B build-app -DSEAM_BUILD_APP=ON
cmake --build build-app -j --target seamcarve_app
./build-app/seamcarve_app data/inputs/tower.png
```

Drag the width and height sliders, choose the algorithm and energy, and switch the view
between the result, all seams, the energy map and a step-by-step replay. **Save result**
writes `result.png`; **Save seams picture** writes `seams.png` (all seams on the original).
Dear ImGui v1.90.4
is downloaded by CMake on the first configure. `seamcarve_app --selftest <image>` checks the
app's logic without opening a window.

## Tests

```bash
ctest --test-dir build --output-on-failure -j 8
./build/seam_tests "[graphcut]"     # run one group; other tags: [dp] [greedy] [energy]
                                    # [maxflow] [carve] [count] [edge] [improvement]
```

The suite compares DP with brute force, graph cut with DP and with Boost's max-flow, checks
greedy against DP, and covers the edge cases (1-pixel-wide images, constant images, gray and
colour, carving down to 1 x 1, and more). See section 4 of the report.

## Benchmarks and report

```bash
python3 -m venv .venv && .venv/bin/pip install -r benchmarks/requirements.txt
PYTHON=.venv/bin/python benchmarks/run_all.sh    # CSVs, pictures, figures (see below)
report/build_report.sh --strict                  # needs pandoc and Chrome/Chromium
PYTHON=.venv/bin/python slides/build_slides.sh   # rebuilds the .pptx and speaker notes
```

`run_all.sh` regenerates every number, table and figure in the report from scratch. It took
11 minutes on our machine (the measured time is written to `results/machine.txt`), most of
it in the graph-cut experiments; the timing experiments are repeated three times.
`build_report.sh` uses pandoc and a headless Chromium-based browser; set `CHROME=/path/to/browser`
if it is not found automatically.

## CI/CD

- **CI** (`.github/workflows/ci.yml`): every push and pull request is linted with
  clang-format, built with `-Wall -Wextra -Werror`, and tested (Catch2 suite, command-line
  end-to-end test, app self-test). The build-and-test job runs twice, with OpenMP on and
  off, and every job has a 20-minute timeout.
- **CD** (`.github/workflows/release.yml`): pushing a version tag publishes a GitHub Release
  with the tested `seamcarve` binary, the report PDF and the slides:

  ```bash
  git tag v1.0.0 && git push origin v1.0.0
  ```

## Formatting

```bash
clang-format -i $(git ls-files '*.cpp' '*.hpp')
```

## Credits

Team 5: Wong Shi Yu Regine, Tan Ying Clarissa, Muhammad Raziq Bin Mohd Rafig, Isaiah Low
Jung, Yasmin Tazi, Kimberly Charlotte Madlangbayan Ambrosio. AI assistance is declared in
section 9 of the report. Test images are from Wikimedia Commons under Creative Commons
licences (`data/inputs/SOURCES.md`).
