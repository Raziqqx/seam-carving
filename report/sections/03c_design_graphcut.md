## Graph cut (item 4)

### The graph

For a vertical seam we build a flow network with one node per pixel plus a source $s$ and a
sink $t$ (`list_seam_graph_edges` in `src/graphcut.cpp`, Figure 3):

| Edge | Capacity | Purpose |
|---|---|---|
| $s \to (i, 0)$ for every row | $\infty$ | the first column is always on the source side |
| forward $(i, j) \to (i, j+1)$ | $e[i][j]$ | cutting it means "remove pixel $(i, j)$" |
| backward $(i, j+1) \to (i, j)$ | $\infty$ | the cut crosses each row only once |
| diagonal $(i, j) \to (i-1, j-1)$ and $(i, j) \to (i+1, j-1)$ | $\infty$ | neighbouring rows are cut at most one column apart |
| $(i, n-1) \to t$ for every row | $e[i][n-1]$ (see the finding below; the brief says $\infty$) | cutting it means "remove the last pixel of the row" |

The network has $V = mn + 2$ nodes and $E = 4mn - 2m - 2n + 2$ edges (checked by a test).

![Figure 3. The flow network for a 3 x 4 image. Blue edges have finite capacity; grey edges are infinite and can never be cut.](figures/fig_graph_construction.png){width=88%}

### Why a finite cut is a seam (proof sketch)

An $s$-$t$ cut is a split of the nodes into $S$ (containing $s$) and $T$ (containing $t$).
Its capacity is the sum of the capacities of the edges that go *from $S$ to $T$*. A cut is
finite exactly when no infinite edge goes from $S$ to $T$; in other words, if $u \in S$ and
$u \to v$ is an infinite edge, then $v \in S$ too.

1. **Each row is cut exactly once.** $(i, 0) \in S$ because of the infinite source edge.
   If $(i, j+1) \in S$, the infinite backward edge forces $(i, j) \in S$. So the $S$ pixels
   of row $i$ are a prefix: columns $0, 1, \dots, c_i$ for some $c_i$. The only forward
   edge of row $i$ leaving $S$ is the one out of $(i, c_i)$.
2. **The cut is connected.** If $(i, j) \in S$ with $j \ge 1$, the infinite diagonal edges
   force $(i-1, j-1) \in S$ and $(i+1, j-1) \in S$. Taking $j = c_i$ gives
   $c_{i-1} \ge c_i - 1$ and $c_{i+1} \ge c_i - 1$. Applying the same statement to the
   neighbouring rows gives the other direction, so $|c_i - c_{i+1}| \le 1$ for all $i$:
   exactly the seam condition.
3. **Capacity = seam energy.** The edges from $S$ to $T$ are the forward edges out of
   $(i, c_i)$ (or the sink edge when $c_i = n - 1$), one per row, with capacity
   $e[i][c_i]$. Backward and diagonal edges that cross the cut go from $T$ to $S$ and do
   not count. So the capacity is $\sum_i e[i][c_i]$, the energy of the seam
   $(c_0, \dots, c_{m-1})$.
4. **Every seam is a finite cut.** Conversely, for any seam, taking $S$ = "all pixels at or
   left of the seam" satisfies all the infinite-edge conditions.

So finite cuts and seams correspond one to one with equal cost. The minimum cut is therefore
the minimum-energy seam, and by the max-flow min-cut theorem its capacity equals the maximum
flow. **The max-flow value is the optimal seam cost**, the same number the DP finds.

### Worked example (2 x 3)

Small example to check the proof above by hand. Energy map (rows top to bottom):

```
4  1  3
2  5  1
```

A seam picks one column per row, (c0, c1), with the two columns at most 1 apart, so there
are 7 seams: (0,0)=6, (0,1)=9, (1,0)=3, (1,1)=6, (1,2)=2, (2,1)=8, (2,2)=4. The best one is
(1,2) with cost 2.

The graph has $V = 8$ nodes and $E = 4 \cdot 6 - 4 - 6 + 2 = 16$ edges. Edmonds-Karp finds
2 augmenting paths, one along each row:

- $s \to (0,0) \to (0,1) \to (0,2) \to t$, capacities 4, 1, 3, so it pushes 1 (flow = 1)
- $s \to (1,0) \to (1,1) \to (1,2) \to t$, capacities 2, 5, 1, so it pushes 1 (flow = 2)

Now $(0,1) \to (0,2)$ and $(1,2) \to t$ are full and there is no path left to $t$ (nothing
infinite goes into $(0,2)$ since diagonal edges only point left). The BFS from $s$ reaches
$(0,0), (0,1), (1,0), (1,1), (1,2)$, so $S$ is those plus $s$, and $T = \{(0,2), t\}$. The
edges cut are $(0,1) \to (0,2)$ (capacity 1) and $(1,2) \to t$ (capacity 1), total 2, which
matches the max flow. Taking the rightmost $S$ pixel in each row gives seam (1, 2), same as
the DP. Row 1 can be one column to the right of row 0 because the diagonal edge
$(1,2) \to (0,1)$ only needs $(0,1)$ to be in $S$, which it is.

With the literal infinite sink link from the brief, $(1,2)$ can't be in $S$, so the best it
can do is (1,0) with cost 3 instead of 2. So this small grid already shows the problem in
the finding below.

### Finding: the graph exactly as worded in the brief cannot remove the last column

The brief connects every last-column pixel to the sink with *infinite* capacity. We
implemented that first (it is still available as `SinkLink::InfiniteToSink`) and compared it
with the DP, as planned. The costs did **not** always agree, and the reason is structural:

- with an infinite edge $(i, n-1) \to t$, the pixel $(i, n-1)$ can never be in $S$, so
  $c_i \le n - 2$ in every row;
- the energy $e[i][n-1]$ is not the capacity of any edge, so it never influences the result.

The literal graph therefore returns the best seam *within columns $0 .. n-2$*. Whenever the
true optimum uses the last column, it is missed. A $3 \times 3$ example from our tests:

```
energy        DP optimum: column 2 in every row, cost 3
9  9  1       literal graph: max flow 27, seam inside columns 0-1
9  9  1       our graph:     max flow 3,  seam = column 2 in every row
9  9  1
```

**Fix.** Think of the image as having one imaginary extra column $n$ that is merged into the
sink. Then the brief's rule "forward edge $(i, j) \to (i, j+1)$ with capacity $e[i][j]$"
also applies to $j = n - 1$, giving an edge $(i, n-1) \to t$ with capacity $e[i][n-1]$. This
is `SinkLink::EnergyToSink`, our default. Steps 1 to 4 above then hold for all $n$ columns.

**Evidence.** `tests/test_graphcut.cpp` checks on 768 random grids ($1 \times 1$ to
$8 \times 8$, wide and narrow value ranges) that with our default graph: seam cost = DP
cost = max-flow value. It also checks that the literal graph equals the DP run on the image
without its last column (420 grids), which confirms the explanation above. On real
photographs the difference is usually invisible, because the optimum is rarely at the very
right edge, but an algorithm that is "optimal except at the border" is not optimal.

### Solving it: our own Edmonds-Karp

`FlowNetwork` in `src/maxflow.cpp` stores the *residual graph* as adjacency lists. Every
edge is stored next to its reverse edge (indices $2k$ and $2k+1$), so the partner of edge
`e` is `e ^ 1`.

```
max_flow(s, t):
    flow = 0
    while BFS from s reaches t using only edges with residual capacity > 0:
        bottleneck = smallest residual capacity on the BFS path
        for each edge on the path:
            edge.residual    -= bottleneck      # use up capacity
            reverse.residual += bottleneck      # allow this flow to be undone later
        flow += bottleneck
    return flow
```

- **Why BFS.** Edmonds-Karp is Ford-Fulkerson with the *shortest* augmenting path. This
  bounds the number of augmentations by $O(VE)$ whatever the capacities are; plain
  Ford-Fulkerson with an arbitrary path is only bounded by the flow value.
- **Why reverse edges.** A later path may need to redirect flow pushed earlier. Without
  reverse edges the algorithm can get stuck below the maximum (a test shows this case).
- **Infinite capacity** is a very large number (`kInfiniteCapacity`), larger than any
  possible sum of energies, so an infinite edge is never saturated.

**Reading the seam.** After the flow is maximal, a BFS from $s$ over edges with residual
capacity $> 0$ gives the source side $S$ (`reachable_from`). By step 1 the $S$ pixels of
each row are a prefix, so the seam pixel is the *rightmost $S$ pixel of the row*
(`extract_seam_from_cut`).

**Which optimal seam?** When several seams have the same minimum cost, the residual BFS
returns the *smallest* source side of any minimum cut, i.e. in every row the leftmost column
used by any optimal seam (tested by brute force). DP breaks ties by its own rule. So DP and
graph cut can return different seams with the same cost; this matters when comparing the
two over many removals ([5.3](#seam-cost)).

**Alternatives considered.** Dinic's algorithm ($O(V^2E)$), push-relabel, or the
Boykov-Kolmogorov solver used in vision libraries would be much faster than Edmonds-Karp on
grid graphs. We kept Edmonds-Karp because the brief names it, it is the algorithm from the
course, and each of its parts is short enough to explain line by line. As an independent
check, an optional test compares our max-flow value with Boost Graph Library's
`edmonds_karp_max_flow` on 120 seam graphs and 100 random graphs; Boost is used nowhere
else.