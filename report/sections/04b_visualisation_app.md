## Command-line tool

```
seamcarve --algo dp|greedy|graphcut --energy gradient|neighbour|forward
          --width W --height H [--seams-image F] [--video F.mp4] [--frames PREFIX]
          <input image> <output image>
```

A target smaller than the image removes seams; a larger one inserts seams. The tool prints
the sizes, the number of seams, the total seam cost and the time.

## Step-by-step visualisation (bonus 7a)

Two views are produced by `src/visualize.cpp`:

- **All seams on the original** (`draw_all_seams`): every removed pixel is painted on the
  original image, red for vertical seams and green for horizontal ones. This needs the
  position of each seam pixel *in the original image*, which the index map of
  [3.5](#remove-and-repeat-item-1c) provides.
- **Animation** (`AnimationRecorder`, `write_video`): the carving loop calls a callback for
  every seam: just before a removal, with the seam about to be removed, and just after an
  insertion, with the new pixels. The recorder draws the seam on that picture and stores it
  as a frame. Frames have the size of the larger of the original and the target image, so
  enlarging is animated too. The frames are written as an MP4 video
  (`results/tower/dp_steps.mp4`) or as numbered PNG files.

![Figure 4. Five frames of the step-by-step animation (DP, tower image): the picture shrinks as seams are removed; the red line is the seam about to be removed.](figures/fig_steps.png){width=100%}

![Figure 5. Left: all vertical (red) and horizontal (green) seams of one resize drawn on the original. Middle: height reduced with horizontal seams. Right: width enlarged by seam insertion.](figures/fig_extras.png){width=100%}

![Figure 6. Seam insertion. Colour always shows the direction of the seam: red = vertical, green = horizontal. Left: the pixels duplicated by vertical seams to make the image 20 % wider. Middle and right: one call that makes the image narrower (red: vertical seams removed) and taller (green: horizontal seams whose pixels were duplicated); every seam is drawn at its position in the original.](figures/fig_insertion.png){width=100%}

## Interactive app (bonus 7b)

`seamcarve_app` (Dear ImGui with GLFW and OpenGL) lets the user:

- load an image by file name;
- drag **target width** and **target height** sliders (from 1 pixel up to almost twice the
  original size, so both shrinking and enlarging);
- choose the algorithm (DP, greedy, graph cut) and the energy (gradient, neighbour
  difference, forward);
- see the result immediately when a slider is released, with the seam count, total cost and
  time;
- switch the view between the result, all seams on the original, the energy map, and a
  **step-by-step replay** with a slider over the seam number;
- save the result.

Graph cut is started only by the *Carve now* button and the app warns when the image is
large, because of the runtimes in [5.2](#runtime). The app is an optional build target
(`-DSEAM_BUILD_APP=ON`) so that the core, the tests and the command-line tool build on
machines without a display. `seamcarve_app --selftest <image>` runs the same code paths as
the window without opening one; CI uses it.

## Continuous integration and delivery (bonus 7c)

`.github/workflows/ci.yml` is the **CI** part. It runs on every push and pull request, in
three jobs:

1. **lint**: `clang-format --dry-run --Werror` on every source file;
2. **build-and-test**: install OpenCV, Catch2 and Boost with `apt-get`, build with
   `-Wall -Wextra -Werror`, run the whole test suite with `ctest`;
3. **app**: build the interactive app and run its headless self-test.

Every milestone is merged into `main` through a pull request that must pass these jobs. On
`main` the built command-line tool and the report PDF are also uploaded as build artifacts.

`.github/workflows/release.yml` is the **CD** part. Pushing a version tag (for example
`v1.0.0`) builds the project again, runs the tests, and publishes a GitHub Release with the
`seamcarve` binary, the report PDF and the slides attached, so a tested version of the
deliverables can be downloaded without building anything.

The CI status badge is at the top of the README.
