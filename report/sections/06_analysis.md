# Analysis and discussion

## Answers to the questions (item 5a)

The questions of item 2 are answered in full in [section 2](#answers-to-item-2). In short:
the number of seams is between $n \cdot 2^{m-1}$ and $n \cdot 3^{m-1}$, so exhaustive search
is hopeless; the problem has optimal substructure and overlapping subproblems, giving the
recurrence $M[i][j] = e[i][j] + \min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])$; and divide and
conquer cannot beat the already linear direct Sobel filter, although it is a good way to
organise parallel work.

## Time and space complexity (item 5b)

Notation: $m$ rows, $n$ columns, $N = mn$ pixels, $k$ seams removed.

### One seam

| Step | Time | Extra space | Derivation |
|---|---|---|---|
| Energy map | $\Theta(mn)$ | $\Theta(mn)$ | 9 pixel reads per pixel, one output value per pixel |
| DP: fill table | $\Theta(mn)$ | $\Theta(mn)$ | $mn$ cells, at most 3 comparisons each; two $m \times n$ tables |
| DP: backtrack | $\Theta(n + m)$ | $\Theta(m)$ | scan the bottom row, then one step per row |
| DP, rolling rows | $\Theta(mn)$ | $2n$ integers + $mn$ bytes | same loop; only two cost rows are kept |
| Greedy | $\Theta(n + m)$ | $\Theta(m)$ | scan the top row, then 3 comparisons per row; no table |
| Graph cut: build graph | $\Theta(mn)$ | $\Theta(mn)$ | $V = mn + 2$, $E = 4mn - 2m - 2n + 2$ |
| Graph cut: Edmonds-Karp | $O(VE^2) = O((mn)^3)$ | $\Theta(V + E) = \Theta(mn)$ | see below |
| Graph cut: read seam | $\Theta(mn)$ | $\Theta(mn)$ | one BFS, then a scan of each row |
| Remove the seam | $\Theta(mn)$ | $\Theta(mn)$ | every row is copied into a new image |

**DP.** The table has $mn$ cells and each is computed in constant time from at most three
cells of the row above, so the time is $\Theta(mn)$; this is optimal up to a constant,
because any algorithm must at least read all $mn$ energies (a single unread pixel could
have been the one cheap pixel that changes the answer).

**Greedy.** The search itself reads only $n + 3(m-1)$ energies. But it still needs the
energy values, and computing the whole map costs $\Theta(mn)$. So *one seam including its
energy map* is $\Theta(mn)$ for greedy too. In our measurements the energy map is
@@greedy_total_share_3200@@ % of greedy's time for one seam on a @@size_greedy_3200@@ image.
(Computing energies lazily, only for the $3m$ pixels greedy looks at plus the top row, would
give $\Theta(n + m)$ per seam; then the $\Theta(mn)$ removal would dominate.)

**Graph cut with Edmonds-Karp.** Each augmentation is one BFS, $O(E)$. The textbook
argument (CLRS, Theorem 26.8) bounds the number of augmentations: the BFS distance from the
source to any node never decreases, and each edge can become the bottleneck ("critical")
at most $V/2$ times, so there are $O(VE)$ augmentations. Total: $O(VE \cdot E) = O(VE^2)$.
With $V \approx mn$ and $E \approx 4mn$ this is $O((mn)^3)$.

**A tighter bound, and what we measured.** Capacities are integers and each augmentation
adds at least 1 unit of flow, so the number of augmentations is at most the flow value
$F$, which is the optimal seam cost. With bounded energies $F \le 2040\,m$, so the time is
$O(F \cdot E) = O(m \cdot mn) = O(m^2 n)$. For images of a fixed aspect ratio
$m \propto \sqrt N$, so this bound is $O(N^{1.5})$, far below the cubic worst case (which
assumes every edge becomes critical $V/2$ times; that does not happen on these regular
grids).

The measurements agree with this picture. The *number* of augmenting paths, which does not
depend on timing noise, grows like $N^{@@exp_augmentations@@}$ over the tested sizes (for
example @@gc_250_augmentations@@ paths for @@size_graphcut_250@@ and
@@gc_500_augmentations@@ for @@size_graphcut_500@@), below the $N^{0.5}$ that the bound
allows. Each augmentation is one BFS over the whole graph, $\Theta(N)$, so we expect a time
of roughly $N^{@@exp_graphcut_expected@@}$. The measured time exponent is
@@exp_graphcut@@ (with the full-size image included in the fit). It is somewhat higher
because the cost of one augmentation, divided by the number of pixels, also rises with the
image size: from @@gc_bfs_ns_small@@ ns per augmentation per pixel for the
@@gc_smallest_size@@ image to @@gc_bfs_ns_large@@ ns for @@size_graphcut_500@@. (This is
not a cost per node visited: a BFS stops as soon as it reaches the sink, so it usually
explores only part of the graph.) The rise is consistent with two effects: on larger images
a BFS explores a growing share of the graph before it reaches the sink, and at the largest
sizes the network (@@bytes_per_pixel_graph_cut_network@@ bytes per pixel, tens of megabytes
in total) no longer fits in the processor's caches. We did not separate the two. What
makes graph cut slow in practice is therefore not the exponent but the *constant*:
hundreds to thousands of breadth-first searches, each over a large part of the graph,
against a single pass over the table for DP.

Space is $\Theta(mn)$ like DP, but with a much larger constant: about $4mn$ edges, each
stored with its reverse edge and an adjacency-list entry. Measured on a
@@graph_memory_size@@ image (@@graph_memory_nodes@@ nodes, @@graph_memory_edges@@ edges):
@@bytes_per_pixel_graph_cut_network@@ bytes per pixel for the network, against
@@bytes_per_pixel_dp_full_tables@@ bytes per pixel for the two DP tables.

### k seams

Removing $k$ vertical seams means running the loop on images of width
$n, n-1, \dots, n-k+1$:

$$ T_k = \sum_{t=0}^{k-1} \Theta\big(m\,(n - t)\big) = \Theta\!\Big(m\big(kn - \tfrac{k(k-1)}{2}\big)\Big) = \Theta(kmn) \quad\text{for } k \le n. $$

| Method | $k$ seams, time | Space |
|---|---|---|
| DP | $\Theta(kmn)$ | $\Theta(mn)$ |
| Greedy (energy map recomputed, image copied) | $\Theta(kmn)$ | $\Theta(mn)$ |
| Graph cut | $O(k\,(mn)^3)$ worst case; $O(k\,m^2 n)$ for bounded energies; about $k \cdot N^{@@exp_graphcut@@}$ measured | $\Theta(mn)$ |

The space does not grow with $k$: each step frees the previous tables. Horizontal seams
swap the roles of $m$ and $n$; the two transposes cost $\Theta(mn)$ once per call, not per
seam. Removing all the way from width $n$ to width 1 costs $\Theta(mn^2)$.

Figure 8 matches this: the cumulative curves bend slightly downwards (each step is a little
cheaper than the last, because the image is narrower), and DP and greedy stay within a
small constant factor of each other on the full image (@@loop_speedup_greedy@@x), since
both pay the same $\Theta(mn)$ energy and copy steps for every seam.

## Comparison of the three methods (item 5c)

| | Dynamic programming | Greedy | Graph cut (Edmonds-Karp) |
|---|---|---|---|
| Finds the minimum-energy seam | Yes (proved; tested against brute force) | No (counter-example; @@cost_ratio_min@@x to @@cost_ratio_max@@x the DP cost) | Yes (max-flow min-cut; tested against DP) |
| Seam search, time | $\Theta(mn)$ | $\Theta(m + n)$ | $O((mn)^3)$ worst, $O(m^2 n)$ for bounded energies, about $N^{@@exp_graphcut@@}$ measured |
| Seam search, @@size_dp_500@@ image | @@seam_ms_dp_500@@ ms | @@seam_ms_greedy_500@@ ms | @@gc_500_seconds@@ s |
| Seam search, @@size_dp_100@@ image | @@seam_ms_dp_100@@ ms | about a microsecond | @@seam_ms_graphcut_100@@ ms |
| Table / graph memory | @@bytes_per_pixel_dp_full_tables@@ bytes per pixel (1 with rolling rows) | none | @@bytes_per_pixel_graph_cut_network@@ bytes per pixel |
| Whole loop on the full tower image (@@seams_count_tower@@ seams) | @@seams_total_ms_dp_tower@@ ms | @@seams_total_ms_greedy_tower@@ ms | about @@gc_500_seconds@@ s for the first seam alone |
| Visual quality at 70 % width | Good on tower; acceptable on castle; bends the bridge's lines | Distorts the tower and the bridge | Same as DP (same cost; ties broken differently) |
| Code size and difficulty | Two short functions | One short function | Graph construction + max-flow + cut extraction |
| Extends to | Forward energy (step-dependent costs) | - | Costs between any neighbouring pixels; video volumes (cuts through 3-D graphs) |

**Greedy against DP.** Greedy's only advantage is the speed of the seam search itself,
which in the complete loop is mostly hidden by the energy map and the removal: a
@@dp_vs_greedy_3200@@-fold difference in median seam-search time on a @@size_dp_3200@@
image (the noisy point of Table 1; with DP's fastest repetition it is still over 300-fold)
becomes @@dp_total_3200@@ ms against @@greedy_total_3200@@ ms once the energy map is
included, and the complete loop of Figure 8 is only @@loop_speedup_greedy@@ times faster.
In exchange its seams cost @@cost_ratio_min@@ to @@cost_ratio_max@@ times more energy, and
the pictures show the damage. Greedy fails because seam finding does not have the
greedy-choice property: a locally cheapest pixel says nothing about the rows below it. DP
looks at every pixel once and is exact; for this problem it is the better choice.

**Graph cut against DP.** Both are exact, and the tests and Table 4 confirm equal seam
costs. DP is the right tool here because the seam graph is a simple layered grid in which
the shortest path can be found in one pass; a general max-flow solver ignores that structure
and pays for it with a runtime thousands of times larger. Graph cut becomes the right tool
when DP no longer applies: Rubinstein et al. (2008) carve *videos*, where the seam is a 2-D
surface through a 3-D volume. A surface cannot be found by a row-by-row DP, but it is still
a minimum cut. Our experiments show the price of that generality on single images.

**Seam carving against scaling and cropping.** Seam carving keeps the subject's shape where
the image has removable low-energy regions (tower, castle). It is the wrong tool where
important straight structure spans the image (bridge) or when too many seams are removed,
because then the seams must pass through content.

## Improvements (item 5d)

### Do we need the whole DP table?

No, not all of it. To compute row $i$ we only need row $i - 1$ of the *costs*, so two rows
that swap roles are enough. What we cannot drop completely is the information for
backtracking: the seam is recovered from the bottom up, so we need each cell's parent.
But a parent is always one of three neighbours, so one byte per cell (a step of $-1$, $0$ or
$+1$) replaces a 4-byte column index. `find_vertical_seam_dp_rolling` does exactly this and
returns the same seam as the full version (tested on 200 grids).

*Table 6. Memory and time of the two DP variants.*

<!-- include: generated/table_memory.md -->

The table memory drops from @@mem_full_mb@@ MB to @@mem_rolling_mb@@ MB
(@@mem_ratio@@x less) for a @@mem_size@@ image, and the seam search even becomes slightly
faster (@@mem_full_ms@@ ms against @@mem_rolling_ms@@ ms; the rolling version was faster in
each of the @@repetitions@@ repetitions). Going further is possible: 2 bits per
step would give 32x; and a divide-and-conquer scheme in the style of Hirschberg's algorithm
(find where the optimal seam crosses the middle row using a forward and a backward cost
pass, then recurse on the two halves) needs only $O(n)$ extra space, at the price of
$O(mn \log m)$ time in the worst case. If only the optimal *cost* is needed and not the
seam, two rows alone are enough. We did not implement these because the one-byte version is
already small and much simpler to explain.

### Does multithreading help?

It depends on which step.

- **Energy map: yes.** Each pixel's energy depends only on the input image, never on
  another energy value, so the rows can be computed in any order by any number of threads
  with no locking ("embarrassingly parallel").
- **DP table: only inside a row.** Row $i$ needs the finished row $i - 1$, so the rows must
  be processed in order. Within one row the cells are independent, so the inner loop can be
  split among threads, but each row holds little work and every row pays the cost of
  starting and joining the threads.
- **Backtracking and greedy: no.** Each step depends on the previous one, and they take
  microseconds anyway.
- **Edmonds-Karp: not usefully.** Augmentations must happen one after another; a parallel
  BFS helps little on such sparse graphs.

**What is compared with what.** To parallelise the energy map we need a loop of our own,
so the baseline for the speed-ups below is our hand-written serial Sobel loop
(`compute_gradient_energy_manual`, @@threads_energy_small_serial_ms@@ ms on a
@@threads_small_size@@ image), *not* the library's default path. OpenCV's `Sobel`, which
the normal carving loop uses, is itself optimised (separable, vectorised) and takes
@@energy_opencv_small_ms@@ ms on the same image: faster than our serial loop and
comparable to our loop with many threads. So the experiment answers "does this computation
parallelise?" (yes); it does not claim that our threaded loop beats OpenCV. The DP baseline
is `build_cumulative_energy`.

*Table 7. OpenMP timings (median of @@repetitions@@ runs), with the speed-up over our serial loop in brackets.*

<!-- include: generated/table_threads.md -->

![Figure 19. Speed-up over the plain serial loop. The dashed line marks 1x (no gain).](figures/fig_threads.png){width=100%}

On the large @@threads_large_size@@ image the energy map reaches a speed-up of
@@threads_energy_large_best@@x with @@threads_energy_large_best_n@@ threads (the best
speed-up ranged from @@threads_energy_large_best_range@@x over the repetitions), while the
row-parallel DP table reaches only @@threads_dp_table_large_best@@x at best. On the small
@@threads_small_size@@ image the row-parallel DP gains nothing (best: 0.55x of the plain loop's speed, with one
thread; Table 7), because a row of 500 cells
takes less time than waking the threads up.

Three details of Table 7 need explaining:

- *One OpenMP thread is usually no faster than the serial function*, although both do the
  same work: the OpenMP version pays for starting a parallel region, and for the DP it pays
  that once per row (0.55x and 0.66x). For the large energy map the cost is visible as well
  (0.84x); for the small one the difference is within the run-to-run noise (its 1-thread
  result varied between 0.86x and 1.21x over the repetitions).
- *The curve is not smooth, and going from 4 to 8 threads can gain little or even lose*
  (small energy map: @@threads_energy_small_4@@x and @@threads_energy_small_8@@x; large DP
table: @@threads_dp_table_large_4@@x and @@threads_dp_table_large_8@@x). The CPU is a hybrid
  design with 6 fast performance cores (12 hardware threads) and 8 slower efficiency cores.
  Up to about 6 threads each thread can have a fast core to itself; beyond that, threads
  share a core through hyper-threading or land on efficiency cores, and a `static`
  schedule then waits for the slowest thread. Turbo frequency also drops as more cores are
  busy.
- *Occasionally 2 threads look slightly more than 2x faster*. That is measurement noise of
  the same origin (the serial baseline may have been timed on a slower core or at a lower
  frequency), which is why we report medians of repeated runs and the range.

The speed-ups stay well below the number of threads for two more reasons: the loops are
limited by memory bandwidth rather than arithmetic, and (Amdahl's law) the sequential parts
of the whole pipeline, such as the removal copy and the row order of the DP, do not shrink.
So multithreading helps the energy step on large images and is not worth it for the DP
itself.

### Incremental energy update

After a seam is removed, only the pixels whose $3 \times 3$ window contains the cut change
their energy. In the new image these are, per row, the four columns
$\text{seam}[i] - 2 \,..\, \text{seam}[i] + 1$ (the rows above and below have their own seam
position at most one column away, which is why the band is four wide rather than two). All
other energies are simply copied, shifted left by one where they were right of the seam.
`update_gradient_energy_after_removal` does this with $4m$ Sobel evaluations instead of
$mn$, and a test checks after *every* seam that the result equals a full recomputation.

**At the borders** the band is clipped to the new image, whose width is $n-1$: row $i$
recomputes the columns $\max(\text{seam}[i]-2,\,0)$ to $\min(\text{seam}[i]+1,\,n-2)$.
Concretely:

- seam at column 0: columns 0 and 1 (two columns; nothing lies to the left);
- seam at column 1: columns 0, 1 and 2;
- seam in the last column $n-1$: the last two columns of the new image, $n-3$ and $n-2$.
  Column $n-2$ must be recomputed even though its pixel did not move: its right
  neighbour was the removed pixel, and now the border clamp makes it its own right
  neighbour;
- seam in column $n-2$: columns $n-4$ to $n-2$, the last three;
- a 2-column image ($n = 2$): only column 0 is left and it is recomputed in every row.

These cases need fewer than four Sobel evaluations per row, so $4m$ is an upper bound.
Two tests try *every* possible seam on a 1-row image (where the clamp also applies above
and below every pixel) and on a 2-column image, and compare with a full recomputation.

*Table 8. Removing @@inc_seams@@ seams from a @@inc_size@@ image with DP.*

<!-- include: generated/table_incremental.md -->

The energy step becomes @@inc_energy_speedup@@x faster and the whole loop
@@inc_total_speedup@@x faster. As in the threading experiment, both rows of Table 8 use our
hand-written Sobel (`sobel_energy_at`), so the comparison is like for like; against
OpenCV's faster `Sobel` the gain of the incremental update would be smaller, but the
update still does $4m$ instead of $mn$ evaluations. The loop is still $\Theta(mn)$ per seam, because the DP
table and the copy remain; a further step would be to update the DP table only inside the
triangle of cells that the changed energies can influence.

### Forward energy

Forward energy ([3.1](#energy-map-item-1a), [5.4](#forward-energy)) improves *quality* at
the same asymptotic cost: the DP is still $\Theta(mn)$, with a slightly larger constant
because the step cost is evaluated for each of the three parents. It adds the least new
edge energy on all three test images (Table 5) and visibly keeps lines straighter.

### Other improvements we considered

- **Remove several seams per energy map.** Computing the map once and removing a batch of
  non-crossing seams would cut the work per seam, but the later seams would be chosen on
  stale energies. Item 1c asks for recomputation after every seam, so we did not do this.
- **A faster max-flow** (Dinic, push-relabel, Boykov-Kolmogorov) would speed up graph cut by
  a large factor but not change the conclusion that DP is the right method for images.
- **Avoid copying the image on removal** by shifting pixels in place. This saves an
  allocation per seam but not the $\Theta(mn)$ moves.
