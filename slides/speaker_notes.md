# Speaker notes

Six sections, 2 minutes each, 12 minutes in total. The time stamps at the start of each note are targets. The same notes are in the notes pane of the slides.

## Slide 1: Image Compression by Seam Carving

**Speaker:** Muhammad Raziq

[Raziq, 0:00-0:15] We are Team 5. Seam carving resizes an image by removing paths of unimportant pixels, rather than squashing or cropping it. We implemented it three ways: dynamic programming, greedy and graph cut. I will cover the energy map and the DP.

## Slide 2: The pipeline: energy, find a seam, remove, repeat

**Speaker:** Muhammad Raziq

[Raziq, 0:15-0:50] All three methods share the same loop. First, the energy map: we take the Sobel gradient of the grayscale image and add the absolute values of Gx and Gy. There is no square root, so every energy is an exact integer. At the border, we replicate the edge pixel. A seam is one pixel per row, with neighbours at most one column apart. The picture shows the energy and the seams DP removed; they avoid the tower. After each removal, we recompute the energy. For horizontal seams, we transpose the image, so each algorithm is written once.

## Slide 3: Dynamic programming finds the optimal seam in O(mn)

**Speaker:** Muhammad Raziq

[Raziq, 0:50-1:55] There are at least 2 to the power m seams, so we cannot try them all. We let M of i j be the cost of the cheapest seam from the top row to pixel i j. That seam must pass through one of the three pixels above, and the part above must itself be the cheapest. That is optimal substructure, and it gives the recurrence. In the edge columns, we skip the missing parent. On the right is our 4 by 4 example. The bottom-row minimum is 6, and following the parent arrows gives the shaded seam. The table has m times n cells with three comparisons each, so the DP takes O of m n time and space. Our tests compare the DP with brute force on 540 random grids. Over to Regine.

## Slide 4: Greedy: pick the locally cheapest pixel, row by row

**Speaker:** Wong Shi Yu Regine

[Regine, 2:00-2:55] Greedy makes one local decision per row. It starts at the cheapest pixel of the top row, then always moves to the cheapest of the three pixels below. No table is needed, so the search is O of n plus m. The seam is always valid, but not optimal. In our 3 by 3 example greedy starts at the 1. From there it can only reach the two left columns, all 9s, so it pays 19. DP starts at the 2 and reaches the cheap right column: total 4. The locally best choice was globally wrong; this problem has no greedy-choice property.

## Slide 5: Greedy vs DP: much higher seam cost, visible damage

**Speaker:** Wong Shi Yu Regine

[Regine, 2:55-3:55] On three images, removing 30 percent of the width, greedy removed 1.11 to 3.00 times as much energy as DP. In the bottom row, DP is on the left and greedy in the middle: DP removes sky and grass and keeps the tower's width; greedy's seams run into the tower and distort it. Greedy's search is about a hundred times faster, but every method still computes an energy map and copies the image per seam, so the whole loop is only about 2.1 times faster. So greedy is not comparable to DP in visual quality, and it saves little time. Next, Clarissa.

## Slide 6: Graph cut: a seam is a minimum s-t cut

**Speaker:** Tan Ying Clarissa

[Clarissa, 4:00-4:55] Graph cut turns the problem into a flow network: one node per pixel, a source joined to the first column, a sink after the last. Blue forward edges carry the energy of the pixel they leave; cutting one removes that pixel. Grey edges are infinite and can never be cut. The backward edges make the source side of each row a prefix, so each row is cut once. The diagonal edges keep neighbouring rows within one column, so the cut is a connected seam. Cut capacity equals seam energy, so the minimum cut is the optimal seam, and its value is the max flow. We solve it with our own Edmonds-Karp, then take the rightmost source-side pixel of each row.

## Slide 7: Finding: fix the last column; result equals DP, but slowly

**Speaker:** Tan Ying Clarissa

[Clarissa, 4:55-5:55] Our tests compare graph cut with DP, and at first they disagreed. The brief joins the last column to the sink with infinite capacity, so a last-column pixel can never be removed. Our fix gives that edge the pixel's energy. Then, on 768 random grids, the seam cost equals the DP cost and the max-flow value, and our max-flow matches Boost's. So graph cut is correct, but slow: one seam on a 250 x 169 image takes 1.75 seconds and 930 augmenting paths, each a BFS over the whole graph. DP needs under a millisecond. The worst case is O of V E squared. Over to Isaiah.

## Slide 8: Testing: 110 test cases, 26,115 checks, all passing

**Speaker:** Isaiah Low Jung

[Isaiah, 6:00-7:05] Our rule: test every algorithm against an independent way of getting the same answer. For DP that is brute force: on grids up to 6 by 6 we list every seam and take the true minimum. Graph cut is checked against DP, and its max flow against Boost, which is used only in that test. Greedy must be valid and never cheaper than DP. We check exact output shapes, that horizontal equals the transpose trick, and edge cases: one pixel, one column, constant images, gray and colour, carving down to one pixel, and enlarging. Optimised variants must equal the plain ones. In total 110 test cases and 26,115 checks, all passing. The cross-check is what caught the last-column issue.

## Slide 9: CI/CD: every push is linted, built and tested

**Speaker:** Isaiah Low Jung

[Isaiah, 7:05-7:55] For CI/CD we use GitHub Actions. Every push and pull request runs lint with clang-format; build-and-test, which builds with warnings as errors and runs all tests, including an end-to-end test of the command-line tool; and an app job that builds the app and runs its self-test without a display. Build-and-test runs twice, with and without OpenMP, so OpenMP stays optional. Milestones 2 to 6 were merged into main through pull requests whose CI checks passed. For delivery, pushing a version tag such as v1.0.0 publishes a release with the tested program, the report and the slides. Yasmin will now show the program running.

## Slide 10: Step-by-step visualisation: every seam, drawn and animated

**Speaker:** Yasmin Tazi

[Yasmin, 8:00-8:35] For the visualisation bonus we show every seam. The carving loop calls a callback for each seam; a recorder draws it, red for vertical and green for horizontal, on the current image and saves a frame, and the frames become an MP4. These five frames show the image shrinking while the tower stays intact. To draw all seams on the original image we carry an index map that remembers where each pixel came from.

## Slide 11: Live demo: interactive resizing app

**Speaker:** Yasmin Tazi

[Yasmin, 8:35-9:55, LIVE DEMO, see slides/demo_script.md] One: I load the tower and drag the width to about 350 with DP; the result appears when I release the slider, with the seam count, total cost and time. Two: the view 'all seams on the original' shows the seams avoiding the tower. Three: with greedy the cost goes up and seams cross the tower. Four: back to DP, the step-by-step view replays the removal. Five: dragging the width above the original size inserts seams, and the image gets wider. Six: on the small tower image, graph cut takes about four seconds for ten seams; its first seam costs exactly what DP's does, and the totals agree within about one percent. Over to Kimberly.

## Slide 12: Item 2: exponentially many seams; D&C does not help Sobel

**Speaker:** Kimberly Charlotte M. Ambrosio

[Kimberly, 10:00-10:45] Question 2a-i: a seam can start in any of n columns. In every later row it can stay in its column, and because n is more than 1, at least one diagonal move is also inside the image: at least 2 choices per row, at most 3. So the count lies between n times 2 to the m minus 1 and n times 3 to the m minus 1: exponential in m. For 2b: direct Sobel is 9 operations per pixel, already linear and optimal. Splitting into quadrants stays linear. Divide and conquer only helps to use several cores.

## Slide 13: Complexity and comparison

**Speaker:** Kimberly Charlotte M. Ambrosio

[Kimberly, 10:45-11:20] DP and graph cut are optimal; greedy is not. The DP search is theta of m n. Greedy's search is m plus n, but with the energy map and removal, k seams cost k m n for both. Graph cut is O of V E squared in the worst case. Measured, the DP costs the same few nanoseconds per pixel at every size, so it is linear; graph cut is far slower.

## Slide 14: Improvements (5d) and conclusions

**Speaker:** Kimberly Charlotte M. Ambrosio

[Kimberly, 11:20-11:55] Do we need the whole DP table? No: two rolling rows of costs and one-byte parents: 8.0 times less memory. Multithreading helps the energy map, up to 7.1 times against our serial loop, but hardly the DP table, because each row depends on the row above. Updating only the energies next to the seam makes that step 7.9 times faster. To conclude: DP is optimal, fast and simple; greedy is fast but poor; graph cut is correct but slow.

## Word counts

At about 150 words per minute, 280 words is roughly 1 minute 50 seconds.

| Speaker | Words |
|---|---|
| Muhammad Raziq | 279 |
| Wong Shi Yu Regine | 208 |
| Tan Ying Clarissa | 231 |
| Isaiah Low Jung | 226 |
| Yasmin Tazi | 197 |
| Kimberly Charlotte M. Ambrosio | 256 |
