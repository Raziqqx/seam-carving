# Answers to Item 2

## Item 2a-i: the number of seams grows at least exponentially in m

**Claim.** Let $S(m, n)$ be the number of vertical seams of an $m \times n$ image. If
$n > 1$ then

$$ n \cdot 2^{m-1} \;\le\; S(m, n) \;\le\; n \cdot 3^{m-1}, $$

so in particular $S(m, n) \ge 2^m$: the number of seams grows at least exponentially in $m$.

**Proof.** A seam is a sequence of columns $(c_0, c_1, \dots, c_{m-1})$ with
$0 \le c_i \le n-1$ and $|c_i - c_{i-1}| \le 1$ for every $i \ge 1$. We count the ways to
build such a sequence row by row.

*Row 0.* Any of the $n$ columns can be chosen: $n$ choices.

*Every later row.* Suppose the seam is at column $c$ in the previous row. The next column
may be $c - 1$, $c$ or $c + 1$, as long as it lies inside the image.

- Column $c$ itself is always allowed.
- Because $n > 1$, at least one of $c - 1$ and $c + 1$ is inside the image (if $c = 0$ then
  $c + 1 \le n - 1$ exists; otherwise $c - 1 \ge 0$ exists).

So every partial seam can be extended in at least 2 and at most 3 ways, whatever column it
is in. Different choices give different sequences, so the numbers of choices multiply. With
$m - 1$ rows after the first:

$$ n \cdot 2^{m-1} \le S(m, n) \le n \cdot 3^{m-1}. $$

Since $n \ge 2$, the lower bound is at least $2 \cdot 2^{m-1} = 2^m$. $\blacksquare$

The assumption $n > 1$ is necessary: for $n = 1$ there is exactly one seam (every row must
use column 0), which does not grow at all.

**Induction form of the same argument.** Let $P(m)$ be "$S(m, n) \ge n \cdot 2^{m-1}$".
Base: $S(1, n) = n$. Step: every seam of the first $m - 1$ rows extends to at least two
different seams of $m$ rows, and seams with different prefixes stay different, so
$S(m, n) \ge 2\,S(m-1, n) \ge n \cdot 2^{m-1}$.

**A tighter bound.** Let $w_i[j]$ be the number of seams of rows $0..i$ that end at column
$j$. Then $w_0[j] = 1$ and

$$ w_i[j] = w_{i-1}[j-1] + w_{i-1}[j] + w_{i-1}[j+1] \quad (\text{terms outside the image are } 0), $$

and $S(m, n) = \sum_j w_{m-1}[j]$. This is the seam-finding recurrence of item 2a-ii with
"sum" in place of "min". In matrix form $w_i = A\,w_{i-1}$, where $A$ is the $n \times n$
tridiagonal matrix with ones on the three central diagonals. Its largest eigenvalue is
$\lambda_n = 1 + 2\cos\!\big(\tfrac{\pi}{n+1}\big)$, so

$$ S(m, n) = \Theta\!\left(\lambda_n^{\,m}\right), \qquad \lambda_n = 1 + 2\cos\frac{\pi}{n+1}. $$

- $n = 2$: $\lambda_2 = 2$ and in fact $S(m, 2) = 2^m$ exactly (both columns always have
  exactly two choices), so the lower bound above is tight.
- $n = 3$: $\lambda_3 = 1 + \sqrt 2 \approx 2.414$; the counts are 3, 7, 17, 41, 99, ...
- $n \to \infty$: $\lambda_n \to 3$, so for wide images the count approaches the upper
  bound $n \cdot 3^{m-1}$ (only seams that touch the left or right border lose choices).

**Checked by the code.** `tests/test_seam_count.cpp` lists every seam by brute force for
all $m \le 7$, $n \le 6$ and checks that the count (a) equals the recurrence, (b) lies
between $n \cdot 2^{m-1}$ and $n \cdot 3^{m-1}$, (c) equals $2^m$ for $n = 2$ and 1 for
$n = 1$, and (d) for $n = 3$ follows $S(m) = 2S(m-1) + S(m-2)$, whose growth rate is
$1 + \sqrt 2$.

**Why this matters.** Our tower test image has $m = 339$ rows, so by the bound above it has
more than $2^{339}$ vertical seams.
Trying them all is impossible, which is the motivation for dynamic programming.

## Item 2a-ii: relating the problem to subproblems

**Subproblem.** Let $M[i][j]$ be the smallest total energy of any seam that starts in row 0
and ends at pixel $(i, j)$.

**Optimal substructure.** Take a cheapest seam $P$ that ends at $(i, j)$ with $i \ge 1$.
Its second-to-last pixel is $(i-1, p)$ for some $p \in \{j-1, j, j+1\}$. Remove the last
pixel: what is left, $P'$, is a seam ending at $(i-1, p)$. $P'$ must be a *cheapest* seam
ending at $(i-1, p)$. If some other seam $Q'$ ending at $(i-1, p)$ were cheaper, then $Q'$
followed by $(i, j)$ would be a valid seam (the last step is still between adjacent
columns) costing $\text{cost}(Q') + e[i][j] < \text{cost}(P)$, contradicting that $P$ is
cheapest. This "cut and paste" argument works because the cost is a plain sum and because
whether the final step is allowed depends only on the column $p$, not on how the seam
reached it.

**Recurrence.**

$$ M[0][j] = e[0][j] $$
$$ M[i][j] = e[i][j] + \min\big(M[i-1][j-1],\; M[i-1][j],\; M[i-1][j+1]\big) \quad (i \ge 1) $$

**Edges.** In column 0 there is no $M[i-1][j-1]$, and in column $n - 1$ there is no
$M[i-1][j+1]$; the missing term is left out of the minimum (equivalently, treated as
$+\infty$). With $n = 1$ only the middle term remains.

**Answer and seam recovery.** The cheapest seam of the whole image costs
$\min_j M[m-1][j]$. To get the seam itself we store, for every cell, which parent column
gave the minimum (`parent_col[i][j]`). Start at the column of the smallest entry in the
bottom row and follow the parents up to row 0.

**Why dynamic programming is the right tool.**

1. *Optimal substructure* (shown above): an optimal solution is built from optimal
   solutions of smaller problems.
2. *Overlapping subproblems*: a naive recursion on the recurrence calls itself up to three
   times per row, so its running time is exponential: it visits every seam, so by item
   2a-i it makes at least $n \cdot 2^{m-1}$ calls for $n \ge 2$
   (exponential in $m$). But there are only $mn$ different subproblems;
   $M[i-1][j]$ alone is needed by three cells of row $i$. Solving each subproblem once and
   storing it in a table gives $\Theta(mn)$ time.
3. *A valid order exists*: row $i$ depends only on row $i - 1$, so filling the table from
   the top row down means every value is ready when needed.

**Worked example.** Figure 1 shows the tables for a $4 \times 4$ energy grid. The bottom row
of $M$ has minimum 6 (at columns 0 and 2; we take the leftmost), and following the parents
gives the seam with columns $(0, 1, 1, 0)$ and cost $1 + 2 + 2 + 1 = 6$. The same example is
checked value by value in `tests/test_dp.cpp`.

![Figure 1. Energy grid (left) and cumulative table with parent arrows (right). The optimal seam is shaded.](figures/fig_dp_table.png){width=62%}

## Item 2b: would divide and conquer make the Sobel computation faster?

**The baseline is already linear.** Convolving an $m \times n$ image with a $k \times k$
kernel directly costs $\Theta(mn\,k^2)$ operations. For Sobel, $k = 3$ is a constant, so the
direct method is $\Theta(mn)$: 9 reads and a few additions per pixel
(`sobel_energy_at` in `src/energy.cpp`). This is also a *lower bound*: every one of the $mn$
outputs has to be written, so no algorithm can be asymptotically faster than
$\Theta(mn)$.

**Plain divide and conquer.** Split the image into four quadrants, compute the gradient of
each one, and combine. The quadrants are almost independent: a pixel on a quadrant border
only needs one extra row or column from the neighbour (a "halo"). With $N = mn$ pixels the
running time is

$$ T(N) = 4\,T(N/4) + O(\sqrt N), $$

where $O(\sqrt N)$ is the border work. By the master theorem ($a = 4$, $b = 4$,
$f(N) = O(N^{1/2})$ is polynomially smaller than $N^{\log_4 4} = N$) this gives
$T(N) = \Theta(N)$. So divide and conquer does **not** reduce the amount of work. On one
core it only adds recursion overhead to the same 9 operations per pixel.

**FFT-based convolution (a divide-and-conquer algorithm).** The fast Fourier transform is a
classic divide-and-conquer algorithm, and convolution can be done as: FFT of the image, FFT
of the kernel, multiply point by point, inverse FFT. This costs $\Theta(mn \log(mn))$,
*independent of the kernel size*, against $\Theta(mn\,k^2)$ for the direct method. It
wins only when $k^2$ is larger than about $\log(mn)$ times the constant factor of the FFT,
which means large kernels (wide blurs, template matching). For a $3 \times 3$ kernel it is
*slower*: 9 integer operations per pixel are replaced by about
$\log_2(500 \cdot 339) \approx 17$ complex floating-point butterfly steps per pixel for each
of three transforms, plus extra memory. Floating-point rounding is not the obstacle (for a
$3 \times 3$ kernel, rounding the result to the nearest integer would recover the exact
integer energies); the cost is.

**Separable kernels.** The Sobel kernel is an outer product of two 1-D filters,
$[1\;2\;1]^T \cdot [-1\;0\;1]$. Filtering the rows with one and the columns with the other
costs $2k$ operations per pixel instead of $k^2$ (6 instead of 9). This is a constant-factor
gain and it is the technique OpenCV's `Sobel` uses; it is not divide and conquer.

**Where divide and conquer does help.**

- *Parallelism.* The four quadrants are independent, so they can run on different cores.
  The work stays $\Theta(mn)$ but with $p$ cores the time becomes about $\Theta(mn/p)$,
  and the recursion depth (span) is only $O(\log(mn))$. Our OpenMP energy map
  (`compute_gradient_energy_parallel`) is the simplest version of this idea: it splits the
  rows into one block per thread. Measured speed-up over our own serial loop: up to
  @@threads_energy_large_best@@x with @@threads_energy_large_best_n@@ threads on a
  @@threads_large_size@@ image ([6.4](#improvements-item-5d)).
- *Cache use.* Recursively splitting until a tile fits in the cache keeps the three image
  rows a $3 \times 3$ filter needs in fast memory. For a row-by-row scan of a 3-row window
  this matters little, because the scan is already cache friendly.

**Verdict.** For the Sobel filter, divide and conquer cannot improve the asymptotic cost:
the direct method is already optimal at $\Theta(mn)$. FFT, the divide-and-conquer
convolution, pays off only for large kernels, not for $3 \times 3$. The real benefit of
divide and conquer here is as a way to *organise* the work for multiple cores (and, less
importantly, for the cache), which changes the wall-clock time by a constant factor but not
the total work. In seam carving a much larger saving comes from not recomputing the map at
all: after removing a seam only the pixels next to it change, so $O(m)$ Sobel evaluations
replace $O(mn)$ ([6.4](#improvements-item-5d)).
