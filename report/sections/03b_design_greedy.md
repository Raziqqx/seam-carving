## Greedy (item 3)

The greedy method makes one local decision per row and never looks back.

```
find_vertical_seam_greedy(e):
    seam[0] = column of the smallest e[0][j]             # leftmost on ties
    for i in 1..m-1:
        c = seam[i-1]
        seam[i] = the cheapest of e[i][c-1], e[i][c], e[i][c+1] that lies inside the image
                  (ties: straight down, then down-left, then down-right)
    return seam
```

It needs no table: one scan of the top row and three comparisons per row,
$\Theta(n + m)$ time and $\Theta(m)$ space for the seam itself.

**It is not optimal.** The greedy-choice property does not hold for this problem: the
cheapest pixel now can lead into an expensive region later. The smallest example we found is
a $3 \times 3$ grid, used as a test in `tests/test_greedy.cpp`:

```
energy          greedy seam (cost 19)     optimal seam, DP (cost 4)
1  2  9         [1] 2  9                   1 [2] 9
9  9  1         [9] 9  1                   9  9 [1]
9  9  1         [9] 9  1                   9  9 [1]
```

Greedy starts at the 1 in the top row because it is the smallest value there. From column 0
it can only reach columns 0 and 1, where everything costs 9. DP starts at the slightly more
expensive 2, from which the cheap right column is reachable. The first choice looked best
locally and was wrong globally.

**What greedy does guarantee.** The seam is always valid (one pixel per row, neighbours at
most one column apart), and its cost can never be below the DP cost, because DP is the
minimum over all valid seams. Both facts are tested on 500 random grids, together with a
check that greedy really is strictly worse on some of them.

**Alternatives considered.** Starting from every column and keeping the best of the $n$
greedy seams would cost $\Theta(nm)$, the same as DP, and still not be optimal. A
"look-ahead" of $d$ rows trades time for quality and becomes DP when $d = m$. We kept the
plain version the brief describes, as the baseline for the comparison in
[5](#results) and [6.3](#comparison-of-the-three-methods-item-5c).
