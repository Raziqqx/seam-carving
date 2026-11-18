# Live demo script (section 5, about 80 seconds)

## Before the presentation

```bash
cmake -S . -B build-app -DSEAM_BUILD_APP=ON
cmake --build build-app -j --target seamcarve_app
./build-app/seamcarve_app data/inputs/tower.png      # leave the window open
```

Have a terminal open in the repository folder as a fallback.

## Steps

| # | Action in the app | What to say | What the audience should see |
|---|---|---|---|
| 1 | Algorithm = *Dynamic programming*. Drag **Target width** from 500 to about 350 and release. | "The result appears as soon as I release the slider. The status line shows the seam count, the total cost and the time." | Narrower picture, tower unchanged; 150 seams in well under a second |
| 2 | **View** = *All seams on the original*. | "Every removed seam, drawn on the original. They pass through sky and grass and avoid the tower." | Red seams left and right of the tower |
| 3 | **Algorithm** = *Greedy* (the resize re-runs). | "Greedy: the total cost is more than double and the seams bunch together." | Higher cost in the status line; dense bundle of seams |
| 4 | Algorithm back to *Dynamic programming*. **View** = *Step by step*; drag **Seam number**. | "The replay shows each seam just before it is removed." | Image shrinking seam by seam, red seam moving |
| 5 | **View** = *Result*. Drag **Target width** to about 600 and release; then **Target height** to about 280. Optionally **View** = *Step by step* to replay the insertion. | "Above the original size, seams are inserted instead of removed. Height uses horizontal seams." | Wider picture with the tower unchanged; then a shorter one |
| 6 | Type `data/inputs/tower_small.png`, **Load**. Algorithm = *Graph cut (slow)*, drag width from 150 to 140, press **Carve now** (about 4 seconds for 10 seams). Then switch the algorithm to *Dynamic programming*. | "Graph cut is also optimal: the first seam costs exactly the same for both, which we checked on the command line, and the totals agree to within about 1 %, because the two break ties differently. But it needs seconds where DP needs milliseconds." | A wait of a few seconds; the same-looking picture; total cost within 1 % of DP's (21,078 against 21,002) |
| 7 (optional) | Press **Save seams picture as seams.png**. | "The app can also save the result and the picture of all seams." | Status line: "Saved seams.png" |

Timing notes for step 6: graph cut takes about 0.4 seconds per seam on the 150 x 101
image, so keep it to about 10 seams during the talk (10 seams take about 4 seconds, 30 seams
about 15 seconds). The status line shows the TOTAL cost of all seams, which is why DP and
graph cut can differ in the last digits. The first seam alone costs 1,888 with both
methods:

```bash
./build/seamcarve --algo graphcut --width 149 data/inputs/tower_small.png gc1.png   # total cost: 1888
./build/seamcarve --algo dp       --width 149 data/inputs/tower_small.png dp1.png   # total cost: 1888
```

## Fallback without the app (command line)

```bash
./build/seamcarve --algo dp --width 350 --seams-image seams_dp.png --video steps.mp4 \
    data/inputs/tower.png out_dp.png
./build/seamcarve --algo greedy --width 350 --seams-image seams_greedy.png \
    data/inputs/tower.png out_greedy.png
./build/seamcarve --algo graphcut --width 140 data/inputs/tower_small.png out_gc.png
./build/seamcarve --algo dp --width 140 data/inputs/tower_small.png out_dp_small.png
./build/seamcarve --algo dp --width 600 --video grow.mp4 data/inputs/tower.png out_wide.png
```

Open `out_dp.png`, `seams_dp.png`, `seams_greedy.png` and `steps.mp4`, and point out the
"total cost" lines printed by the tool.

## Recording checklist

- Each member speaks for at most 2 minutes; total at most 12 minutes.
- Show the app actually running (item 8c).
- Upload the video and put the link in `report/sections/08_links.md`, then rebuild the PDF.
