| Stage | Image (w x h) | Serial (ms) | 1 thread | 2 threads | 4 threads | 8 threads | 16 threads |
|---|---|---|---|---|---|---|---|
| Energy map | 500 x 339 | 1.04 | 0.9361 (1.11x) | 0.4715 (2.21x) | 0.2698 (3.86x) | 0.2642 (3.94x) | 0.2003 (5.20x) |
| DP table (parallel inside each row) | 500 x 339 | 0.4360 | 0.7992 (0.55x) | 1.00 (0.43x) | 1.41 (0.31x) | 1.96 (0.22x) | 1.95 (0.22x) |
| Energy map, OpenCV Sobel (for reference) | 500 x 339 | 0.2963 | - | - | - | - | - |
| Energy map | 3000 x 2034 | 30.62 | 36.49 (0.84x) | 16.41 (1.87x) | 8.72 (3.51x) | 7.34 (4.17x) | 4.34 (7.06x) |
| DP table (parallel inside each row) | 3000 x 2034 | 16.39 | 24.74 (0.66x) | 24.25 (0.68x) | 14.40 (1.14x) | 16.68 (0.98x) | 18.43 (0.89x) |
| Energy map, OpenCV Sobel (for reference) | 3000 x 2034 | 10.94 | - | - | - | - | - |
