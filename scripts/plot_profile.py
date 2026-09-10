"""Plot a kl_profiler jsonl dump.

Every line of the input is one finished block, as written by
src/debug/profiler.c:

    { "label": "...", "timestamp": <ms>, "duration": <ms>, "depth": 0, "thread": 123 }

`timestamp` is the monotonic time (ms) the block began, `duration` is in ms.

The figure has three parts, all sharing the same label order (by total time):

    * total time per label (self time split out when blocks nest)
    * duration distribution per label (min-max, p25-p75, median)
    * one timeline lane per label: each call's duration over the run

A summary table is also printed to stdout.

Usage:
    uv run plot_profile.py /path/to/profile.txt
    uv run plot_profile.py profile.txt -o out.png --top 8 --filter raycast
    uv run plot_profile.py profile.txt --theme dark --show
"""

import argparse
import sys
from collections import defaultdict
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path
from typing import Literal

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.axes import Axes
from matplotlib.ticker import FuncFormatter, LogLocator, NullFormatter
from pydantic import BaseModel, ValidationError

# Durations are written with %.6f ms, so 1 ns is the smallest non-zero value.
# Clamp to it so zero-length blocks still land on a log axis.
MIN_DURATION_MS = 1e-6

# Labels can be up to 127 chars; longer ones are shortened for display only.
MAX_LABEL_CHARS = 48


class ProfilerPoint(BaseModel):
    label: str
    timestamp: float  # ms, monotonic, when the block began
    duration: float  # ms
    depth: int
    thread: int


@dataclass
class LabelStats:
    label: str
    starts: np.ndarray  # ms since the first sample
    durations: np.ndarray  # ms
    self_total: float  # ms, total minus time spent in direct children

    @property
    def count(self) -> int:
        return len(self.durations)

    @property
    def total(self) -> float:
        return float(self.durations.sum())

    def percentile(self, q: float) -> float:
        return float(np.percentile(self.durations, q))


# Colors per theme. Categorical color is never needed: every panel shows one
# series, and a label's identity is carried by its row / lane.
THEMES = {
    "light": {
        "surface": "#fcfcfb",
        "ink": "#0b0b0b",
        "ink_secondary": "#52514e",
        "ink_muted": "#898781",
        "grid": "#e1e0d9",
        "axis": "#c3c2b7",
        "accent": "#2a78d6",
        "accent_soft": "#9ec5f4",
    },
    "dark": {
        "surface": "#1a1a19",
        "ink": "#ffffff",
        "ink_secondary": "#c3c2b7",
        "ink_muted": "#898781",
        "grid": "#2c2c2a",
        "axis": "#383835",
        "accent": "#3987e5",
        "accent_soft": "#184f95",
    },
}


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------


def load_points(filepath: Path) -> Iterator[ProfilerPoint]:
    with filepath.open("r") as f:
        for lineno, line in enumerate(f, start=1):
            line = line.strip()
            if not line:
                continue
            try:
                yield ProfilerPoint.model_validate_json(line)
            except ValidationError:
                # The writer thread may have been mid-line when the file was read.
                print(f"warning: skipping malformed line {lineno}", file=sys.stderr)


def compute_self_times(points: list[ProfilerPoint]) -> list[float]:
    """Duration of each point minus the durations of its direct children.

    A parent begins no later than its children and has a smaller depth, so
    sorting each thread by (timestamp, depth) lets a stack walk find parents.
    """
    self_times = [p.duration for p in points]
    order = sorted(range(len(points)), key=lambda i: (points[i].thread, points[i].timestamp, points[i].depth))

    stack: list[int] = []
    thread = None
    for i in order:
        p = points[i]
        if p.thread != thread:
            stack.clear()
            thread = p.thread
        while stack and points[stack[-1]].depth >= p.depth:
            stack.pop()
        if stack and points[stack[-1]].depth == p.depth - 1:
            self_times[stack[-1]] -= p.duration
        stack.append(i)

    return [max(t, 0.0) for t in self_times]


def build_stats(points: list[ProfilerPoint]) -> list[LabelStats]:
    """Per-label stats, sorted by total time, largest first."""
    t0 = min(p.timestamp for p in points)
    self_times = compute_self_times(points)

    starts: dict[str, list[float]] = defaultdict(list)
    durations: dict[str, list[float]] = defaultdict(list)
    self_totals: dict[str, float] = defaultdict(float)
    for p, self_time in zip(points, self_times):
        starts[p.label].append(p.timestamp - t0)
        durations[p.label].append(p.duration)
        self_totals[p.label] += self_time

    stats = [
        LabelStats(
            label=label,
            starts=np.asarray(starts[label]),
            durations=np.asarray(durations[label]),
            self_total=self_totals[label],
        )
        for label in durations
    ]
    stats.sort(key=lambda s: s.total, reverse=True)
    return stats


# ---------------------------------------------------------------------------
# Formatting
# ---------------------------------------------------------------------------


def format_ms(ms: float) -> str:
    """Human-readable duration from a value in ms."""
    if ms <= 0:
        return "0"
    if ms < 1e-3:
        return f"{ms * 1e6:.3g} ns"
    if ms < 1:
        return f"{ms * 1e3:.3g} µs"
    if ms < 1e3:
        return f"{ms:.3g} ms"
    return f"{ms / 1e3:.3g} s"


def display_label(label: str) -> str:
    if len(label) <= MAX_LABEL_CHARS:
        return label
    keep = MAX_LABEL_CHARS - 1
    return label[: keep - keep // 3] + "…" + label[-(keep // 3):]


def format_share(ms: float, wall_ms: float) -> str:
    if wall_ms <= 0:
        return "-"
    pct = 100 * ms / wall_ms
    return f"{pct:.1f}%" if pct >= 0.1 else "<0.1%"


def print_summary(stats: list[LabelStats], wall_ms: float) -> None:
    headers = ["label", "calls", "total", "self", "% wall", "mean", "p50", "p95", "max"]
    rows = [
        [
            s.label,
            f"{s.count:,}",
            format_ms(s.total),
            format_ms(s.self_total),
            format_share(s.total, wall_ms),
            format_ms(s.total / s.count),
            format_ms(s.percentile(50)),
            format_ms(s.percentile(95)),
            format_ms(float(s.durations.max())),
        ]
        for s in stats
    ]
    widths = [max(len(r[i]) for r in [headers, *rows]) for i in range(len(headers))]

    def fmt(row: list[str]) -> str:
        first = row[0].ljust(widths[0])
        rest = (cell.rjust(w) for cell, w in zip(row[1:], widths[1:]))
        return "  ".join([first, *rest])

    print(fmt(headers))
    print("  ".join("-" * w for w in widths))
    for row in rows:
        print(fmt(row))


# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------


def style_axes(ax: Axes, theme: dict[str, str], grid_axis: Literal["both", "x", "y"] = "x") -> None:
    ax.set_facecolor(theme["surface"])
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(theme["axis"])
    ax.tick_params(which="both", colors=theme["ink_muted"], labelsize=8, length=0)
    ax.grid(axis=grid_axis, color=theme["grid"], linewidth=0.8)
    ax.set_axisbelow(True)


def log_duration_axis(axis) -> None:
    axis.set_major_locator(LogLocator(base=10))
    axis.set_minor_formatter(NullFormatter())
    axis.set_major_formatter(FuncFormatter(lambda v, _: format_ms(v)))


def plot_totals(ax: Axes, stats: list[LabelStats], wall_ms: float, theme: dict[str, str]) -> None:
    y = np.arange(len(stats))
    totals = np.array([s.total for s in stats])
    selfs = np.array([s.self_total for s in stats])
    nested = bool(np.any(selfs < totals - 1e-9))
    style_axes(ax, theme)

    if nested:
        ax.barh(y, totals, height=0.6, color=theme["accent_soft"], label="total (incl. children)")
        ax.barh(y, selfs, height=0.6, color=theme["accent"], label="self")
        ax.legend(loc="lower right", fontsize=8, frameon=False, labelcolor=theme["ink_secondary"])
    else:
        ax.barh(y, totals, height=0.6, color=theme["accent"])

    for yi, s in zip(y, stats):
        ax.annotate(
            f"{format_ms(s.total)}  ({s.count:,} calls · {format_share(s.total, wall_ms)} of wall)",
            xy=(s.total, yi),
            xytext=(6, 0),
            textcoords="offset points",
            va="center",
            fontsize=8,
            color=theme["ink_secondary"],
        )

    ax.set_yticks(y, [display_label(s.label) for s in stats], fontsize=9, color=theme["ink"])
    ax.invert_yaxis()
    ax.set_xlim(0, totals.max() * 1.6)  # room for the annotations
    ax.xaxis.set_major_formatter(FuncFormatter(lambda v, _: format_ms(v)))
    ax.set_title("Total time", loc="left", fontsize=10, color=theme["ink"], fontweight="bold")


def plot_distributions(ax: Axes, stats: list[LabelStats], theme: dict[str, str]) -> None:
    style_axes(ax, theme)
    for yi, s in enumerate(stats):
        d = np.maximum(s.durations, MIN_DURATION_MS)
        lo, p25, p50, p75, hi = np.percentile(d, [0, 25, 50, 75, 100])
        ax.hlines(yi, lo, hi, color=theme["ink_muted"], linewidth=1)
        ax.hlines(yi, p25, p75, color=theme["accent"], linewidth=7)
        ax.plot(p50, yi, "o", markersize=8, color=theme["accent"],
                markeredgecolor=theme["surface"], markeredgewidth=2)
        ax.annotate(
            f"p50 {format_ms(s.percentile(50))} · p95 {format_ms(s.percentile(95))}",
            xy=(hi, yi),
            xytext=(8, 0),
            textcoords="offset points",
            va="center",
            fontsize=8,
            color=theme["ink_secondary"],
        )

    ax.set_xscale("log")
    lo = min(max(float(s.durations.min()), MIN_DURATION_MS) for s in stats)
    hi = max(float(s.durations.max()) for s in stats)
    ax.set_xlim(lo / 2, hi * 100)  # room for the annotations
    log_duration_axis(ax.xaxis)
    ax.tick_params(axis="y", labelleft=False)
    ax.set_title("Duration per call  (min–max, p25–p75, median)", loc="left", fontsize=10,
                 color=theme["ink"], fontweight="bold")


def plot_lane(ax: Axes, s: LabelStats, span_s: float, theme: dict[str, str]) -> None:
    style_axes(ax, theme, grid_axis="both")
    x = s.starts / 1e3
    d = np.maximum(s.durations, MIN_DURATION_MS)
    ax.scatter(x, d, s=10, color=theme["accent"], alpha=0.6, linewidths=0)

    p50 = s.percentile(50)
    ax.axhline(max(p50, MIN_DURATION_MS), color=theme["ink_muted"], linewidth=1, linestyle="--")

    # Mark the slowest call so the eye lands on spikes.
    worst = int(np.argmax(s.durations))
    ax.annotate(
        f"max {format_ms(float(s.durations[worst]))}",
        xy=(x[worst], d[worst]),
        xytext=(5, 4),
        textcoords="offset points",
        fontsize=7,
        color=theme["ink_secondary"],
    )

    ax.set_yscale("log")
    ymin, ymax = float(d.min()), float(d.max())
    ax.set_ylim(ymin / 2, ymax * 3)
    log_duration_axis(ax.yaxis)
    ax.yaxis.tick_right()
    ax.set_xlim(-span_s * 0.01, span_s * 1.01)

    # Lane name in the same left column as the summary rows' labels.
    ax.annotate(display_label(s.label), xy=(0, 0.5), xycoords="axes fraction", xytext=(-3.5, 1),
                textcoords="offset points", ha="right", va="bottom", fontsize=9, color=theme["ink"])
    ax.annotate(f"{s.count:,} calls · median {format_ms(p50)} (dashed)", xy=(0, 0.5),
                xycoords="axes fraction", xytext=(-3.5, -1), textcoords="offset points",
                ha="right", va="top", fontsize=7.5, color=theme["ink_muted"])


def plot_profile(stats: list[LabelStats], wall_ms: float, title: str, subtitle: str,
                 theme: dict[str, str]) -> plt.Figure:
    n = len(stats)
    span_s = wall_ms / 1e3

    summary_h = 0.42 * n + 1.0
    lane_h = 1.0
    fig = plt.figure(figsize=(15, summary_h + lane_h * n + 1.4), facecolor=theme["surface"])
    grid = fig.add_gridspec(
        2, 1,
        height_ratios=[summary_h, lane_h * n],
        hspace=0.12 + 0.6 / (summary_h + lane_h * n),
    )

    top = grid[0].subgridspec(1, 2, width_ratios=[1, 1], wspace=0.04)
    ax_totals = fig.add_subplot(top[0])
    ax_dist = fig.add_subplot(top[1], sharey=ax_totals)
    plot_totals(ax_totals, stats, wall_ms, theme)
    plot_distributions(ax_dist, stats, theme)

    lanes = grid[1].subgridspec(n, 1, hspace=0.15)
    lane_axes: list[Axes] = []
    for i, s in enumerate(stats):
        ax = fig.add_subplot(lanes[i], sharex=lane_axes[0] if lane_axes else None)
        plot_lane(ax, s, span_s, theme)
        if i < n - 1:
            ax.tick_params(axis="x", labelbottom=False)
        lane_axes.append(ax)
    lane_axes[0].set_title("Timeline  (each dot is one call)", loc="left", fontsize=10,
                           color=theme["ink"], fontweight="bold")
    lane_axes[-1].set_xlabel("time since first sample (s)", fontsize=9, color=theme["ink_secondary"])

    fig.suptitle(title, x=0.01, ha="left", fontsize=14, fontweight="bold", color=theme["ink"])
    fig.text(0.01, 1 - 0.55 / fig.get_figheight(), subtitle, ha="left", va="top",
             fontsize=9, color=theme["ink_secondary"])

    # Size the left column to the widest label so nothing gets clipped.
    label_w = max(t.get_window_extent().width for t in ax_totals.get_yticklabels()) / fig.dpi
    width, height = fig.get_figwidth(), fig.get_figheight()
    fig.subplots_adjust(left=(label_w + 0.3) / width, right=1 - 0.7 / width,
                        top=1 - 0.9 / height, bottom=0.5 / height)
    return fig


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot a kl_profiler jsonl dump.")
    parser.add_argument("file", type=Path, help="profiler output (jsonl)")
    parser.add_argument("-o", "--output", type=Path,
                        help="image to write (default: <input name>.png in the current directory)")
    parser.add_argument("--top", type=int, default=10, help="plot the N labels with the most total time (default: 10)")
    parser.add_argument("--filter", default=None, help="only include labels containing this substring")
    parser.add_argument("--thread", type=int, default=None, help="only include points from this thread id")
    parser.add_argument("--theme", choices=THEMES.keys(), default="light")
    parser.add_argument("--dpi", type=int, default=150)
    parser.add_argument("--show", action="store_true", help="also open an interactive window")
    args = parser.parse_args()

    points = list(load_points(args.file))
    if args.filter:
        points = [p for p in points if args.filter in p.label]
    if args.thread is not None:
        points = [p for p in points if p.thread == args.thread]
    if not points:
        sys.exit(f"no points to plot in {args.file}")

    stats = build_stats(points)
    wall_ms = max(p.timestamp + p.duration for p in points) - min(p.timestamp for p in points)
    print_summary(stats, wall_ms)

    shown = stats[: max(args.top, 1)]
    threads = len({p.thread for p in points})
    subtitle = (
        f"{len(points):,} samples  ·  {len(stats)} labels  ·  "
        f"{threads} thread{'s' if threads != 1 else ''}  ·  {format_ms(wall_ms)} span"
    )
    hidden = stats[len(shown):]
    if hidden:
        subtitle += f"  ·  {len(hidden)} more labels not shown ({format_ms(sum(s.total for s in hidden))})"

    fig = plot_profile(shown, wall_ms, f"Profile  —  {args.file.name}", subtitle, THEMES[args.theme])

    output = args.output or Path(f"{args.file.stem}.png")
    fig.savefig(output, dpi=args.dpi, facecolor=fig.get_facecolor())
    print(f"\nwrote {output}")

    if args.show:
        plt.show()


if __name__ == "__main__":
    main()
