# Conclusion

We implemented seam carving three ways and compared them on the same pipeline.

- **Dynamic programming** is exact, linear in the number of pixels and simple. It is the
  method to use.
- **Greedy** has a faster seam search but removes @@cost_ratio_min@@ to @@cost_ratio_max@@
  times as much energy on our three photographs (and exactly as much on the synthetic
  checkerboard), visibly damages the subject, and makes the full loop only
  @@loop_speedup_greedy@@ times faster. The hand-built $3 \times 3$ grid shows why a local choice cannot be trusted.
- **Graph cut** reaches the same optimum as DP, confirmed on every random grid we tried,
  provided the last column is linked to the sink with its energy rather than with infinite
  capacity. With Edmonds-Karp it is thousands of times slower than DP; its value is that
  the same formulation extends to problems (such as video) where DP does not apply.

The theory and the measurements agree: the seam count is exponential (so brute force is
impossible), DP runs in $\Theta(mn)$, and the benchmarks show a constant cost per pixel
(@@dp_ns_per_pixel_best_range@@ ns) and a log-log slope of about 1 (@@exp_dp@@). For item 5d, the DP needs only two rows of costs plus one byte per pixel for
backtracking; multithreading helps the energy map but not the DP table; and updating the
energy only next to the removed seam speeds up the whole carving loop by @@inc_total_speedup@@x.

**Limitations.** Images with straight structure across the whole width (our bridge) are
distorted by any seam-carving method; removing width before height is not the optimal
order; and our graph cut is a teaching implementation, not a fast one.

# References {.unnumbered}

1. S. Avidan and A. Shamir, "Seam carving for content-aware image resizing," *ACM
   Transactions on Graphics*, vol. 26, no. 3, article 10, 2007.
   <https://dl.acm.org/doi/10.1145/1275808.1276390>
2. M. Rubinstein, A. Shamir and S. Avidan, "Improved seam carving for video retargeting,"
   *ACM Transactions on Graphics*, vol. 27, no. 3, article 16, 2008.
   <https://dl.acm.org/doi/10.1145/1360612.1360615>
3. T. H. Cormen, C. E. Leiserson, R. L. Rivest and C. Stein, *Introduction to Algorithms*,
   3rd ed., MIT Press, 2009. Chapter 15 (dynamic programming; Problem 15-8 is seam
   carving), Chapter 26 (maximum flow; Section 26.2 for Ford-Fulkerson and Edmonds-Karp,
   Theorem 26.6 for max-flow min-cut), Section 4.5 (master theorem), Chapter 30 (FFT).
4. J. Edmonds and R. M. Karp, "Theoretical improvements in algorithmic efficiency for
   network flow problems," *Journal of the ACM*, vol. 19, no. 2, pp. 248-264, 1972.
5. D. S. Hirschberg, "A linear space algorithm for computing maximal common subsequences,"
   *Communications of the ACM*, vol. 18, no. 6, pp. 341-343, 1975.
6. Y. Boykov and V. Kolmogorov, "An experimental comparison of min-cut/max-flow algorithms
   for energy minimization in vision," *IEEE Transactions on Pattern Analysis and Machine
   Intelligence*, vol. 26, no. 9, pp. 1124-1137, 2004.
7. Wikipedia, "Seam carving." <https://en.wikipedia.org/wiki/Seam_carving>
8. A. Das, "Real-world dynamic programming: seam carving," 2019.
   <https://avikdas.com/2019/05/14/real-world-dynamic-programming-seam-carving.html>
9. OpenCV documentation, `cv::Sobel` and image filtering.
   <https://docs.opencv.org/4.x/d4/d86/group__imgproc__filter.html>
10. Dear ImGui (user-interface library used by the app). <https://github.com/ocornut/imgui>
11. Test images from Wikimedia Commons; authors and licences are listed in
    `data/inputs/SOURCES.md`.
