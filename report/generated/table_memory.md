| DP variant | Image (w x h) | Table memory (MB) | Seam search (ms) |
|---|---|---|---|
| Full tables (cost + parent, 4-byte ints) | 3000 x 2034 | 48.8 | 16.57 |
| Two rolling rows + 1-byte steps | 3000 x 2034 | 6.1 | 16.32 |
