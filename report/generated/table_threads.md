| Stage | Image (w x h) | Serial (ms) | 1 thread | 2 threads | 4 threads | 8 threads | 16 threads |
|---|---|---|---|---|---|---|---|
| Energy map | 500 x 339 | 0.9501 | 0.9488 (1.00x) | 0.4830 (1.97x) | 0.3038 (3.13x) | 0.2705 (3.51x) | 0.4167 (2.28x) |
| DP table (parallel inside each row) | 500 x 339 | 0.5328 | 0.7987 (0.67x) | 0.9801 (0.54x) | 1.20 (0.44x) | 2.01 (0.27x) | 3.14 (0.17x) |
| Energy map, OpenCV Sobel (for reference) | 500 x 339 | 0.2850 | - | - | - | - | - |
| Energy map | 3000 x 2034 | 33.04 | 36.07 (0.92x) | 18.22 (1.81x) | 9.77 (3.38x) | 8.93 (3.70x) | 5.14 (6.43x) |
| DP table (parallel inside each row) | 3000 x 2034 | 33.76 | 42.79 (0.79x) | 33.53 (1.01x) | 29.64 (1.14x) | 31.68 (1.07x) | 54.50 (0.62x) |
| Energy map, OpenCV Sobel (for reference) | 3000 x 2034 | 10.70 | - | - | - | - | - |
