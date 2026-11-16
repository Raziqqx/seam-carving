#!/usr/bin/env python3
"""Builds the presentation deck (PowerPoint) and the speaker-notes hand-out.

Usage:  python slides/build_slides.py <repo dir>

The deck has one section per team member (2 minutes each, 12 minutes in total). Figures
come from report/figures and numbers from report/generated/facts.txt, so the slides
always agree with the report. Speaker notes are stored in each slide's notes pane and
also written to slides/speaker_notes.md.
"""
import sys
from pathlib import Path

from PIL import Image
from pptx import Presentation
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
from pptx.util import Inches, Pt

from notes_text import speaker_notes

SLIDE_W, SLIDE_H = 13.333, 7.5  # inches, 16:9
INK = RGBColor(0x1B, 0x1B, 0x1B)
MUTED = RGBColor(0x55, 0x55, 0x55)
ACCENT = RGBColor(0x2A, 0x78, 0xD6)
PALE = RGBColor(0xEE, 0xF3, 0xFA)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)
FONT = "Calibri"
MONO = "Consolas"


class Deck:
    """Thin wrapper around python-pptx with the few layouts this deck needs."""

    def __init__(self, figures_dir):
        self.presentation = Presentation()
        self.presentation.slide_width = Inches(SLIDE_W)
        self.presentation.slide_height = Inches(SLIDE_H)
        self.figures_dir = figures_dir
        self.notes = []  # (slide number, title, speaker, notes text)

    # -- low-level helpers --------------------------------------------------------------
    def _text(self, slide, left, top, width, height, lines, size=18, colour=INK, bold=False,
              font=FONT, bullet=False, spacing=6):
        """Adds a text box. `lines` is a string or a list of strings (one paragraph each)."""
        box = slide.shapes.add_textbox(Inches(left), Inches(top), Inches(width), Inches(height))
        frame = box.text_frame
        frame.word_wrap = True
        if isinstance(lines, str):
            lines = [lines]
        for index, line in enumerate(lines):
            paragraph = frame.paragraphs[0] if index == 0 else frame.add_paragraph()
            run = paragraph.add_run()
            run.text = ("• " + line) if bullet else line
            run.font.size = Pt(size)
            run.font.bold = bold
            run.font.name = font
            run.font.color.rgb = colour
            paragraph.space_after = Pt(spacing)
        return box

    def _picture(self, slide, name, left, top, max_width, max_height):
        """Adds a figure scaled to fit inside the given box, centred, keeping its shape."""
        path = self.figures_dir / name
        with Image.open(path) as image:
            ratio = image.width / image.height
        width = max_width
        height = width / ratio
        if height > max_height:
            height = max_height
            width = height * ratio
        slide.shapes.add_picture(str(path), Inches(left + (max_width - width) / 2),
                                 Inches(top + (max_height - height) / 2), Inches(width),
                                 Inches(height))

    def _new_slide(self, title, speaker, section, notes):
        slide = self.presentation.slides.add_slide(self.presentation.slide_layouts[6])  # blank
        number = len(self.presentation.slides)
        # Accent bar and title.
        bar = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(0.22), Inches(SLIDE_H))
        bar.fill.solid()
        bar.fill.fore_color.rgb = ACCENT
        bar.line.fill.background()
        self._text(slide, 0.6, 0.32, 12.2, 0.9, title, size=30, bold=True)
        # Footer: section, speaker and slide number.
        self._text(slide, 0.6, 6.98, 9.5, 0.4, f"{section}  |  {speaker}", size=12, colour=MUTED)
        self._text(slide, 11.9, 6.98, 1.0, 0.4, str(number), size=12, colour=MUTED)
        slide.notes_slide.notes_text_frame.text = notes
        self.notes.append((number, title, speaker, notes))
        return slide

    # -- layouts ------------------------------------------------------------------------
    def title_slide(self, title, subtitle, team_lines, speaker, notes):
        slide = self.presentation.slides.add_slide(self.presentation.slide_layouts[6])
        band = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(SLIDE_W), Inches(3.3))
        band.fill.solid()
        band.fill.fore_color.rgb = ACCENT
        band.line.fill.background()
        self._text(slide, 0.8, 0.9, 11.7, 1.2, title, size=44, bold=True, colour=WHITE)
        self._text(slide, 0.8, 2.1, 11.7, 0.8, subtitle, size=22, colour=WHITE)
        self._text(slide, 0.8, 3.6, 5.6, 3.4, team_lines, size=16, spacing=3)
        self._picture(slide, "fig_seams_tower.png", 6.3, 3.7, 6.7, 3.2)
        slide.notes_slide.notes_text_frame.text = notes
        self.notes.append((1, title, speaker, notes))

    def bullets_and_picture(self, title, speaker, section, bullets, picture, notes,
                            text_width=5.6, bullet_size=19):
        slide = self._new_slide(title, speaker, section, notes)
        self._text(slide, 0.6, 1.45, text_width, 5.3, bullets, size=bullet_size, bullet=True,
                   spacing=10)
        left = 0.6 + text_width + 0.3
        self._picture(slide, picture, left, 1.4, SLIDE_W - left - 0.4, 5.4)
        return slide

    def bullets_above_picture(self, title, speaker, section, bullets, picture, notes,
                              bullet_size=18, text_height=2.5):
        """For wide figures: bullets across the top, the picture across the bottom."""
        slide = self._new_slide(title, speaker, section, notes)
        self._text(slide, 0.6, 1.35, 12.2, text_height, bullets, size=bullet_size, bullet=True,
                   spacing=5)
        top = 1.35 + text_height + 0.1
        self._picture(slide, picture, 0.6, top, 12.2, 6.85 - top)
        return slide

    def picture_slide(self, title, speaker, section, picture, caption, notes):
        slide = self._new_slide(title, speaker, section, notes)
        self._picture(slide, picture, 0.6, 1.35, 12.2, 4.75)
        self._text(slide, 0.6, 6.2, 12.2, 0.7, caption, size=17, colour=INK)
        return slide

    def bullets_and_code(self, title, speaker, section, bullets, code, notes, text_width=6.0,
                         code_size=14):
        slide = self._new_slide(title, speaker, section, notes)
        self._text(slide, 0.6, 1.45, text_width, 5.3, bullets, size=19, bullet=True, spacing=10)
        left = 0.6 + text_width + 0.3
        width = SLIDE_W - left - 0.5
        line_count = code.count("\n") + 1
        panel_height = min(5.1, line_count * code_size * 1.32 / 72 + 0.45)
        panel = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(left), Inches(1.5),
                                       Inches(width), Inches(panel_height))
        panel.fill.solid()
        panel.fill.fore_color.rgb = PALE
        panel.line.fill.background()
        self._text(slide, left + 0.2, 1.65, width - 0.4, 4.8, code.split("\n"), size=code_size,
                   font=MONO, spacing=1)
        return slide

    def table_slide(self, title, speaker, section, header, rows, column_widths, notes,
                    footer_lines=None, font_size=15):
        slide = self._new_slide(title, speaker, section, notes)
        row_height = 0.52
        shape = slide.shapes.add_table(len(rows) + 1, len(header), Inches(0.6), Inches(1.45),
                                       Inches(sum(column_widths)),
                                       Inches(row_height * (len(rows) + 1)))
        table = shape.table
        for index, width in enumerate(column_widths):
            table.columns[index].width = Inches(width)
        for row_index, row in enumerate([header] + rows):
            for column_index, value in enumerate(row):
                cell = table.cell(row_index, column_index)
                cell.text = value
                paragraph = cell.text_frame.paragraphs[0]
                paragraph.runs[0].font.size = Pt(font_size)
                paragraph.runs[0].font.name = FONT
                paragraph.runs[0].font.bold = (row_index == 0)
                paragraph.runs[0].font.color.rgb = WHITE if row_index == 0 else INK
                cell.fill.solid()
                cell.fill.fore_color.rgb = ACCENT if row_index == 0 else (
                    PALE if row_index % 2 == 0 else WHITE)
        if footer_lines:
            top = 1.45 + row_height * (len(rows) + 1) + 0.3
            self._text(slide, 0.6, top, 12.2, SLIDE_H - top - 0.7, footer_lines, size=18,
                       bullet=True, spacing=8)
        return slide

    def save(self, path):
        self.presentation.save(str(path))


def read_facts(path):
    facts = {}
    for line in path.read_text().splitlines():
        key, _, value = line.partition("=")
        facts[key] = value
    return facts


def build(repo_dir):
    figures = repo_dir / "report" / "figures"
    f = read_facts(repo_dir / "report" / "generated" / "facts.txt")
    deck = Deck(figures)
    notes = speaker_notes(f)

    # ------------------------------------------------------------------ 1. Raziq
    who, section = "Muhammad Raziq", "1. Problem, energy and dynamic programming"
    deck.title_slide(
        "Image Compression by Seam Carving",
        "Dynamic programming, greedy and graph cut  |  Algorithms Assignment 2",
        ["Team 5",
         "Muhammad Raziq Bin Mohd Rafig (2403133)",
         "Wong Shi Yu Regine (2401957)",
         "Tan Ying Clarissa (2401887)",
         "Isaiah Low Jung (2400844)",
         "Yasmin Tazi (2402451)",
         "Kimberly Charlotte Madlangbayan Ambrosio (2401836)"],
        who,
        notes[1])
    deck.bullets_above_picture(
        "The pipeline: energy, find a seam, remove, repeat", who, section,
        ["Energy = Sobel gradient |Gx| + |Gy| on the grayscale image: high on edges, about 0 "
         "in flat areas",
         "Borders: replicate the edge pixel, so the frame does not look like an edge",
         "Seam = one pixel per row, neighbours at most 1 column apart",
         "After every removal the energy map is recomputed (item 1c)",
         "Horizontal seams: transpose, carve vertical seams, transpose back, so there is one "
         "implementation to test"],
        "fig_seams_tower.png",
        notes[2], text_height=2.75)
    deck.bullets_and_picture(
        "Dynamic programming finds the optimal seam in O(mn)", who, section,
        ["M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])",
         "Base case M[0][j] = e[0][j]; a parent outside the image is skipped",
         "Why it works: the part of a best seam above (i, j) must itself be a best seam "
         "(optimal substructure), and cells are reused (overlapping subproblems)",
         "Answer = smallest value in the bottom row; follow parent pointers upwards",
         "Checked against brute force over all seams on 540 random grids"],
        "fig_dp_table.png",
        notes[3], text_width=6.3,
        bullet_size=18)

    # ------------------------------------------------------------------ 2. Regine
    who, section = "Wong Shi Yu Regine", "2. Greedy"
    deck.bullets_and_code(
        "Greedy: pick the locally cheapest pixel, row by row", who, section,
        ["Start at the minimum-energy pixel of the top row",
         "Each next row: move to the cheapest of the (up to) 3 pixels below",
         "No table: O(n + m) time, O(m) space",
         "Always a valid seam, but not optimal: a cheap start can lead into an expensive "
         "region",
         "Our 3 x 3 counter-example: greedy pays 19, the optimum is 4"],
        "energy        greedy = 19     DP = 4\n"
        "1  2  9       [1] 2  9        1 [2] 9\n"
        "9  9  1       [9] 9  1        9  9 [1]\n"
        "9  9  1       [9] 9  1        9  9 [1]\n"
        "\n"
        "seam[0] = argmin e[0][j]\n"
        "for i = 1 .. m-1:\n"
        "    c = seam[i-1]\n"
        "    seam[i] = cheapest of\n"
        "        e[i][c-1], e[i][c], e[i][c+1]",
        notes[4], text_width=6.1, code_size=15)
    deck.bullets_and_picture(
        "Greedy vs DP: much higher seam cost, visible damage", who, section,
        [f"Total energy removed is {f['cost_ratio_min']}x to {f['cost_ratio_max']}x the DP "
         "cost on our three images (30 % of the width)",
         "Greedy seams run into the subject: the tower gets thinner",
         f"Seam search is far faster, but the whole loop only {f['loop_speedup_greedy']}x: "
         f"{f['seams_total_ms_greedy_tower']} ms vs {f['seams_total_ms_dp_tower']} ms for "
         f"{f['seams_count_tower']} seams, because energy map and removal are O(mn) for both",
         "Verdict: not comparable in quality; the time saved is small"],
        "fig_compare_tower.png",
        notes[5], text_width=4.6,
        bullet_size=18)

    # ------------------------------------------------------------------ 3. Clarissa
    who, section = "Tan Ying Clarissa", "3. Graph cut"
    deck.bullets_and_picture(
        "Graph cut: a seam is a minimum s-t cut", who, section,
        ["One node per pixel, plus source s and sink t",
         "Forward edge (i,j) to (i,j+1) with capacity e[i][j]: cutting it removes pixel (i,j)",
         "Infinite backward edges: the source side is a prefix of each row, so each row is "
         "cut exactly once",
         "Infinite diagonal edges: neighbouring rows are cut at most 1 column apart, so the "
         "cut is a connected seam",
         "Cut capacity = seam energy, so min cut = optimal seam = max flow"],
        "fig_graph_construction.png",
        notes[6], text_width=5.4, bullet_size=18)
    deck.bullets_and_picture(
        "Finding: fix the last column; result equals DP, but slowly", who, section,
        ["Brief's wording: last column to sink with infinite capacity. Then the last column "
         "can never be removed and its energy is never used",
         "Fix: capacity e[i][n-1] on that edge (an imaginary extra column)",
         "Tests: seam cost = DP cost = max-flow value on 768 random grids; max flow = "
         "Boost's Edmonds-Karp",
         f"Speed: {f['gc_250_seconds']} s for one seam on {f['size_graphcut_250']}, about "
         f"{f['gc_500_seconds']} s on a full image; DP takes under a millisecond",
         "Worst case O(V E^2) = O((mn)^3)"],
        "fig_runtime_vs_size.png",
        notes[7], text_width=5.6, bullet_size=17)

    # ------------------------------------------------------------------ 4. Isaiah
    who, section = "Isaiah Low Jung", "4. Testing and CI/CD"
    deck.table_slide(
        f"Testing: {f['test_cases']} test cases, {f['test_assertions']} checks, all passing",
        who, section,
        ["What we check", "How"],
        [["DP is optimal", "equals brute force over ALL seams, 540 random grids up to 6 x 6"],
         ["Graph cut is optimal", "seam cost = DP cost = max-flow value, 768 random grids"],
         ["Our max-flow", "textbook networks; same value as Boost on 220 graphs"],
         ["Greedy", "valid seam, cost >= DP on 500 grids; hand-built grid 19 vs 4"],
         ["Shapes and directions", "exactly (m, n-k) or (m-k, n); horizontal = transpose trick"],
         ["Edge cases", "1 x 1, one column, n = 2, constant image, gray vs colour, down to 1 x 1, "
                        "enlarging"],
         ["Optimised variants", "rolling rows, OpenMP, incremental energy = plain versions"]],
        [3.3, 8.9],
        notes[8],
        footer_lines=["The graph-cut vs DP cross-check is what exposed the last-column problem"],
        font_size=15)
    deck.bullets_and_code(
        "CI/CD: every push is linted, built and tested", who, section,
        ["GitHub Actions on every push and pull request",
         "lint: clang-format must report no differences",
         "build-and-test: -Wall -Wextra -Werror, then the full Catch2 suite and a "
         "command-line end-to-end test",
         "app: the interactive app builds and passes a headless self-test",
         "Each milestone is merged to main through a pull request that must pass CI",
         "CD: pushing a version tag publishes a release (program, report, slides)"],
        "jobs:\n"
        "  lint:\n"
        "    clang-format --dry-run --Werror\n"
        "  build-and-test:\n"
        "    apt-get install libopencv-dev\n"
        "        catch2 libboost-graph-dev\n"
        "    cmake -DSEAM_WERROR=ON\n"
        "    cmake --build build\n"
        "    ctest --output-on-failure\n"
        "  app:\n"
        "    cmake -DSEAM_BUILD_APP=ON\n"
        "    seamcarve_app --selftest",
        notes[9], text_width=6.4, code_size=15)

    # ------------------------------------------------------------------ 5. Yasmin
    who, section = "Yasmin Tazi", "5. Visualisation and live demo"
    deck.picture_slide(
        "Step-by-step visualisation: every seam, drawn and animated", who, section,
        "fig_steps.png",
        "Red = seam about to be removed. An index map remembers where each pixel was in the "
        "original, so all seams can also be drawn on the original image. Output: MP4 or PNG frames.",
        notes[10])
    deck.bullets_above_picture(
        "Live demo: interactive resizing app", who, section,
        ["Dear ImGui + GLFW app: load an image, drag width and height sliders",
         "Choose DP, greedy or graph cut, and the energy function",
         "Views: result, all seams, energy map, step-by-step replay",
         "Shrinks and enlarges (seam insertion)",
         "Demo: 1) tower, DP, width to 70 %   2) all seams   3) greedy for contrast   "
         "4) step-by-step slider   5) enlarge   6) graph cut on a small image: same "
         "first-seam cost as DP, totals within 1 %"],
        "fig_extras.png",
        notes[11], text_height=2.75)

    # ------------------------------------------------------------------ 6. Kimberly
    who, section = "Kimberly Charlotte M. Ambrosio", "6. Analysis and conclusions"
    deck.bullets_and_code(
        "Item 2: exponentially many seams; divide and conquer does not help Sobel", who, section,
        ["2a-i: n choices in row 0, then at least 2 and at most 3 per row, so "
         "n*2^(m-1) <= #seams <= n*3^(m-1); brute-force counts confirm it",
         "Exactly 2^m for n = 2; growth rate 1 + 2cos(pi/(n+1)) in general",
         "2b: direct 3 x 3 Sobel is already Theta(mn), which is optimal",
         "Splitting in 4: T(N) = 4T(N/4) + O(sqrt N) = Theta(N), no gain",
         "FFT convolution is O(mn log mn): pays off only for large kernels",
         "D&C helps as a way to split work across cores, not asymptotically"],
        "Seams of an m x n image\n"
        "\n"
        " m   n=2    n=3     n=4\n"
        " 1    2      3       4\n"
        " 2    4      7      10\n"
        " 3    8     17      26\n"
        " 4   16     41      68\n"
        " 5   32     99     178\n"
        "\n"
        "lower bound n*2^(m-1)\n"
        "upper bound n*3^(m-1)",
        notes[12], text_width=7.0,
        code_size=15)
    deck.table_slide(
        "Complexity and comparison", who, section,
        ["Property", "DP", "Greedy", "Graph cut (Edmonds-Karp)"],
        [["Optimal seam?", "Yes", f"No ({f['cost_ratio_min']}x-{f['cost_ratio_max']}x cost)",
          "Yes (= DP cost)"],
         ["Seam search", "Theta(mn)", "Theta(m + n)", "O(V E^2) = O((mn)^3)"],
         ["k seams, full loop", "Theta(kmn)", "Theta(kmn)", "O(k (mn)^3)"],
         ["Memory", f"Theta(mn), {f['bytes_per_pixel_dp_full_tables']} B/pixel", "Theta(m)",
          f"Theta(mn), {f['bytes_per_pixel_graph_cut_network']} B/pixel"],
         [f"One seam, {f['size_dp_500']}", f"{f['seam_ms_dp_500']} ms",
          f"{f['seam_ms_greedy_500']} ms",
          f"{f['gc_500_seconds']} s"],
         ["Measured growth (all sizes)", f"N^{f['exp_dp']}", f"N^{f['exp_greedy']}",
          f"N^{f['exp_graphcut']}"]],
        [2.9, 2.6, 3.0, 3.7],
        notes[13],
        footer_lines=["Greedy's fast search is hidden by the O(mn) energy map and removal that "
                      "every method needs"],
        font_size=15)
    deck.bullets_above_picture(
        "Improvements (5d) and conclusions", who, section,
        [f"Whole DP table? No: 2 rolling cost rows + 1-byte parents instead of 8 bytes per "
         f"pixel: {f['mem_ratio']}x less memory, same seam",
         f"Multithreading (vs our serial loop): energy map up to "
         f"{f['threads_energy_large_best']}x faster; DP table "
         f"only {f['threads_dp_table_large_best']}x (rows depend on each other)",
         f"Incremental energy near the seam: energy step {f['inc_energy_speedup']}x faster",
         "Forward energy: fewer new edges, straighter lines",
         "Conclusion: DP is optimal, fast and simple; greedy is fast but poor; graph cut is "
         "correct but slow and generalises to video"],
        "fig_threads.png",
        notes[14],
        bullet_size=17, text_height=2.85)

    output = repo_dir / "slides" / "Team5_Seam_Carving.pptx"
    deck.save(output)

    # Speaker notes as a hand-out.
    lines = ["# Speaker notes", "",
             "Six sections, 2 minutes each, 12 minutes in total. The time stamps at the start of "
             "each note are targets. The same notes are in the notes pane of the slides.", ""]
    words_per_speaker = {}
    for number, title, speaker, text in deck.notes:
        lines += [f"## Slide {number}: {title}", "", f"**Speaker:** {speaker}", "", text, ""]
        words_per_speaker[speaker] = words_per_speaker.get(speaker, 0) + len(text.split())
    lines += ["## Word counts", "",
              "At about 150 words per minute, 280 words is roughly 1 minute 50 seconds.", "",
              "| Speaker | Words |", "|---|---|"]
    for speaker, words in words_per_speaker.items():
        lines.append(f"| {speaker} | {words} |")
        print(f"  {speaker}: {words} words")
    lines.append("")
    (repo_dir / "slides" / "speaker_notes.md").write_text("\n".join(lines))
    print(f"wrote {output} ({len(deck.notes)} slides) and speaker_notes.md")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    build(Path(sys.argv[1]).resolve())
