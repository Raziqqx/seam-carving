#!/usr/bin/env python3
"""Turns the benchmark CSV files into the report's figures, tables and numbers.

Usage:  python benchmarks/plot_results.py <results dir> <report dir>

Reads   <results dir>/csv/*.csv and the result pictures in <results dir>/<image>/
Writes  <report dir>/figures/*.png        plots, picture panels and diagrams
        <report dir>/generated/*.md       Markdown tables included by the report
        <report dir>/generated/facts.txt  key=value numbers quoted in the report text

Nothing in the report is typed by hand from a benchmark: the text uses @@key@@
placeholders that report/build_report.sh fills in from facts.txt.
"""
import csv
import math
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # no window needed
import matplotlib.image as mpimg
import matplotlib.pyplot as plt
import numpy as np

# One fixed colour, marker and label per algorithm, used in every figure.
STYLE = {
    "dp": {"color": "#2a78d6", "marker": "o", "label": "Dynamic programming"},
    "greedy": {"color": "#eb6834", "marker": "s", "label": "Greedy"},
    "graphcut": {"color": "#1baf7a", "marker": "^", "label": "Graph cut (Edmonds-Karp)"},
}
IMAGES = ["tower", "bridge", "castle"]
# Synthetic image made by bench.cpp (full size only, DP and greedy only).
CHECKERBOARD = "checkerboard"
CHECKERBOARD_SQUARE = 25  # must match kCheckerboardSquare in bench.cpp
SOBEL_MAX = 1020  # largest Sobel response of 8-bit data: 4 * 255
TEXT_COLOUR = "#333333"
GRID_COLOUR = "#dddddd"

plt.rcParams.update({
    "font.size": 10,
    "axes.edgecolor": "#888888",
    "axes.labelcolor": TEXT_COLOUR,
    "xtick.color": TEXT_COLOUR,
    "ytick.color": TEXT_COLOUR,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.color": GRID_COLOUR,
    "grid.linewidth": 0.6,
    "legend.frameon": False,
    "savefig.dpi": 160,
    "savefig.bbox": "tight",
})


def read_csv(path):
    """Returns the rows of a CSV file as a list of dictionaries."""
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def median_over_repetitions(rows, value_columns):
    """Combines the repetitions of a timing experiment.

    Rows that agree in every column except "repetition" and the value columns are merged
    into one row holding the MEDIAN of each value column, plus "<column>_min" and
    "<column>_max" with the smallest and largest value seen.
    """
    groups = {}
    for row in rows:
        key = tuple((name, value) for name, value in row.items()
                    if name != "repetition" and name not in value_columns)
        groups.setdefault(key, []).append(row)
    merged = []
    for key, members in groups.items():
        row = dict(key)
        for column in value_columns:
            values = [float(member[column]) for member in members]
            row[column] = str(float(np.median(values)))
            row[column + "_min"] = str(min(values))
            row[column + "_max"] = str(max(values))
        merged.append(row)
    return merged


def by_repetition(rows):
    """Splits the rows of a repeated experiment into one list per repetition."""
    groups = {}
    for row in rows:
        groups.setdefault(row["repetition"], []).append(row)
    return list(groups.values())


def range_text(values, digits=2):
    """Formats the smallest and largest of some values, e.g. '0.95 to 1.03'."""
    return f"{min(values):.{digits}f} to {max(values):.{digits}f}"


def write_table(path, header, rows):
    """Writes a Markdown table."""
    lines = ["| " + " | ".join(header) + " |", "|" + "|".join(["---"] * len(header)) + "|"]
    for row in rows:
        lines.append("| " + " | ".join(str(cell) for cell in row) + " |")
    Path(path).write_text("\n".join(lines) + "\n")


def fmt_ms(value):
    """Formats a time in milliseconds with a sensible number of digits."""
    value = float(value)
    if value >= 100:
        return f"{value:,.0f}"
    if value >= 1:
        return f"{value:.2f}"
    return f"{value:.4f}"


def fmt_int(value):
    return f"{int(value):,}"


def fit_exponent(xs, ys):
    """Slope of the best straight line through (log x, log y): time ~ pixels^slope."""
    slope, _ = np.polyfit(np.log(xs), np.log(ys), 1)
    return slope


# ----------------------------------------------------------------------------------------
# Plots from the CSV files
# ----------------------------------------------------------------------------------------
def growth_exponent(rows, algo, column="seam_ms_min"):
    """Exponent b in  value ~ pixels^b  for one algorithm, fitted over ALL tested sizes.

    The default column is the per-size MINIMUM over the repetitions. A run can only be
    slowed down by outside disturbances (other programs, page faults, a slow core), never
    sped up, so the fastest observation is the most reliable estimate of the real cost.
    """
    mine = [r for r in rows if r["algorithm"] == algo]
    pixels = [int(r["pixels"]) for r in mine]
    values = [float(r[column]) for r in mine]
    return fit_exponent(pixels, values)


def ns_per_pixel(row, column="seam_ms"):
    """Time per pixel in nanoseconds."""
    return float(row[column]) * 1e6 / int(row["pixels"])


def seam_time(rows, algo, cols):
    """Seam-search time (ms) of one algorithm at one image width."""
    return float(next(r for r in rows if r["algorithm"] == algo
                      and r["cols"] == str(cols))["seam_ms"])


def plot_runtime_vs_size(rows, repetitions, figures, generated, facts):
    fig, axis = plt.subplots(figsize=(6.4, 4.2))
    table = []
    for algo, style in STYLE.items():
        mine = [r for r in rows if r["algorithm"] == algo]
        pixels = [int(r["pixels"]) for r in mine]
        times = [float(r["seam_ms"]) for r in mine]
        axis.plot(pixels, times, color=style["color"], marker=style["marker"], linewidth=2,
                  markersize=7, label=style["label"])
        axis.fill_between(pixels, [float(r["seam_ms_min"]) for r in mine],
                          [float(r["seam_ms_max"]) for r in mine], color=style["color"],
                          alpha=0.2, linewidth=0)
        facts[f"exp_{algo}"] = f"{growth_exponent(rows, algo):.2f}"
        facts[f"exp_{algo}_median"] = f"{growth_exponent(rows, algo, 'seam_ms'):.2f}"
        for r in mine:
            table.append([style["label"], f"{r['cols']} x {r['rows']}", fmt_int(r["pixels"]),
                          fmt_ms(r["energy_ms"]), fmt_ms(r["seam_ms"]),
                          f"{fmt_ms(r['seam_ms_min'])} to {fmt_ms(r['seam_ms_max'])}",
                          # Per-pixel cost only makes sense for the methods that look at
                          # every pixel; greedy is Theta(m + n).
                          "-" if algo == "greedy" else f"{ns_per_pixel(r, 'seam_ms_min'):.3g}",
                          "-" if algo == "greedy" else f"{ns_per_pixel(r):.3g}",
                          fmt_int(float(r["augmentations"])) if algo == "graphcut" else "-"])
    axis.set_xscale("log")
    axis.set_yscale("log")
    axis.set_xlabel("Image size (pixels, log scale)")
    axis.set_ylabel("Time to find one seam (ms, log scale)")
    axis.set_title("Finding one seam: runtime vs image size", color=TEXT_COLOUR)
    # Legend in the empty area right of the graph-cut line and above the DP line.
    axis.legend(loc="upper right", bbox_to_anchor=(1.0, 0.86))
    fig.savefig(figures / "fig_runtime_vs_size.png")
    plt.close(fig)
    write_table(generated / "table_runtime_size.md",
                ["Algorithm", "Size (w x h)", "Pixels", "Energy map (ms)", "Seam search (ms)",
                 "Range over 3 runs (ms)", "ns per pixel (fastest run)",
                 "ns per pixel (median)", "Augmenting paths"],
                table)

    # Per-pixel cost of the DP: flat if the DP is linear.
    dp_rows = [r for r in rows if r["algorithm"] == "dp"]
    dp_ns = [ns_per_pixel(r) for r in dp_rows]
    dp_ns_best = [ns_per_pixel(r, "seam_ms_min") for r in dp_rows]
    facts["dp_ns_per_pixel_range"] = f"{min(dp_ns):.1f} to {max(dp_ns):.1f}"
    facts["dp_ns_per_pixel_best_range"] = f"{min(dp_ns_best):.1f} to {max(dp_ns_best):.1f}"
    largest = dp_rows[-1]
    facts["dp_largest_size"] = f"{largest['cols']} x {largest['rows']}"
    # Two tables (cost and parent) of 4-byte integers.
    facts["dp_largest_table_mb"] = f"{int(largest['pixels']) * 8 / 1e6:.0f}"
    facts["dp_largest_ms_range"] = (f"{fmt_ms(largest['seam_ms_min'])} to "
                                    f"{fmt_ms(largest['seam_ms_max'])}")
    facts["dp_largest_spread"] = (
        f"{float(largest['seam_ms_max']) / float(largest['seam_ms_min']):.2f}")

    # Graph cut: time per augmentation per pixel (time / augmenting paths / pixels).
    def bfs_ns_per_node(r):
        return float(r["seam_ms_min"]) * 1e6 / float(r["augmentations"]) / int(r["pixels"])

    gc_rows = [r for r in rows if r["algorithm"] == "graphcut"]
    facts["gc_bfs_ns_small"] = f"{bfs_ns_per_node(gc_rows[0]):.0f}"
    facts["gc_bfs_ns_large"] = f"{bfs_ns_per_node(gc_rows[-1]):.0f}"
    facts["gc_smallest_size"] = f"{gc_rows[0]['cols']} x {gc_rows[0]['rows']}"

    # Headline numbers quoted in the text.
    def find(algo, cols):
        return next(r for r in rows if r["algorithm"] == algo and r["cols"] == str(cols))

    for algo, cols in [("dp", 400), ("greedy", 400), ("dp", 3200), ("greedy", 3200),
                       ("graphcut", 100), ("graphcut", 250), ("graphcut", 500), ("dp", 100),
                       ("dp", 500), ("greedy", 500)]:
        row = find(algo, cols)
        facts[f"seam_ms_{algo}_{cols}"] = fmt_ms(row["seam_ms"])
        facts[f"energy_ms_{algo}_{cols}"] = fmt_ms(row["energy_ms"])
        facts[f"size_{algo}_{cols}"] = f"{row['cols']} x {row['rows']}"
    gc = find("graphcut", 250)
    facts["gc_250_seconds"] = f"{float(gc['seam_ms']) / 1000:.2f}"
    facts["gc_250_augmentations"] = fmt_int(float(gc["augmentations"]))
    facts["gc_250_pixels"] = fmt_int(gc["pixels"])
    gc500 = find("graphcut", 500)
    dp500 = find("dp", 500)
    facts["gc_500_seconds"] = f"{float(gc500['seam_ms']) / 1000:.0f}"
    facts["gc_500_minutes_150_seams"] = f"{150 * float(gc500['seam_ms']) / 60000:.0f}"
    facts["gc_vs_dp_500"] = fmt_int(round(float(gc500["seam_ms"]) / float(dp500["seam_ms"]), -3))
    facts["gc_500_augmentations"] = fmt_int(float(gc500["augmentations"]))
    ratios_500 = [seam_time(one, "graphcut", 500) / seam_time(one, "dp", 500)
                  for one in repetitions]
    facts["gc_vs_dp_500_range"] = (f"{fmt_int(round(min(ratios_500), -3))} to "
                                   f"{fmt_int(round(max(ratios_500), -3))}")
    # Growth of the NUMBER of augmenting paths (it does not depend on timing).
    facts["exp_augmentations"] = f"{growth_exponent(rows, 'graphcut', 'augmentations'):.2f}"
    facts["exp_graphcut_expected"] = (
        f"{1 + growth_exponent(rows, 'graphcut', 'augmentations'):.2f}")
    gc100 = find("graphcut", 100)
    facts["gc_100_seconds"] = f"{float(gc100['seam_ms']) / 1000:.2f}"
    dp100 = find("dp", 100)
    facts["gc_vs_dp_100"] = fmt_int(round(float(gc100["seam_ms"]) / float(dp100["seam_ms"]), -2))
    dp3200 = find("dp", 3200)
    gr3200 = find("greedy", 3200)
    facts["dp_vs_greedy_3200"] = fmt_int(round(float(dp3200["seam_ms"]) / float(gr3200["seam_ms"])))
    facts["greedy_total_share_3200"] = f"{100 * float(gr3200['energy_ms']) / (float(gr3200['energy_ms']) + float(gr3200['seam_ms'])):.1f}"
    facts["dp_total_3200"] = fmt_ms(float(dp3200["energy_ms"]) + float(dp3200["seam_ms"]))
    facts["greedy_total_3200"] = fmt_ms(float(gr3200["energy_ms"]) + float(gr3200["seam_ms"]))


def loop_speedup(rows):
    """How many times faster the complete greedy loop is than the DP loop (full image)."""
    def total(algo):
        return float([r for r in rows if r["algorithm"] == algo
                      and r["image"] == "tower"][-1]["cumulative_ms"])
    return total("dp") / total("greedy")


def plot_runtime_vs_seams(rows, repetitions, figures, generated, facts):
    fig, axes = plt.subplots(1, 2, figsize=(10.5, 4.0))
    panels = [("tower_small", "Small image (all three methods, log scale)", True),
              ("tower", "Full image (DP and greedy)", False)]
    table = []
    for axis, (image, title, log_scale) in zip(axes, panels):
        for algo, style in STYLE.items():
            mine = [r for r in rows if r["algorithm"] == algo and r["image"] == image]
            if not mine:
                continue
            seams = [int(r["seams_removed"]) for r in mine]
            times = [float(r["cumulative_ms"]) for r in mine]
            axis.plot(seams, times, color=style["color"], marker=style["marker"], linewidth=2,
                      markersize=6, label=style["label"])
            last = mine[-1]
            size = f"{last['cols']} x {last['rows']}"
            table.append([style["label"], size, last["seams_removed"], fmt_ms(last["cumulative_ms"]),
                          fmt_ms(float(last["cumulative_ms"]) / int(last["seams_removed"]))])
            facts[f"seams_total_ms_{algo}_{image}"] = fmt_ms(last["cumulative_ms"])
            facts[f"seams_count_{image}"] = last["seams_removed"]
            facts[f"seams_size_{image}"] = size
        if log_scale:
            axis.set_yscale("log")
        axis.set_xlabel("Seams removed")
        axis.set_ylabel("Cumulative time (ms)")
        axis.set_title(title, color=TEXT_COLOUR)
        axis.legend()
    fig.savefig(figures / "fig_runtime_vs_seams.png")
    plt.close(fig)
    write_table(generated / "table_runtime_seams.md",
                ["Algorithm", "Start size (w x h)", "Seams removed", "Total time (ms)",
                 "Average per seam (ms)"], table)
    facts["loop_speedup_greedy"] = f"{loop_speedup(rows):.1f}"
    facts["loop_speedup_greedy_range"] = range_text(
        [loop_speedup(one) for one in repetitions], digits=1)
    small_gc = float(next(r for r in reversed(rows) if r["algorithm"] == "graphcut")["cumulative_ms"])
    facts["seams_gc_seconds"] = f"{small_gc / 1000:.1f}"


def checkerboard_rows(pick, facts):
    """Table 3 rows for the synthetic checkerboard, plus a check against the hand-derived
    cheapest seam. Its energy is SOBEL_MAX on the two pixel lines either side of each square
    border and 0 elsewhere, so a seam that stays inside one row/column of squares costs
    2 * SOBEL_MAX per border it must cross, and no seam can cost less."""
    rows = []
    for direction in ["vertical", "horizontal"]:
        dp = pick(CHECKERBOARD, direction, "dp")
        greedy = pick(CHECKERBOARD, direction, "greedy")
        # A vertical seam crosses the borders between rows of squares, a horizontal one
        # the borders between columns of squares.
        length = int(dp["rows"]) if direction == "vertical" else int(dp["cols"])
        borders = (length - 1) // CHECKERBOARD_SQUARE
        cheapest = borders * 2 * SOBEL_MAX
        if int(dp["first_seam_cost"]) != cheapest:
            raise SystemExit(f"DP's first checkerboard seam ({direction}) costs "
                             f"{dp['first_seam_cost']}, expected {cheapest}")
        ratio = int(greedy["total_cost"]) / int(dp["total_cost"])
        facts[f"checker_borders_{direction}"] = str(borders)
        facts[f"checker_cheapest_{direction}"] = fmt_int(cheapest)
        facts[f"checker_dp_at_bound_{direction}"] = \
            "every" if int(dp["total_cost"]) == cheapest * int(dp["seams"]) else "not every"
        facts[f"cost_ratio_checkerboard_{direction}"] = f"{ratio:.2f}"
        rows.append([CHECKERBOARD, f"{dp['cols']} x {dp['rows']}", direction, dp["seams"],
                     fmt_int(dp["total_cost"]), fmt_int(greedy["total_cost"]), f"{ratio:.2f}x",
                     f"{greedy['optimal_steps']} / {greedy['seams']}",
                     fmt_ms(dp["time_ms"]), fmt_ms(greedy["time_ms"])])
    return rows


def cost_tables(rows, figures, generated, facts):
    def pick(image, direction, algo, energy="gradient"):
        return next(r for r in rows if r["image"] == image and r["direction"] == direction
                    and r["algorithm"] == algo and r["energy"] == energy)

    # Full-size images: DP vs greedy.
    full = []
    ratios = []
    for image in IMAGES:
        for direction in ["vertical", "horizontal"]:
            dp = pick(image, direction, "dp")
            greedy = pick(image, direction, "greedy")
            ratio = int(greedy["total_cost"]) / int(dp["total_cost"])
            ratios.append(ratio)
            facts[f"cost_ratio_{image}_{direction}"] = f"{ratio:.2f}"
            full.append([image, f"{dp['cols']} x {dp['rows']}", direction, dp["seams"],
                         fmt_int(dp["total_cost"]), fmt_int(greedy["total_cost"]), f"{ratio:.2f}x",
                         f"{greedy['optimal_steps']} / {greedy['seams']}",
                         fmt_ms(dp["time_ms"]), fmt_ms(greedy["time_ms"])])
    full.extend(checkerboard_rows(pick, facts))
    write_table(generated / "table_cost_full.md",
                ["Image", "Size (w x h)", "Seams", "Count", "DP total cost", "Greedy total cost",
                 "Greedy / DP", "Greedy steps that were optimal", "DP time (ms)",
                 "Greedy time (ms)"], full)
    facts["cost_ratio_min"] = f"{min(ratios):.2f}"
    facts["cost_ratio_max"] = f"{max(ratios):.2f}"

    # Small copies: DP vs greedy vs graph cut.
    small = []
    gc_optimal = 0
    gc_steps = 0
    worst_gap = 0.0
    for image in IMAGES:
        for direction in ["vertical", "horizontal"]:
            name = image + "_small"
            dp = pick(name, direction, "dp")
            greedy = pick(name, direction, "greedy")
            gc = pick(name, direction, "graphcut")
            gc_optimal += int(gc["optimal_steps"])
            gc_steps += int(gc["seams"])
            gap = abs(int(gc["total_cost"]) - int(dp["total_cost"])) / int(dp["total_cost"])
            worst_gap = max(worst_gap, gap)
            small.append([image, f"{dp['cols']} x {dp['rows']}", direction, dp["seams"],
                          fmt_int(dp["first_seam_cost"]), fmt_int(gc["first_seam_cost"]),
                          fmt_int(greedy["first_seam_cost"]),
                          fmt_int(dp["total_cost"]), fmt_int(gc["total_cost"]),
                          fmt_int(greedy["total_cost"]),
                          f"{gc['optimal_steps']} / {gc['seams']}",
                          fmt_ms(dp["time_ms"]), fmt_ms(gc["time_ms"])])
            if int(dp["first_seam_cost"]) != int(gc["first_seam_cost"]):
                raise SystemExit(f"first seam cost differs between DP and graph cut on {name}")
    write_table(generated / "table_cost_small.md",
                ["Image", "Size", "Seams", "Count", "1st seam DP", "1st seam GC", "1st seam greedy",
                 "Total DP", "Total GC", "Total greedy", "GC steps optimal", "DP ms", "GC ms"],
                small)
    facts["gc_optimal_steps"] = str(gc_optimal)
    facts["gc_total_steps"] = str(gc_steps)
    facts["gc_total_gap_percent"] = f"{100 * worst_gap:.2f}"
    tower = pick("tower_small", "vertical", "dp")
    tower_gc = pick("tower_small", "vertical", "graphcut")
    facts["tower_small_dp_total"] = fmt_int(tower["total_cost"])
    facts["tower_small_gc_total"] = fmt_int(tower_gc["total_cost"])
    facts["tower_small_dp_ms"] = fmt_ms(tower["time_ms"])
    facts["tower_small_gc_s"] = f"{float(tower_gc['time_ms']) / 1000:.1f}"
    facts["tower_small_seams"] = tower["seams"]

    # Backward vs forward energy: how much edge energy does the result contain?
    forward = []
    for image in IMAGES:
        back = pick(image, "vertical", "dp")
        fwd = pick(image, "vertical", "dp", "forward")
        greedy = pick(image, "vertical", "greedy")
        pixels_before = int(back["rows"]) * int(back["cols"])
        pixels_after = int(back["rows"]) * (int(back["cols"]) - int(back["seams"]))
        row = [image, f"{int(back['image_energy_before']) / pixels_before:.1f}"]
        for run in (back, fwd, greedy):
            row.append(f"{int(run['image_energy_after']) / pixels_after:.1f}")
        forward.append(row)
        facts[f"mean_energy_before_{image}"] = row[1]
        facts[f"mean_energy_dp_{image}"] = row[2]
        facts[f"mean_energy_forward_{image}"] = row[3]
        facts[f"mean_energy_greedy_{image}"] = row[4]
    write_table(generated / "table_forward.md",
                ["Image", "Original", "After DP (gradient energy)", "After DP (forward energy)",
                 "After greedy"], forward)

    # Bar chart: total cost, DP vs greedy, vertical seams on the full images.
    fig, axis = plt.subplots(figsize=(6.4, 3.8))
    width = 0.36
    for offset, algo in [(-width / 2, "dp"), (width / 2, "greedy")]:
        values = [int(pick(image, "vertical", algo)["total_cost"]) / 1e6 for image in IMAGES]
        bars = axis.bar([i + offset for i in range(len(IMAGES))], values, width - 0.03,
                        color=STYLE[algo]["color"], label=STYLE[algo]["label"])
        for bar, value in zip(bars, values):
            axis.text(bar.get_x() + bar.get_width() / 2, value, f"{value:.2f}", ha="center",
                      va="bottom", fontsize=9, color=TEXT_COLOUR)
    axis.set_xticks(range(len(IMAGES)))
    axis.set_xticklabels(IMAGES)
    axis.set_ylabel("Total energy removed (millions)")
    axis.set_title("Total seam cost for removing 30 % of the width", color=TEXT_COLOUR)
    axis.grid(axis="x", visible=False)
    axis.legend()
    fig.savefig(figures / "fig_seam_cost.png")
    plt.close(fig)


def improvement_tables(results, figures, generated, facts):
    # Memory.
    rows = median_over_repetitions(read_csv(results / "csv" / "improve_memory.csv"), ["seam_ms"])
    full, rolling = rows[0], rows[1]
    table = []
    for row, label in [(full, "Full tables (cost + parent, 4-byte ints)"),
                       (rolling, "Two rolling rows + 1-byte steps")]:
        table.append([label, f"{row['cols']} x {row['rows']}",
                      f"{int(row['table_bytes']) / 1e6:.1f}", fmt_ms(row["seam_ms"])])
    write_table(generated / "table_memory.md",
                ["DP variant", "Image (w x h)", "Table memory (MB)", "Seam search (ms)"], table)
    facts["mem_full_mb"] = table[0][2]
    facts["mem_rolling_mb"] = table[1][2]
    facts["mem_ratio"] = f"{int(full['table_bytes']) / int(rolling['table_bytes']):.1f}"
    facts["mem_full_ms"] = fmt_ms(full["seam_ms"])
    facts["mem_rolling_ms"] = fmt_ms(rolling["seam_ms"])
    facts["mem_size"] = f"{full['cols']} x {full['rows']}"

    # Threads.
    all_rows = read_csv(results / "csv" / "improve_threads.csv")
    rows = median_over_repetitions(all_rows, ["time_ms"])
    repetitions = by_repetition(all_rows)

    def speedups_of(some_rows, stage, cols):
        """Speed-up over the serial loop for each thread count: {threads: speed-up}."""
        mine = [r for r in some_rows if r["stage"] == stage and int(r["cols"]) == cols]
        serial = float(next(r for r in mine if r["threads"] == "0")["time_ms"])
        return {int(r["threads"]): serial / float(r["time_ms"]) for r in mine
                if r["threads"] != "0"}

    sizes = sorted({(int(r["cols"]), int(r["rows"])) for r in rows})
    fig, axes = plt.subplots(1, len(sizes), figsize=(10.5, 3.9), sharey=True)
    stage_style = {"energy": ("#2a78d6", "o", "Energy map"),
                   "dp_table": ("#eb6834", "s", "DP table (parallel inside each row)")}
    table = []
    for axis, (cols, rows_count) in zip(axes, sizes):
        for stage, (colour, marker, label) in stage_style.items():
            mine = [r for r in rows if r["stage"] == stage and int(r["cols"]) == cols]
            serial = float(next(r for r in mine if r["threads"] == "0")["time_ms"])
            threaded = [r for r in mine if r["threads"] != "0"]
            threads = [int(r["threads"]) for r in threaded]
            speedups = [serial / float(r["time_ms"]) for r in threaded]
            axis.plot(threads, speedups, color=colour, marker=marker, linewidth=2, markersize=6,
                      label=label)
            # Shaded band: smallest and largest speed-up over the repetitions.
            per_run = [speedups_of(one, stage, cols) for one in repetitions]
            axis.fill_between(threads, [min(run[t] for run in per_run) for t in threads],
                              [max(run[t] for run in per_run) for t in threads], color=colour,
                              alpha=0.2, linewidth=0)
            table.append([label, f"{cols} x {rows_count}", fmt_ms(serial)] +
                         [f"{fmt_ms(r['time_ms'])} ({s:.2f}x)" for r, s in zip(threaded, speedups)])
            size_key = "small" if cols == sizes[0][0] else "large"
            facts[f"threads_{stage}_{size_key}_serial_ms"] = fmt_ms(serial)
            facts[f"threads_{stage}_{size_key}_best_range"] = range_text(
                [max(run.values()) for run in per_run], digits=1)
            facts[f"threads_{stage}_{size_key}_4"] = f"{speedups[2]:.2f}"
            facts[f"threads_{stage}_{size_key}_8"] = f"{speedups[3]:.2f}"
            facts[f"threads_{stage}_{size_key}_best"] = f"{max(speedups):.1f}"
            facts[f"threads_{stage}_{size_key}_best_n"] = str(threads[speedups.index(max(speedups))])
            facts[f"threads_{stage}_{size_key}_16"] = f"{speedups[-1]:.2f}"
            facts[f"threads_{stage}_{size_key}_2"] = f"{speedups[1]:.2f}"
        opencv_ms = float(next(r for r in rows if r["stage"] == "energy_opencv"
                               and int(r["cols"]) == cols)["time_ms"])
        table.append(["Energy map, OpenCV Sobel (for reference)", f"{cols} x {rows_count}",
                      fmt_ms(opencv_ms), "-", "-", "-", "-", "-"])
        facts[f"energy_opencv_{'small' if cols == sizes[0][0] else 'large'}_ms"] = fmt_ms(opencv_ms)
        axis.axhline(1.0, color="#888888", linewidth=1, linestyle="--")
        axis.set_xscale("log", base=2)
        axis.set_xticks([1, 2, 4, 8, 16])
        axis.set_xticklabels(["1", "2", "4", "8", "16"])
        axis.set_xlabel("Threads")
        axis.set_title(f"{cols} x {rows_count} image", color=TEXT_COLOUR)
    axes[0].set_ylabel("Speed-up over our serial loop")
    axes[0].legend()
    fig.savefig(figures / "fig_threads.png")
    plt.close(fig)
    write_table(generated / "table_threads.md",
                ["Stage", "Image (w x h)", "Serial (ms)", "1 thread", "2 threads", "4 threads",
                 "8 threads", "16 threads"], table)
    facts["threads_small_size"] = f"{sizes[0][0]} x {sizes[0][1]}"
    facts["threads_large_size"] = f"{sizes[1][0]} x {sizes[1][1]}"

    # Incremental energy.
    rows = median_over_repetitions(read_csv(results / "csv" / "improve_incremental.csv"),
                                   ["energy_ms", "total_ms"])
    full, incremental = rows[0], rows[1]
    table = []
    for row, label in [(full, "Recompute the whole energy map"),
                       (incremental, "Update only the band next to the seam")]:
        table.append([label, f"{row['cols']} x {row['rows']}", row["seams"],
                      fmt_ms(row["energy_ms"]), fmt_ms(row["total_ms"])])
    write_table(generated / "table_incremental.md",
                ["Energy strategy", "Start size (w x h)", "Seams", "Energy time (ms)",
                 "Whole loop (ms)"], table)
    facts["inc_energy_speedup"] = f"{float(full['energy_ms']) / float(incremental['energy_ms']):.1f}"
    facts["inc_total_speedup"] = f"{float(full['total_ms']) / float(incremental['total_ms']):.1f}"
    facts["inc_seams"] = full["seams"]

    # Memory per pixel: graph-cut network against the DP tables.
    for row in read_csv(results / "csv" / "memory_per_pixel.csv"):
        facts[f"bytes_per_pixel_{row['structure']}"] = f"{float(row['bytes_per_pixel']):.0f}"
    graph = read_csv(results / "csv" / "memory_per_pixel.csv")[0]
    facts["graph_memory_size"] = f"{graph['cols']} x {graph['rows']}"
    facts["graph_memory_nodes"] = fmt_int(graph["nodes"])
    facts["graph_memory_edges"] = fmt_int(graph["edges"])
    facts["inc_size"] = f"{full['cols']} x {full['rows']}"


# ----------------------------------------------------------------------------------------
# Picture panels
# ----------------------------------------------------------------------------------------
def picture_panel(paths_and_titles, output, columns, width_per_picture=3.3, suptitle=None):
    """Lays out pictures in a grid, drawn at a common scale so widths can be compared."""
    count = len(paths_and_titles)
    rows = math.ceil(count / columns)
    pictures = [mpimg.imread(path) for path, _ in paths_and_titles]
    widest = max(p.shape[1] for p in pictures)
    tallest = max(p.shape[0] for p in pictures)
    fig, axes = plt.subplots(rows, columns,
                             figsize=(width_per_picture * columns,
                                      width_per_picture * tallest / widest * rows + 0.45 * rows))
    axes = np.atleast_1d(axes).ravel()
    for axis in axes:
        axis.axis("off")
    for axis, picture, (_, title) in zip(axes, pictures, paths_and_titles):
        axis.imshow(picture, interpolation="nearest")
        # The same axis limits for every picture: a narrower picture LOOKS narrower.
        axis.set_xlim(0, widest)
        axis.set_ylim(tallest, 0)
        axis.set_title(title, fontsize=9, color=TEXT_COLOUR)
    if suptitle:
        fig.suptitle(suptitle, fontsize=11, color=TEXT_COLOUR)
    fig.tight_layout()
    fig.savefig(output)
    plt.close(fig)


def picture_figures(results, figures):
    for image in IMAGES:
        folder = results / image
        picture_panel([
            (folder / "original.png", "Original"),
            (folder / "scaled.png", "Plain scaling to 70 % width"),
            (folder / "cropped.png", "Centre crop to 70 % width"),
            (folder / "dp.png", "Seam carving, DP"),
            (folder / "greedy.png", "Seam carving, greedy"),
            (folder / "dp_forward.png", "Seam carving, DP + forward energy"),
        ], figures / f"fig_compare_{image}.png", columns=3)
        picture_panel([
            (folder / "energy.png", "Energy map (Sobel)"),
            (folder / "dp_seams.png", "DP: all removed seams"),
            (folder / "greedy_seams.png", "Greedy: all removed seams"),
        ], figures / f"fig_seams_{image}.png", columns=3)
        picture_panel([
            (folder / "small_dp_seams.png", "DP seams"),
            (folder / "small_graphcut_seams.png", "Graph-cut seams"),
            (folder / "small_greedy_seams.png", "Greedy seams"),
            (folder / "small_dp.png", "DP result"),
            (folder / "small_graphcut.png", "Graph-cut result"),
            (folder / "small_greedy.png", "Greedy result"),
        ], figures / f"fig_small_{image}.png", columns=3, width_per_picture=2.6)
    tower = results / "tower"
    picture_panel([(tower / f"dp_step_{p}.png", f"{p} % of the seams removed")
                   for p in (0, 25, 50, 75, 100)],
                  figures / "fig_steps.png", columns=5, width_per_picture=2.3)
    picture_panel([
        (tower / "dp_both_seams.png", "Vertical (red) and horizontal (green) seams"),
        (tower / "dp_horizontal.png", "Height reduced to 70 % (horizontal seams)"),
        (tower / "dp_enlarged.png", "Width enlarged to 120 % (seam insertion)"),
    ], figures / "fig_extras.png", columns=3)
    picture_panel([
        (tower / "dp_enlarged_seams.png", "Pixels duplicated by vertical seam insertion (red)"),
        (tower / "dp_mixed_seams.png", "Vertical seams removed (red), horizontal inserted (green)"),
        (tower / "dp_mixed.png", "Result: 80 % width, 115 % height"),
    ], figures / "fig_insertion.png", columns=3)


# ----------------------------------------------------------------------------------------
# Diagrams (drawn, not measured)
# ----------------------------------------------------------------------------------------
def diagram_graph_construction(figures):
    """The flow network of spec item 4 for a 3 x 4 image."""
    rows, cols = 3, 4
    fig, axis = plt.subplots(figsize=(8.6, 4.4))
    axis.axis("off")
    axis.set_xlim(-2.0, cols + 1.6)
    axis.set_ylim(rows - 0.3, -1.0)
    finite, infinite = "#2a78d6", "#999999"

    def arrow(start, end, colour, bend=0.0, width=1.6, style="-"):
        axis.annotate("", xy=end, xytext=start,
                      arrowprops=dict(arrowstyle="-|>", color=colour, linewidth=width,
                                      linestyle=style, shrinkA=13, shrinkB=13,
                                      connectionstyle=f"arc3,rad={bend}"))

    source, sink = (-1.5, 1), (cols + 0.9, 1)
    for row in range(rows):
        arrow(source, (0, row), infinite)
        arrow((cols - 1, row), sink, finite, width=2.2)
        for col in range(cols):
            if col + 1 < cols:
                arrow((col, row), (col + 1, row), finite, bend=-0.28, width=2.2)   # forward
                arrow((col + 1, row), (col, row), infinite, bend=-0.28)            # backward
                # The label sits just above the middle of its forward edge, on a white
                # patch so the diagonal edges crossing behind it do not hide it.
                axis.text(col + 0.5, row - 0.2, f"e[{row},{col}]", ha="center", va="center",
                          fontsize=7, color=finite, zorder=5,
                          bbox=dict(facecolor="white", edgecolor="none", pad=0.4))
            if col >= 1:
                if row >= 1:
                    arrow((col, row), (col - 1, row - 1), infinite, style=":")
                if row + 1 < rows:
                    arrow((col, row), (col - 1, row + 1), infinite, style=":")
        axis.text(cols - 0.42, row - 0.2 + (row - 1) * 0.22, f"e[{row},{cols - 1}]", fontsize=7.5,
                  color=finite)
    for row in range(rows):
        for col in range(cols):
            axis.add_patch(plt.Circle((col, row), 0.17, facecolor="white", edgecolor="#333333",
                                      zorder=3))
            axis.text(col, row, f"{row},{col}", ha="center", va="center", fontsize=7, zorder=4)
    for (x, y), name in [(source, "s"), (sink, "t")]:
        axis.add_patch(plt.Circle((x, y), 0.22, facecolor="#333333", zorder=3))
        axis.text(x, y, name, color="white", ha="center", va="center", fontsize=11, zorder=4)
    axis.plot([], [], color=finite, linewidth=2.2, label="finite: forward edge, capacity e[i,j]")
    axis.plot([], [], color=infinite, linewidth=1.6, label="infinite: source and backward edges")
    axis.plot([], [], color=infinite, linewidth=1.6, linestyle=":",
              label="infinite: diagonal edges (i,j) to (i-1,j-1), (i+1,j-1)")
    axis.legend(loc="upper center", bbox_to_anchor=(0.5, 0.02), ncol=1, fontsize=8.5)
    axis.set_title("Flow network for a vertical seam on a 3 x 4 image", color=TEXT_COLOUR)
    fig.savefig(figures / "fig_graph_construction.png")
    plt.close(fig)


def diagram_dp_table(figures):
    """The hand-worked 4 x 4 example that tests/test_dp.cpp also checks."""
    energy = [[1, 4, 3, 5], [3, 2, 5, 2], [5, 2, 4, 2], [1, 6, 1, 3]]
    rows, cols = 4, 4
    cumulative = [energy[0][:]]
    parent = [[-1] * cols]
    for i in range(1, rows):
        cumulative.append([0] * cols)
        parent.append([0] * cols)
        for j in range(cols):
            best = j  # same tie order as the C++ code: up, up-left, up-right
            if j - 1 >= 0 and cumulative[i - 1][j - 1] < cumulative[i - 1][best]:
                best = j - 1
            if j + 1 < cols and cumulative[i - 1][j + 1] < cumulative[i - 1][best]:
                best = j + 1
            cumulative[i][j] = energy[i][j] + cumulative[i - 1][best]
            parent[i][j] = best
    seam = [0] * rows
    col = min(range(cols), key=lambda j: cumulative[rows - 1][j])
    for i in range(rows - 1, -1, -1):
        seam[i] = col
        col = parent[i][col]

    fig, axes = plt.subplots(1, 2, figsize=(7.6, 3.6))
    for axis, values, title in [(axes[0], energy, "Energy e[i][j]"),
                                (axes[1], cumulative, "Cumulative M[i][j] with parents")]:
        axis.set_xlim(0, cols)
        axis.set_ylim(rows, 0)
        axis.set_aspect("equal")
        axis.axis("off")
        axis.set_title(title, color=TEXT_COLOUR, fontsize=10)
        for i in range(rows):
            for j in range(cols):
                on_seam = seam[i] == j
                axis.add_patch(plt.Rectangle((j, i), 1, 1, edgecolor="#888888",
                                             facecolor="#cfe1f7" if on_seam else "white"))
                axis.text(j + 0.5, i + 0.58, str(values[i][j]), ha="center", va="center",
                          fontsize=13, fontweight="bold" if on_seam else "normal")
    for i in range(1, rows):
        for j in range(cols):
            axes[1].annotate("", xy=(parent[i][j] + 0.5, i - 0.22), xytext=(j + 0.5, i + 0.3),
                             arrowprops=dict(arrowstyle="-|>", color="#eb6834", linewidth=1.1))
    fig.savefig(figures / "fig_dp_table.png")
    plt.close(fig)


def machine_and_test_facts(results, facts):
    """Adds the machine description and the test-suite totals written by run_all.sh."""
    for line in (results / "machine.txt").read_text().splitlines():
        key, _, value = line.partition("=")
        facts[key] = value.strip()
    summary = (results / "test_summary.txt").read_text()
    match = re.search(r"All tests passed \((\d+) assertions in (\d+) test cases\)", summary)
    if not match:
        raise SystemExit("the test suite did not pass: see results/test_summary.txt")
    facts["test_assertions"] = fmt_int(match.group(1))
    facts["test_cases"] = match.group(2)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    results = Path(sys.argv[1])
    report = Path(sys.argv[2])
    figures = report / "figures"
    generated = report / "generated"
    figures.mkdir(parents=True, exist_ok=True)
    generated.mkdir(parents=True, exist_ok=True)

    facts = {}
    size_rows = read_csv(results / "csv" / "runtime_vs_size.csv")
    plot_runtime_vs_size(median_over_repetitions(size_rows, ["energy_ms", "seam_ms",
                                                             "augmentations"]),
                         by_repetition(size_rows), figures, generated, facts)
    seam_rows = read_csv(results / "csv" / "runtime_vs_seams.csv")
    plot_runtime_vs_seams(median_over_repetitions(seam_rows, ["cumulative_ms"]),
                          by_repetition(seam_rows), figures, generated, facts)
    facts["repetitions"] = str(len(by_repetition(size_rows)))
    cost_tables(read_csv(results / "csv" / "seam_cost.csv"), figures, generated, facts)
    improvement_tables(results, figures, generated, facts)
    picture_figures(results, figures)
    diagram_graph_construction(figures)
    diagram_dp_table(figures)

    machine_and_test_facts(results, facts)
    lines = [f"{key}={value}" for key, value in sorted(facts.items())]
    (generated / "facts.txt").write_text("\n".join(lines) + "\n")
    print(f"wrote {len(facts)} facts, figures in {figures}, tables in {generated}")


if __name__ == "__main__":
    main()
