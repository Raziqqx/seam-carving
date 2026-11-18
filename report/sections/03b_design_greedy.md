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

**How bad can it get?** The same trap works for any number of rows. Take an $m \times 3$
grid whose top row is 1, 2, 9 and whose other rows are 9, 9, 1. Greedy starts in column 0
and stays there, paying $1 + 9(m-1)$. DP starts in column 1 at cost 2 and moves to the cheap
column 2 on each row, paying $2 + (m-1)$. For $m \ge 2$ the ratio of greedy's cost to DP's is
$(1 + 9(m-1))/(2 + (m-1)) = (9m-8)/(m+1)$: $19/4$ for $m = 3$ (the $3 \times 3$ example),
approaching the trap value 9 as $m$ grows. So this family's ratio is bounded, but greedy has
no constant-factor guarantee in general: lower the cheap values by one (top row 0, 1, 9;
other rows 9, 9, 0). Greedy still starts in column 0 and pays $9(m-1)$, while DP pays
only 1, so the ratio $9(m-1)$ grows linearly with the number of rows.

**What greedy does guarantee.** The seam is always valid (one pixel per row, neighbours at
most one column apart), and its cost can never be below the DP cost, because DP is the
minimum over all valid seams. Both facts are tested on 500 random grids, together with a
check that greedy really is strictly worse on some of them.

**Alternatives considered.** Starting from every column and keeping the best of the $n$
greedy seams would cost $\Theta(nm)$, the same as DP, and still not be optimal. A
"look-ahead" of $d$ rows trades time for quality and becomes DP when $d = m$. We kept the
plain version the brief describes, as the baseline for the comparison in
[5](#results) and [6.3](#comparison-of-the-three-methods-item-5c).
