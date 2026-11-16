#!/usr/bin/env bash
# Builds report/report.pdf from the Markdown sections.
#
#   report/build_report.sh [--strict]
#
# Pipeline:  title_page.md (title page) + sections/*.md  (in file-name order)
#              -> tables from generated/*.md are inserted at "<!-- include: ... -->" lines
#              -> "<!-- include-glob: pattern -->" lines insert every matching file
#              -> @@key@@ placeholders are replaced by the numbers in generated/facts.txt
#              -> pandoc -> one standalone HTML file (MathML for formulas)
#              -> a Chromium-based browser in headless mode prints it to PDF
#
# Run benchmarks/run_all.sh first: it creates generated/ and figures/. Without it the
# report still builds, with "(pending)" in place of the missing numbers and tables;
# --strict turns a missing number into an error (use it for the final submission).
#
# Environment variables:
#   CHROME   path of a Chrome / Chromium / Edge executable. If not set, the script looks
#            for the usual names, and for Windows Chrome when running inside WSL.
set -euo pipefail

REPORT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$REPORT_DIR/build"
FACTS="$REPORT_DIR/generated/facts.txt"
mkdir -p "$BUILD_DIR"

STRICT=0
if [[ "${1:-}" == "--strict" ]]; then
    STRICT=1
fi

# 1. Join the sections, inserting the generated tables.
COMBINED="$BUILD_DIR/report.md"
: > "$COMBINED"
for section in "$REPORT_DIR"/sections/*.md; do
    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" =~ ^\<!--\ include:\ (.+)\ --\>$ ]]; then
            # A generated table. It is missing until the benchmarks have been run.
            if [[ -f "$REPORT_DIR/${BASH_REMATCH[1]}" ]]; then
                cat "$REPORT_DIR/${BASH_REMATCH[1]}" >> "$COMBINED"
            else
                printf '*(table %s: not generated yet)*\n' "${BASH_REMATCH[1]}" >> "$COMBINED"
            fi
        elif [[ "$line" =~ ^\<!--\ include-glob:\ (.+)\ --\>$ ]]; then
            # Every file matching the pattern, in name order (used for the per-member rows
            # of the AI declaration, so each member edits only their own file).
            for part in "$REPORT_DIR"/${BASH_REMATCH[1]}; do
                [[ -f "$part" ]] && cat "$part" >> "$COMBINED"
            done
        else
            printf '%s\n' "$line" >> "$COMBINED"
        fi
    done < "$section"
    printf '\n' >> "$COMBINED"
done

# 2. Replace every @@key@@ by its value from facts.txt.
SED_SCRIPT="$BUILD_DIR/facts.sed"
: > "$SED_SCRIPT"
if [[ -f "$FACTS" ]]; then
    while IFS='=' read -r key value; do
        [[ -z "$key" ]] && continue
        escaped=$(printf '%s' "$value" | sed -e 's/[\\&|]/\\&/g')
        printf 's|@@%s@@|%s|g\n' "$key" "$escaped" >> "$SED_SCRIPT"
    done < "$FACTS"
    sed -i -f "$SED_SCRIPT" "$COMBINED"
else
    echo "warning: $FACTS not found (run benchmarks/run_all.sh to create it)" >&2
fi

# Numbers that are still missing (benchmarks not run, or not merged yet) are shown as
# "(pending)" so the report still builds. With --strict a missing number is an error;
# use that for the final submission.
if grep -q '@@[A-Za-z0-9_]*@@' "$COMBINED"; then
    missing=$(grep -o '@@[A-Za-z0-9_]*@@' "$COMBINED" | sort -u | tr '\n' ' ')
    if [[ "$STRICT" == 1 ]]; then
        echo "error: no value in facts.txt for: $missing" >&2
        exit 1
    fi
    echo "warning: shown as (pending), no value in facts.txt for: $missing" >&2
    sed -i -e 's/@@[A-Za-z0-9_]*@@/(pending)/g' "$COMBINED"
fi

# 3. Markdown -> standalone HTML. Run from the report directory so that pandoc finds the
#    figures and embeds them into the single HTML file.
cd "$REPORT_DIR"
# The title page (title, team ID, members) comes before the table of contents.
pandoc title_page.md --from markdown --to html5 --output build/title_page.html
pandoc "build/report.md" \
    --include-before-body build/title_page.html \
    --from markdown --to html5 --standalone --embed-resources \
    --mathml --toc --toc-depth=2 --number-sections \
    --css style.css --metadata pagetitle="Team 5 - Seam Carving" --metadata lang=en \
    --output "build/report.html"

# 4. HTML -> PDF with a headless browser.
find_chrome() {
    if [[ -n "${CHROME:-}" ]]; then
        echo "$CHROME"
        return
    fi
    for name in google-chrome google-chrome-stable chromium chromium-browser microsoft-edge; do
        if command -v "$name" > /dev/null 2>&1; then
            command -v "$name"
            return
        fi
    done
    for path in "/mnt/c/Program Files/Google/Chrome/Application/chrome.exe" \
                "/mnt/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe" \
                "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"; do
        if [[ -x "$path" ]]; then
            echo "$path"
            return
        fi
    done
}

BROWSER="$(find_chrome)"
if [[ -z "$BROWSER" ]]; then
    echo "error: no Chrome/Chromium found. Set CHROME=/path/to/browser." >&2
    echo "The HTML version is ready at $BUILD_DIR/report.html" >&2
    exit 1
fi

HTML_PATH="$BUILD_DIR/report.html"
PDF_PATH="$REPORT_DIR/report.pdf"
if [[ "$BROWSER" == *.exe ]]; then
    # A Windows browser started from WSL needs Windows-style paths.
    HTML_URL="file:///$(wslpath -m "$HTML_PATH")"
    PDF_ARG="$(wslpath -w "$PDF_PATH")"
else
    HTML_URL="file://$HTML_PATH"
    PDF_ARG="$PDF_PATH"
fi

"$BROWSER" --headless --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$PDF_ARG" "$HTML_URL" > /dev/null 2>&1

echo "wrote $PDF_PATH"
