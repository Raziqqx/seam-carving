# Results

## Set-up

All numbers, tables and figures in this section and the next are produced by
`benchmarks/run_all.sh` (which runs `seam_bench` and `benchmarks/plot_results.py`); the raw
CSV files are in `results/csv/` and all output pictures in `results/`. The machine is an
@@cpu@@ (@@logical_cores@@ logical cores) running @@os@@, compiler @@compiler@@ with `-O2`.

**How timings are measured, and how much they vary.** Each timing is the median of several
calls, and each timing experiment as a whole is repeated @@repetitions@@ times from scratch;
tables show the median of the repetitions, and plots show the range as a shaded band. This
matters, because on a laptop CPU with mixed performance and efficiency cores, turbo
frequencies and other programs running, single measurements vary noticeably from run to
run. The ranges we observed over the repetitions are quoted next to the numbers below
(for example the graph-cut/DP ratio on a full image ranged from @@gc_vs_dp_500_range@@).
The growth rates and orders of magnitude are stable; the second significant digit of any
single timing is not.

**Test images** (sources and licences in `data/inputs/SOURCES.md`):

| Image | Size (w x h) | Why chosen |
|---|---|---|
| `tower` | 500 x 339 | Clear subject on a plain background; the image in the assignment brief |
| `bridge` | 500 x 313 | Strong straight lines and regular structure: the hard case |
| `castle` | 500 x 375 | Textured subject, clouds, detailed foreground |

Graph cut is too slow for these sizes ([5.2](#runtime)), so comparisons that include it use
copies scaled down to 150 pixels wide.

## Runtime

**One seam, growing image size** (Figure 7, Table 1). The seam-search time excludes the
energy map, which is the same for all three methods and is listed separately.

![Figure 7. Time to find one seam against the number of pixels (both axes logarithmic).](figures/fig_runtime_vs_size.png){width=70%}

*Table 1. Time for one seam search (median of @@repetitions@@ repetitions, with the range).*

<!-- include: generated/table_runtime_size.md -->

The two "ns per pixel" columns of Table 1 are the most direct check of the DP's
complexity. In the fastest of the @@repetitions@@ repetitions the seam search costs
between @@dp_ns_per_pixel_best_range@@ ns per pixel at every size from 6,700 to 6.9 million
pixels; a constant cost per pixel is exactly what $\Theta(mn)$ means. The median column
(@@dp_ns_per_pixel_range@@ ns) is the same except where a repetition was disturbed, most
visibly at the largest size (discussed below).

Fitting a straight line through all sizes of Figure 7 on the log-log scale gives the growth
of time with the number of pixels $N$. We fit the per-size *minimum* over the repetitions,
because a run can be slowed down by outside disturbances but never sped up, so the fastest
observation is the most reliable; the fit through the medians is shown for comparison.

| Method | Exponent (fit of minima) | Exponent (fit of medians) | Theory |
|---|---|---|---|
| Dynamic programming | $N^{@@exp_dp@@}$ | $N^{@@exp_dp_median@@}$ | $N^{1}$ |
| Greedy | $N^{@@exp_greedy@@}$ | $N^{@@exp_greedy_median@@}$ | $N^{0.5}$ at a fixed aspect ratio ($m + n$) |
| Graph cut | $N^{@@exp_graphcut@@}$ | $N^{@@exp_graphcut_median@@}$ | see [6.2](#time-and-space-complexity-item-5b) |

DP is linear in the number of pixels, as predicted. The one place where single
measurements stray is the largest image (@@dp_largest_size@@): there the three repetitions
ranged from @@dp_largest_ms_range@@ ms, a factor of @@dp_largest_spread@@. This is
measurement noise and not super-linear growth: every call allocates two fresh tables of
about @@dp_largest_table_mb@@ MB, which under WSL2 means thousands of page faults whose cost varies from run to
run, and the same function on a similar-sized image in Table 6 runs at the usual cost per
pixel. Greedy grows roughly like $\sqrt N$ because it only walks down the $m$ rows after one
scan of the top row; its times are a few microseconds, so its exponent is the least precise.
Graph cut grows faster than linearly and has a very large constant: on a
@@size_graphcut_100@@ image it is about @@gc_vs_dp_100@@ times slower than DP, and a
@@size_graphcut_250@@ image (@@gc_250_pixels@@ pixels) takes @@gc_250_seconds@@ s for a
single seam, using @@gc_250_augmentations@@ augmenting paths.

**Many seams** (Figure 8, Table 2). Here each step includes the energy map, the seam search
and the removal.

![Figure 8. Cumulative time while removing half of the columns of the tower image.](figures/fig_runtime_vs_seams.png){width=100%}

*Table 2. Total time to remove half of the columns.*

<!-- include: generated/table_runtime_seams.md -->

On the full image, DP needs @@seams_total_ms_dp_tower@@ ms for
@@seams_count_tower@@ seams and greedy @@seams_total_ms_greedy_tower@@ ms. Greedy's seam
search is hundreds of times faster than DP's (Table 1), yet the whole loop is only
@@loop_speedup_greedy@@ times faster (range @@loop_speedup_greedy_range@@), because every step still computes an energy map and
copies the image, both $\Theta(mn)$, whichever seam finder is used. Graph cut needs @@seams_gc_seconds@@ s for @@seams_count_tower_small@@ seams
on the small @@seams_size_tower_small@@ copy.

**Practical sizes for graph cut.** With our Edmonds-Karp, graph cut is comfortable up to
about @@size_graphcut_100@@ pixels (@@gc_100_seconds@@ s per seam) and usable up to about
@@size_graphcut_250@@ (@@gc_250_seconds@@ s per seam). On a full @@size_graphcut_500@@ test
image one seam takes about @@gc_500_seconds@@ s, roughly @@gc_vs_dp_500@@ times longer than
DP (range over the repetitions: @@gc_vs_dp_500_range@@), so removing 30 % of the width
(150 seams) would take in the order of @@gc_500_minutes_150_seams@@ minutes against a
fraction of a second for DP.

## Seam cost

**DP against greedy, full-size images** (Table 3, Figure 9). Each run removes 30 % of the
width (vertical seams) or of the height (horizontal seams). "Total cost" is the sum of the
energies of all removed seams, each measured when it was removed.

*Table 3. Total seam cost, DP against greedy.*

<!-- include: generated/table_cost_full.md -->

![Figure 9. Total energy removed with vertical seams (lower is better).](figures/fig_seam_cost.png){width=62%}

Greedy removes between @@cost_ratio_min@@ and @@cost_ratio_max@@ times as much energy as
DP. The column "greedy steps that were optimal" counts the steps at which the greedy seam
happened to have the minimum possible cost for the image at that step. The smallest gap
(bridge, horizontal seams) is a case where both methods find the calm sky and water, so
even greedy's seams are cheap.

**All three methods, small images** (Table 4).

*Table 4. Seam cost on the 150-pixel-wide copies. GC = graph cut.*

<!-- include: generated/table_cost_small.md -->

Three observations:

1. **The first seam costs exactly the same for DP and graph cut** on every image and in
   both directions, as the theory says (the plotting script stops with an error if they
   ever differ).
2. **Every graph-cut step is optimal.** At each of the @@gc_total_steps@@ graph-cut steps in
   Table 4 we also ran DP on the same intermediate image; the costs were equal in
   @@gc_optimal_steps@@ of @@gc_total_steps@@ steps.
3. **The totals of DP and graph cut still differ slightly** (by at most
   @@gc_total_gap_percent@@ %, and in either direction). This is not an error. When two
   seams tie for the minimum, DP and graph cut pick different ones
   ([3.4](#graph-cut-item-4)): after that the two runs are carving *different images*, so
   their later seams are not comparable one by one. Removing the best seam at each step is
   itself a greedy strategy over the sequence of removals, and neither tie-breaking rule
   is better in general.

## Visual comparison

Figures 10 to 12 compare seam carving with the two baselines from the brief (plain scaling
and cropping) at 70 % of the original width, and Figures 13 to 15 show where each method
removed pixels.

![Figure 10. Tower: baselines and seam carving at 70 % width.](figures/fig_compare_tower.png){width=100%}

![Figure 11. Bridge at 70 % width.](figures/fig_compare_bridge.png){width=100%}

![Figure 12. Castle at 70 % width.](figures/fig_compare_castle.png){width=100%}

![Figure 13. Tower: energy map, and all seams removed by DP and by greedy.](figures/fig_seams_tower.png){width=100%}

![Figure 14. Bridge: energy map and seams.](figures/fig_seams_bridge.png){width=100%}

![Figure 15. Castle: energy map and seams.](figures/fig_seams_castle.png){width=100%}

**Tower (clear subject, plain background).** This is the ideal case. Scaling makes the
tower thinner; cropping keeps its shape but cuts off the left of the scene, including the
person. DP takes its seams from the sky and grass on both sides of the tower (Figure 13,
middle) and leaves the tower at its original width; the person is kept and simply ends up
closer to the tower. Greedy seams (Figure 13, right) start at a cheap sky pixel but cannot
plan ahead: many of them run straight down into the tower, and in Figure 10 the greedy
result has a visibly thinner, distorted tower.

**Bridge (straight lines).** This is the hard case for every seam-carving variant. The deck
and the cables cross the full width, so every vertical seam must cut through them, and each
seam shifts part of a line by one pixel. After 150 seams the cables are bent near the left
edge and the far tower is squeezed towards the border. Greedy does worse (it destroys the
building in the lower left); forward energy keeps the lines smoothest of the three. Plain
scaling keeps straight lines straight and is arguably the better choice for this picture,
which agrees with the limitations discussed by Avidan and Shamir.

**Castle (texture everywhere).** The castle spans almost the whole width, so at 70 % some
seams must pass through it whatever the method. DP places them in the smoother wall
sections and the sky gaps between the towers; the greedy result looks similar at this size
but its seam cost is @@cost_ratio_castle_vertical@@ times higher, and Figure 15 shows its
seams spread across the building instead of concentrating where the energy is low.

**Does greedy give comparable visual quality?** No. It can look acceptable when little is
removed or when the picture has large flat regions in every row, but at 30 % it distorts
the subject of the tower and bridge images, and its seam cost is @@cost_ratio_min@@ to
@@cost_ratio_max@@ times the optimum. It makes the complete loop only
@@loop_speedup_greedy@@ times faster ([5.2](#runtime)), which does not justify the loss of
quality.

**DP against graph cut** (Figures 16 to 18, small copies). The seams and results are the
same or nearly the same, as expected from two exact methods: they differ only where ties
are broken differently.

![Figure 16. Tower, 150 pixels wide: seams and results of DP, graph cut and greedy.](figures/fig_small_tower.png){width=88%}

![Figure 17. Bridge, 150 pixels wide.](figures/fig_small_bridge.png){width=88%}

![Figure 18. Castle, 150 pixels wide.](figures/fig_small_castle.png){width=88%}

## Forward energy

Removing low-energy pixels can *create* new edges, because the pixels left and right of the
seam become neighbours. Forward energy chooses the seam that adds the least new edge energy.
Table 5 measures the effect as the mean Sobel energy per pixel of the result: a higher value
than the original means the picture has become "busier" (more artificial edges per pixel).

*Table 5. Mean gradient energy per pixel before and after removing 30 % of the width.*

<!-- include: generated/table_forward.md -->

Removing the calmest pixels necessarily raises the mean energy of what is left. Between the
two DP variants, forward energy raises it less on every image, which matches the smoother
lines in the last panels of Figures 10 to 12. Greedy shows the smallest increase for a bad
reason: its seams run through high-energy content (Figures 13 to 15), so it removes edges
that should have been kept. This number is therefore only meaningful for comparing the two
optimal methods, and must be read together with the pictures.
