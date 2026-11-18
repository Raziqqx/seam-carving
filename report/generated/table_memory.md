| DP variant | Image (w x h) | Table memory (MB) | Seam search (ms) |
|---|---|---|---|
| Full tables (cost + parent, 4-byte ints) | 3000 x 2034 | 48.8 | 17.09 |
| Two rolling rows + 1-byte steps | 3000 x 2034 | 6.1 | 15.79 |
