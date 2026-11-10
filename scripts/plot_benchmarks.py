#!/usr/bin/env python3
import csv
import html
import sys
from pathlib import Path

def svg_bar(title, labels, values, ylabel):
    width, height = 960, 520
    left, top, bottom = 90, 55, 90
    chart_h = height - top - bottom
    chart_w = width - left - 30
    vmax = max(values) if values else 1.0
    if vmax <= 0:
        vmax = 1.0
    gap = chart_w / max(1, len(values))
    bar_w = gap * 0.62

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width/2}" y="28" text-anchor="middle" font-size="20">{html.escape(title)}</text>',
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top+chart_h}" stroke="black"/>',
        f'<line x1="{left}" y1="{top+chart_h}" x2="{left+chart_w}" y2="{top+chart_h}" stroke="black"/>',
        f'<text x="18" y="{top+chart_h/2}" transform="rotate(-90 18 {top+chart_h/2})" text-anchor="middle" font-size="14">{html.escape(ylabel)}</text>',
    ]
    for i, (label, value) in enumerate(zip(labels, values)):
        x = left + i * gap + (gap - bar_w) / 2
        h = chart_h * value / vmax
        y = top + chart_h - h
        parts.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_w:.1f}" height="{h:.1f}" fill="#555"/>')
        parts.append(f'<text x="{x+bar_w/2:.1f}" y="{y-6:.1f}" text-anchor="middle" font-size="11">{value:.3f}</text>')
        parts.append(f'<text x="{x+bar_w/2:.1f}" y="{top+chart_h+20}" text-anchor="middle" font-size="11">{html.escape(label)}</text>')
    parts.append('</svg>')
    return "\n".join(parts)

def main():
    if len(sys.argv) != 3:
        raise SystemExit("Uso: plot_benchmarks.py summary.csv outdir")
    summary = Path(sys.argv[1])
    outdir = Path(sys.argv[2])
    outdir.mkdir(parents=True, exist_ok=True)

    rows = list(csv.DictReader(summary.open()))
    groups = {}
    for row in rows:
        key = (row["kernel"], row["size"])
        groups.setdefault(key, []).append(row)

    for (kernel, size), items in groups.items():
        items.sort(key=lambda r: (r["config"], int(r["threads"])))
        labels = [
            r["config"] if r["config"] != "C3"
            else f'C3-t{r["threads"]}' for r in items
        ]
        runtime = [float(r["run_median_ms"]) for r in items]
        compile_time = [float(r["compile_median_ms"]) for r in items]

        (outdir / f"runtime_{kernel}_{size}.svg").write_text(
            svg_bar(f"Runtime — {kernel} — N={size}",
                    labels, runtime, "mediana (ms)"))
        (outdir / f"compile_{kernel}_{size}.svg").write_text(
            svg_bar(f"Compilacao — {kernel} — N={size}",
                    labels, compile_time, "mediana (ms)"))

        c0 = next((float(r["run_median_ms"]) for r in items
                   if r["config"] == "C0"), None)
        if c0:
            speed_labels, speed_values = [], []
            for r in items:
                if r["config"] == "C3":
                    speed_labels.append(f'C3-t{r["threads"]}')
                    value = float(r["run_median_ms"])
                    speed_values.append(c0 / value if value else 0.0)
            if speed_values:
                (outdir / f"speedup_{kernel}_{size}.svg").write_text(
                    svg_bar(f"Speedup C3 vs C0 — {kernel} — N={size}",
                            speed_labels, speed_values, "speedup (x)"))

if __name__ == "__main__":
    main()
