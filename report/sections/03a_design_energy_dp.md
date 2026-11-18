# Algorithm design

All three seam finders share one pipeline (Figure 2). Only the middle step differs.

```
image --> grayscale --> energy map --> find one seam --> remove seam --> smaller image
              ^                        (DP | greedy |                        |
              |                         graph cut)                           |
              +---------------- repeat until the target size ----------------+
```

*Figure 2. The seam-carving loop (`carve_to_size` in `src/carve.cpp`).*

Every seam finder has the same interface: it takes an energy map and returns a vertical
seam as a list `seam[row] = column`. Horizontal seams reuse it through a transpose
([3.5](#remove-and-repeat-item-1c)).

## Energy map (item 1a)

**Definition.** We convert the image to 8-bit grayscale $I$ and use the L1 Sobel gradient
magnitude (the $e_1$ energy of Avidan and Shamir):

$$ e[i][j] = |G_x(i, j)| + |G_y(i, j)|, $$

$$ G_x = \begin{bmatrix} -1 & 0 & 1 \\ -2 & 0 & 2 \\ -1 & 0 & 1 \end{bmatrix} * I, \qquad
   G_y = \begin{bmatrix} -1 & -2 & -1 \\ 0 & 0 & 0 \\ 1 & 2 & 1 \end{bmatrix} * I. $$

A pixel in a flat area (sky) has energy near 0; a pixel on an edge has a high energy, at
most 1530 ($|G_x|$ and $|G_y|$ cannot both reach 1020 at once; we use the simple bound
$2 \cdot 1020 = 2040$ below).

**Boundary handling.** Pixels outside the image are read as the nearest border pixel
("replicate"; `clamp_index` in our code, `BORDER_REPLICATE` in OpenCV). We chose this over
zero padding because zero padding would create a fake bright-to-black edge around the whole
image and make border pixels look important.

**Design choices and alternatives.**

| Choice | Why | Alternative considered |
|---|---|---|
| L1 norm $\lvert G_x\rvert + \lvert G_y\rvert$ | Exact integers: seam costs can be compared with `==` in tests, and max-flow capacities are exact | L2 norm $\sqrt{G_x^2 + G_y^2}$: needs floating point, and results look the same |
| Sobel $3 \times 3$ | Smooths a little, so single noisy pixels matter less | Simple difference $\lvert I[i][j+1] - I[i][j-1]\rvert$: implemented as the `neighbour` energy, the cost the brief suggests for graph cut |
| Grayscale | One channel, 3x less work | Per-channel gradients summed: more faithful to colour edges, 3x the cost |
| Two implementations | `compute_gradient_energy` (OpenCV `Sobel`) is the default; `compute_gradient_energy_manual` is our own double loop | A test checks they agree on every pixel, so the report can reason about code we wrote |

**Forward energy (option).** The gradient energy measures what a pixel *has*. Rubinstein,
Shamir and Avidan (2008) instead measure what removing it *adds*: after the removal, pixels
that were not neighbours become neighbours, creating new edges. For a seam step into
$(i, j)$ the new-edge cost is

$$ C_U = |I[i][j+1] - I[i][j-1]| \quad\text{(straight step)}, $$
$$ C_L = C_U + |I[i-1][j] - I[i][j-1]|, \qquad C_R = C_U + |I[i-1][j] - I[i][j+1]| $$

for steps from the upper-left and upper-right. This cost depends on the *direction of the
step*, not only on the pixel, so it is added inside the minimum of the DP recurrence
(`find_vertical_seam_dp_forward`). Results are in [5.5](#forward-energy).

## Dynamic programming (item 1b)

The recurrence and its justification are in [2.2](#item-2a-ii-relating-the-problem-to-subproblems).
The implementation has two short functions (the three-way minimum is the helper
`best_parent_col`, shared by every DP variant).

```
build_cumulative_energy(e):                  # fills M and parent, top row first
    for j in 0..n-1:  M[0][j] = e[0][j];  parent[0][j] = -1
    for i in 1..m-1:
        for j in 0..n-1:
            p = j                                         # straight up always exists
            if j-1 >= 0 and M[i-1][j-1] < M[i-1][p]: p = j-1   # up-left
            if j+1 <  n and M[i-1][j+1] < M[i-1][p]: p = j+1   # up-right
            M[i][j] = e[i][j] + M[i-1][p];  parent[i][j] = p

backtrack_seam(M, parent):
    j = column of the smallest M[m-1][j]     # leftmost on ties
    for i = m-1 down to 0:
        seam[i] = j;  j = parent[i][j]
    return seam
```

**Ties.** The strict `<` means: prefer straight up, then up-left, then up-right; and the
leftmost minimum in the bottom row. Any rule gives an optimal seam; a fixed rule makes the
output deterministic and testable (a constant image always gives the straight seam in
column 0).

**Correctness.** By induction on the row, $M[i][j]$ is the cost of the cheapest seam ending
at $(i, j)$ (optimal substructure, [2.2](#item-2a-ii-relating-the-problem-to-subproblems)),
so the bottom-row minimum is the optimal cost and the parents trace one seam with that
cost. We do not rely on the proof alone: the tests compare the DP cost with a brute-force
search over all seams on 540 random grids of every size from $1 \times 1$ to $6 \times 6$.

**Alternatives considered.** (1) Top-down recursion with memoisation: same $\Theta(mn)$
time, but recursion depth $m$ and slower in practice. (2) Shortest path in a graph (Dijkstra):
the pixels form a directed acyclic graph with edges only from row $i$ to row $i + 1$, and
the DP *is* the shortest-path algorithm for such a graph, processing nodes in row order; a
priority queue would only add a $\log$ factor. (3) Storing the parent as a step
$-1 / 0 / +1$ in one byte instead of a column: used in the memory-optimised variant
`find_vertical_seam_dp_rolling` ([6.4](#improvements-item-5d)).
