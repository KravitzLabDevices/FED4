#!/usr/bin/env python3
"""FED4 touch drift analysis.

Reads the touch diagnostic CSVs written by ``FED4::logTouch()``
(``*_T.CSV``, see docs/wiki/Poke-Functionality.md) from one or more devices and
answers the four questions the instrumentation was added for:

1. Is the failure baseline drift or sensitivity loss?
2. Are idle and poke values consistent across devices and across L/C/R ports?
3. What does the failure actually look like just before pokes stop?
4. Does light sleep behave differently from full wake?

Two detection models run concurrently in firmware and are logged side by side:

    hardware   smooth - benchmark >= wakeAbs      (benchmark auto-tracks drift)
    software   smooth - idle      >= riseThresh * idle   (idle frozen at char)

Comparing them is the core measurement: ``Idle`` is flat by construction, so if
``Bench`` rises while ``Idle`` does not, hardware tracking has absorbed
something the software model cannot see.

Usage:
    python fed4_touch_analysis.py /path/to/sd_dumps -o out/
    python fed4_touch_analysis.py A_T.CSV B_T.CSV -o out/

Install: pip install -r requirements.txt
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt  # noqa: E402
import pandas as pd  # noqa: E402

PADS = ("L", "C", "R")
PAD_NAMES = {"L": "Left", "C": "Center", "R": "Right"}
PAD_COLORS = {"L": "#1f77b4", "C": "#ff7f0e", "R": "#2ca02c"}

CONTEXT_COLS = [
    "Cage",
    "RackSlot",
    "Orientation",
    "FrontPlate",
    "BatterySide",
    "PokeModule",
]

NUMERIC_COLS = (
    [f"Smooth{p}" for p in PADS]
    + [f"Bench{p}" for p in PADS]
    + [f"Idle{p}" for p in PADS]
    + [f"Std{p}" for p in PADS]
    + [f"RiseThresh{p}" for p in PADS]
    + [f"WakeAbs{p}" for p in PADS]
    + [
        "ElapsedSeconds",
        "LatchPad",
        "ConfirmPad",
        "PeakSmooth",
        "PokeDuration",
        "RecharCount",
        "ProxMm",
        "Temperature",
        "Humidity",
        "BatteryVoltage",
        "BatteryPercent",
        "WakeCount",
    ]
)


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------


def find_files(inputs: list[str]) -> list[Path]:
    """Expand directories to their ``*_T.CSV`` files; keep explicit files."""
    files: list[Path] = []
    for item in inputs:
        path = Path(item)
        if path.is_dir():
            files.extend(sorted(path.rglob("*_T.CSV")))
            files.extend(sorted(path.rglob("*_T.csv")))
        elif path.is_file():
            files.append(path)
        else:
            print(f"  ! not found, skipping: {path}", file=sys.stderr)
    # rglob on a case-insensitive filesystem can return the same file twice
    unique = {p.resolve(): p for p in files}
    return list(unique.values())


def load(files: list[Path]) -> pd.DataFrame:
    frames = []
    for path in files:
        try:
            df = pd.read_csv(path)
        except Exception as exc:  # noqa: BLE001 - report and continue
            print(f"  ! failed to read {path.name}: {exc}", file=sys.stderr)
            continue
        if "RowType" not in df.columns:
            print(f"  ! not a touch log (no RowType): {path.name}", file=sys.stderr)
            continue
        df["SourceFile"] = path.name
        frames.append(df)
        print(f"  + {path.name}: {len(df)} rows")

    if not frames:
        raise SystemExit("No touch logs found. Point this at a folder of *_T.CSV files.")

    df = pd.concat(frames, ignore_index=True)

    df["DateTime"] = pd.to_datetime(df["DateTime"], errors="coerce")
    for col in NUMERIC_COLS:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    # Motion is "Disabled" when the PIR is off — keep a numeric twin
    if "Motion" in df.columns:
        df["MotionPct"] = pd.to_numeric(df["Motion"], errors="coerce")

    # Context is stamped on BootChar rows only; broadcast it per device+file
    for col in CONTEXT_COLS:
        if col not in df.columns:
            df[col] = pd.NA
    boot = df[df["RowType"] == "BootChar"]
    for (uid, src), grp in boot.groupby(["DeviceUID", "SourceFile"], dropna=False):
        mask = (df["DeviceUID"] == uid) & (df["SourceFile"] == src)
        for col in CONTEXT_COLS:
            vals = grp[col].dropna()
            vals = vals[vals.astype(str).str.strip() != ""]
            if not vals.empty:
                df.loc[mask, col] = vals.iloc[0]

    df["Device"] = df["DeviceUID"].astype(str).str[-6:]

    # Derived: what each detection model actually sees
    for p in PADS:
        df[f"HwDelta{p}"] = df[f"Smooth{p}"] - df[f"Bench{p}"]
        df[f"SwDelta{p}"] = df[f"Smooth{p}"] - df[f"Idle{p}"]
        df[f"BenchDrift{p}"] = df[f"Bench{p}"] - df[f"Idle{p}"]
        df[f"RiseFrac{p}"] = df[f"SwDelta{p}"] / df[f"Idle{p}"]

    # Poke amplitude against both baselines (per-row resolved pad)
    pad_to_letter = {"Left": "L", "Center": "C", "Right": "R"}
    df["PadLetter"] = df["Pad"].map(pad_to_letter)
    df["PeakOverIdle"] = pd.NA
    df["PeakOverBench"] = pd.NA
    for letter in PADS:
        mask = df["PadLetter"] == letter
        df.loc[mask, "PeakOverIdle"] = df.loc[mask, "PeakSmooth"] - df.loc[mask, f"Idle{letter}"]
        df.loc[mask, "PeakOverBench"] = df.loc[mask, "PeakSmooth"] - df.loc[mask, f"Bench{letter}"]
    df["PeakOverIdle"] = pd.to_numeric(df["PeakOverIdle"], errors="coerce")
    df["PeakOverBench"] = pd.to_numeric(df["PeakOverBench"], errors="coerce")

    df = df.sort_values(["DeviceUID", "DateTime"], kind="stable").reset_index(drop=True)
    return df


def _devices(df: pd.DataFrame) -> list:
    return sorted(df["Device"].dropna().unique())


def _save(fig, out_dir: Path, name: str) -> None:
    path = out_dir / name
    fig.tight_layout()
    fig.savefig(path, dpi=130)
    plt.close(fig)
    print(f"  -> {path}")


# ---------------------------------------------------------------------------
# Panel 1 — baseline drift
# ---------------------------------------------------------------------------


def panel_baseline_drift(df: pd.DataFrame, out_dir: Path) -> None:
    """Idle (flat by construction) vs Bench (should track) vs Smooth, with
    temperature / humidity / battery overlaid as candidate drivers."""
    hb = df[df["RowType"] == "Heartbeat"]
    if hb.empty:
        print("  (no Heartbeat rows - skipping baseline drift)")
        return

    devices = _devices(hb)
    fig, axes = plt.subplots(
        len(devices), 2, figsize=(15, 3.6 * len(devices)), squeeze=False
    )

    for row, dev in enumerate(devices):
        d = hb[hb["Device"] == dev]
        ax = axes[row][0]
        for p in PADS:
            ax.plot(d["DateTime"], d[f"Idle{p}"], color=PAD_COLORS[p], ls="--",
                    lw=1.0, label=f"{PAD_NAMES[p]} idle (frozen)")
            ax.plot(d["DateTime"], d[f"Bench{p}"], color=PAD_COLORS[p], lw=1.6,
                    label=f"{PAD_NAMES[p]} benchmark (HW)")
            ax.plot(d["DateTime"], d[f"Smooth{p}"], color=PAD_COLORS[p], lw=0.7,
                    alpha=0.35, label=f"{PAD_NAMES[p]} smooth")
        ax.set_title(f"{dev} — baselines on heartbeats")
        ax.set_ylabel("touch counts")
        ax.legend(fontsize=6, ncol=3)
        ax.grid(alpha=0.25)

        # Benchmark drift away from the frozen idle is the drift signal itself
        ax2 = axes[row][1]
        for p in PADS:
            ax2.plot(d["DateTime"], d[f"BenchDrift{p}"], color=PAD_COLORS[p],
                     lw=1.4, label=f"{PAD_NAMES[p]} bench − idle")
            ax2.axhline(0, color="grey", lw=0.6)
        ax2.set_ylabel("counts")
        ax2.set_title(f"{dev} — benchmark drift + covariates")
        ax2.grid(alpha=0.25)

        cov = ax2.twinx()
        for col, color, label in (
            ("Temperature", "#d62728", "temp °C"),
            ("Humidity", "#9467bd", "humidity %"),
            ("BatteryVoltage", "#8c564b", "battery V"),
        ):
            if col in d.columns and d[col].notna().any():
                cov.plot(d["DateTime"], d[col], color=color, lw=0.9, alpha=0.6,
                         label=label)
        cov.set_ylabel("temp / humidity / battery")

        handles = ax2.get_legend_handles_labels()[0] + cov.get_legend_handles_labels()[0]
        labels = ax2.get_legend_handles_labels()[1] + cov.get_legend_handles_labels()[1]
        ax2.legend(handles, labels, fontsize=6, ncol=2)

    fig.suptitle("1. Baseline drift — idle is frozen, benchmark tracks", y=1.0)
    _save(fig, out_dir, "01_baseline_drift.png")


# ---------------------------------------------------------------------------
# Panel 2 — poke sensitivity
# ---------------------------------------------------------------------------


def panel_sensitivity(df: pd.DataFrame, out_dir: Path) -> None:
    """PeakSmooth − Idle and PeakSmooth − Bench over time, per pad. If the
    baselines are stable but these shrink, it is true sensitivity loss."""
    pokes = df[(df["RowType"] == "Poke") & df["PadLetter"].notna()]
    if pokes.empty:
        print("  (no Poke rows - skipping sensitivity)")
        return

    devices = _devices(pokes)
    fig, axes = plt.subplots(
        len(devices), 2, figsize=(15, 3.4 * len(devices)), squeeze=False
    )

    for row, dev in enumerate(devices):
        d = pokes[pokes["Device"] == dev]
        for col, metric, title in (
            (0, "PeakOverIdle", "peak − idle (software model)"),
            (1, "PeakOverBench", "peak − benchmark (hardware model)"),
        ):
            ax = axes[row][col]
            for p in PADS:
                dp = d[d["PadLetter"] == p]
                if dp.empty:
                    continue
                ax.scatter(dp["DateTime"], dp[metric], s=12, alpha=0.65,
                           color=PAD_COLORS[p], label=PAD_NAMES[p])
                # Threshold the pad must clear for that model
                thresh = dp[f"WakeAbs{p}"]
                ax.plot(dp["DateTime"], thresh, color=PAD_COLORS[p], ls=":", lw=1.0)
            ax.axhline(0, color="grey", lw=0.6)
            ax.set_title(f"{dev} — {title}")
            ax.set_ylabel("counts")
            ax.legend(fontsize=7)
            ax.grid(alpha=0.25)

    fig.suptitle(
        "2. Poke sensitivity over time (dotted = that pad's wakeAbs threshold)", y=1.0
    )
    _save(fig, out_dir, "02_poke_sensitivity.png")


# ---------------------------------------------------------------------------
# Panel 3 — cross-device / cross-port
# ---------------------------------------------------------------------------


def panel_cross_device(df: pd.DataFrame, out_dir: Path) -> tuple[pd.DataFrame, pd.DataFrame]:
    """BootChar characterization table plus poke-delta histograms faceted by
    device and pad. One port dead across all devices points at FED4-wide
    calibration; one device only points at the module."""
    boot = df[df["RowType"] == "BootChar"]
    rows = []
    for _, r in boot.iterrows():
        for p in PADS:
            rows.append(
                {
                    "Device": r["Device"],
                    "DateTime": r["DateTime"],
                    "Mode": r.get("Mode"),
                    "Pad": PAD_NAMES[p],
                    "Idle": r[f"Idle{p}"],
                    "Std": r[f"Std{p}"],
                    "RiseThresh": r[f"RiseThresh{p}"],
                    "WakeAbs": r[f"WakeAbs{p}"],
                    **{c: r.get(c) for c in CONTEXT_COLS},
                }
            )
    boot_table = pd.DataFrame(rows)

    pokes = df[(df["RowType"] == "Poke") & df["PadLetter"].notna()]
    poke_table = pd.DataFrame()
    if not pokes.empty:
        poke_table = (
            pokes.groupby(["Device", "Pad"])
            .agg(
                n=("PeakOverIdle", "size"),
                peak_over_idle_median=("PeakOverIdle", "median"),
                peak_over_bench_median=("PeakOverBench", "median"),
                hold_ms_median=("PokeDuration", "median"),
            )
            .reset_index()
        )

        devices = _devices(pokes)
        fig, axes = plt.subplots(
            len(devices), 3, figsize=(14, 3.0 * len(devices)), squeeze=False
        )
        for row, dev in enumerate(devices):
            for col, p in enumerate(PADS):
                ax = axes[row][col]
                dp = pokes[(pokes["Device"] == dev) & (pokes["PadLetter"] == p)]
                if dp.empty:
                    ax.text(0.5, 0.5, "no pokes", ha="center", va="center",
                            transform=ax.transAxes, color="crimson")
                else:
                    ax.hist(dp["PeakOverIdle"].dropna(), bins=25,
                            color=PAD_COLORS[p], alpha=0.8)
                    wa = dp[f"WakeAbs{p}"].median()
                    if pd.notna(wa):
                        ax.axvline(wa, color="black", ls="--", lw=1.0)
                ax.set_title(f"{dev} · {PAD_NAMES[p]}", fontsize=9)
                ax.set_xlabel("peak − idle (counts)")
                ax.grid(alpha=0.2)
        fig.suptitle(
            "3. Poke delta by device × port (dashed = wakeAbs threshold)", y=1.0
        )
        _save(fig, out_dir, "03_cross_device_port.png")

    return boot_table, poke_table


# ---------------------------------------------------------------------------
# Panel 4 — failure signature
# ---------------------------------------------------------------------------


def panel_failure_signature(
    df: pd.DataFrame, out_dir: Path, drought_minutes: float
) -> pd.DataFrame:
    """Poke droughts, TouchMiss rate, and Rechar events with the proximity and
    motion readings at that moment (the 'recalibrated with a mouse nearby' test)."""
    devices = _devices(df)
    fig, axes = plt.subplots(
        len(devices), 1, figsize=(14, 3.2 * len(devices)), squeeze=False
    )

    drought_rows = []
    for row, dev in enumerate(devices):
        d = df[df["Device"] == dev]
        ax = axes[row][0]

        pokes = d[d["RowType"] == "Poke"]
        misses = d[d["RowType"] == "TouchMiss"]
        rechars = d[d["RowType"] == "Rechar"]
        hb = d[d["RowType"] == "Heartbeat"]

        # Benchmark drift as the background signal
        for p in PADS:
            if not hb.empty:
                ax.plot(hb["DateTime"], hb[f"BenchDrift{p}"], color=PAD_COLORS[p],
                        lw=1.0, alpha=0.7, label=f"{PAD_NAMES[p]} bench − idle")

        for _, r in rechars.iterrows():
            ax.axvline(r["DateTime"], color="crimson", lw=1.4, alpha=0.8)
            note = f"rechar  prox={r.get('ProxMm')}mm  motion={r.get('Motion')}"
            ax.annotate(note, (r["DateTime"], ax.get_ylim()[1]), fontsize=6,
                        rotation=90, va="top", color="crimson")

        if not pokes.empty:
            ax.scatter(pokes["DateTime"], [0] * len(pokes), marker="|", s=120,
                       color="black", label="poke")
        if not misses.empty:
            ax.scatter(misses["DateTime"], [0] * len(misses), marker="x", s=28,
                       color="crimson", label="TouchMiss")

        # Poke droughts: gaps between consecutive resolved pokes
        if len(pokes) >= 2:
            times = pokes["DateTime"].sort_values()
            gaps = times.diff().dt.total_seconds() / 60.0
            for start, gap in zip(times.shift(1)[1:], gaps[1:]):
                if gap >= drought_minutes:
                    ax.axvspan(start, start + pd.Timedelta(minutes=gap),
                               color="orange", alpha=0.15)
                    lead = hb[hb["DateTime"] <= start].tail(3)
                    drought_rows.append(
                        {
                            "Device": dev,
                            "DroughtStart": start,
                            "DroughtMinutes": round(gap, 1),
                            **{
                                f"BenchDrift{p}_before": lead[f"BenchDrift{p}"].mean()
                                for p in PADS
                            },
                            "TouchMissesInGap": int(
                                (
                                    (misses["DateTime"] > start)
                                    & (
                                        misses["DateTime"]
                                        <= start + pd.Timedelta(minutes=gap)
                                    )
                                ).sum()
                            ),
                            "RecharsInGap": int(
                                (
                                    (rechars["DateTime"] > start)
                                    & (
                                        rechars["DateTime"]
                                        <= start + pd.Timedelta(minutes=gap)
                                    )
                                ).sum()
                            ),
                        }
                    )

        touch_rows = len(pokes) + len(misses)
        miss_rate = (len(misses) / touch_rows * 100.0) if touch_rows else float("nan")
        ax.set_title(
            f"{dev} — pokes={len(pokes)}  TouchMiss={len(misses)} "
            f"({miss_rate:.1f}% of touch wakes)  Rechar={len(rechars)}"
        )
        ax.set_ylabel("bench − idle (counts)")
        ax.legend(fontsize=6, ncol=4)
        ax.grid(alpha=0.25)

    fig.suptitle(
        f"4. Failure signature — shaded = poke drought >= {drought_minutes:g} min", y=1.0
    )
    _save(fig, out_dir, "04_failure_signature.png")
    return pd.DataFrame(drought_rows)


# ---------------------------------------------------------------------------
# Panel 5 — sleep vs awake
# ---------------------------------------------------------------------------


def panel_sleep_vs_awake(df: pd.DataFrame, out_dir: Path) -> pd.DataFrame:
    """The same measures split by Mode. If the awake arm stays healthy while the
    sleep arm dies, the fault is the light-sleep / benchmark / latch path."""
    modes = [m for m in df["Mode"].dropna().unique()]
    if len(modes) < 2:
        print(f"  (only one Mode present: {modes or 'none'} - comparison is trivial)")

    fig, axes = plt.subplots(1, 3, figsize=(15, 4.2))

    hb = df[df["RowType"] == "Heartbeat"]
    ax = axes[0]
    data, labels = [], []
    for mode in modes:
        for p in PADS:
            vals = hb.loc[hb["Mode"] == mode, f"BenchDrift{p}"].dropna()
            if not vals.empty:
                data.append(vals)
                labels.append(f"{mode}\n{PAD_NAMES[p]}")
    if data:
        ax.boxplot(data, showfliers=False)
        ax.set_xticks(range(1, len(labels) + 1))
        ax.set_xticklabels(labels)
    ax.set_title("benchmark - idle on heartbeats")
    ax.set_ylabel("counts")
    ax.tick_params(axis="x", labelsize=7)
    ax.grid(alpha=0.25)

    pokes = df[(df["RowType"] == "Poke") & df["PadLetter"].notna()]
    ax = axes[1]
    data, labels = [], []
    for mode in modes:
        for p in PADS:
            vals = pokes.loc[
                (pokes["Mode"] == mode) & (pokes["PadLetter"] == p), "PeakOverIdle"
            ].dropna()
            if not vals.empty:
                data.append(vals)
                labels.append(f"{mode}\n{PAD_NAMES[p]}")
    if data:
        ax.boxplot(data, showfliers=False)
        ax.set_xticks(range(1, len(labels) + 1))
        ax.set_xticklabels(labels)
    ax.set_title("poke delta (peak - idle)")
    ax.set_ylabel("counts")
    ax.tick_params(axis="x", labelsize=7)
    ax.grid(alpha=0.25)

    ax = axes[2]
    summary_rows = []
    for mode in modes:
        d = df[df["Mode"] == mode]
        n_poke = int((d["RowType"] == "Poke").sum())
        n_miss = int((d["RowType"] == "TouchMiss").sum())
        touch = n_poke + n_miss
        summary_rows.append(
            {
                "Mode": mode,
                "Pokes": n_poke,
                "TouchMiss": n_miss,
                "TouchMissPct": round(n_miss / touch * 100.0, 2) if touch else float("nan"),
                "Rechar": int((d["RowType"] == "Rechar").sum()),
                "Heartbeats": int((d["RowType"] == "Heartbeat").sum()),
            }
        )
    summary = pd.DataFrame(summary_rows)
    if not summary.empty:
        ax.bar(summary["Mode"], summary["TouchMissPct"], color="crimson", alpha=0.75)
        for i, v in enumerate(summary["TouchMissPct"]):
            if pd.notna(v):
                ax.text(i, v, f"{v:.1f}%", ha="center", va="bottom", fontsize=9)
    ax.set_title("unresolved touch wakes")
    ax.set_ylabel("% of touch wakes")
    ax.grid(alpha=0.25)

    fig.suptitle("5. Light sleep vs full wake", y=1.0)
    _save(fig, out_dir, "05_sleep_vs_awake.png")
    return summary


# ---------------------------------------------------------------------------
# Interpretation
# ---------------------------------------------------------------------------


def interpret(df: pd.DataFrame, droughts: pd.DataFrame) -> list[str]:
    """Encode the interpretation rules from the plan as explicit findings."""
    out: list[str] = []

    for dev in _devices(df):
        d = df[df["Device"] == dev]
        hb = d[d["RowType"] == "Heartbeat"]
        pokes = d[(d["RowType"] == "Poke") & d["PadLetter"].notna()]
        rechars = d[d["RowType"] == "Rechar"]

        if len(hb) >= 4:
            head, tail = hb.head(max(3, len(hb) // 10)), hb.tail(max(3, len(hb) // 10))
            for p in PADS:
                bench_shift = tail[f"Bench{p}"].mean() - head[f"Bench{p}"].mean()
                idle_shift = tail[f"Idle{p}"].mean() - head[f"Idle{p}"].mean()
                wake_abs = hb[f"WakeAbs{p}"].median()
                if pd.notna(bench_shift) and pd.notna(wake_abs) and wake_abs > 0:
                    if abs(bench_shift) >= wake_abs and abs(idle_shift) < wake_abs / 2:
                        out.append(
                            f"[{dev} {PAD_NAMES[p]}] benchmark moved {bench_shift:+.0f} "
                            f"counts (>= wakeAbs {wake_abs:.0f}) while idle stayed put "
                            f"({idle_shift:+.0f}) - hardware tracking absorbed the "
                            f"occupant or drift."
                        )
                if pd.notna(idle_shift) and pd.notna(wake_abs) and wake_abs > 0:
                    if abs(idle_shift) >= wake_abs and not rechars.empty:
                        out.append(
                            f"[{dev} {PAD_NAMES[p]}] idle jumped {idle_shift:+.0f} counts "
                            f"and {len(rechars)} Rechar row(s) are present - check whether "
                            f"recalibration ran with a mouse nearby (ProxMm/Motion on "
                            f"those rows)."
                        )

        if len(pokes) >= 6:
            for p in PADS:
                dp = pokes[pokes["PadLetter"] == p]
                if len(dp) < 6:
                    continue
                first = dp["PeakOverIdle"].head(len(dp) // 3).median()
                last = dp["PeakOverIdle"].tail(len(dp) // 3).median()
                bench_stable = True
                if len(hb) >= 4:
                    bs = hb[f"Bench{p}"]
                    wa = hb[f"WakeAbs{p}"].median()
                    bench_stable = pd.notna(wa) and (bs.max() - bs.min()) < wa
                if pd.notna(first) and pd.notna(last) and first > 0:
                    drop = (first - last) / first
                    if drop >= 0.3 and bench_stable:
                        out.append(
                            f"[{dev} {PAD_NAMES[p]}] poke delta fell {drop * 100:.0f}% "
                            f"({first:.0f} -> {last:.0f}) with stable baselines - true "
                            f"sensitivity loss (coupling, debris, mechanical)."
                        )

        touch = d[d["RowType"].isin(["Poke", "TouchMiss"])]
        if len(touch) >= 10:
            miss_pct = (touch["RowType"] == "TouchMiss").mean() * 100.0
            if miss_pct >= 20:
                out.append(
                    f"[{dev}] {miss_pct:.0f}% of touch wakes did not resolve to a pad - "
                    f"identification is failing, not just detection."
                )

    # Dead ports: across all devices vs one device only
    pokes = df[(df["RowType"] == "Poke") & df["PadLetter"].notna()]
    if not pokes.empty:
        devices = _devices(df)
        for p in PADS:
            dead = [
                dev
                for dev in devices
                if ((pokes["Device"] == dev) & (pokes["PadLetter"] == p)).sum() == 0
            ]
            if len(dead) == len(devices) and devices:
                out.append(
                    f"[all devices] no pokes registered on {PAD_NAMES[p]} - "
                    f"FED4-wide calibration issue for that port."
                )
            elif len(dead) == 1 and len(devices) > 1:
                out.append(
                    f"[{dead[0]}] no pokes on {PAD_NAMES[p]} while other devices "
                    f"register them - poke module, not FED4-wide."
                )

    modes = df["Mode"].dropna().unique()
    if len(modes) >= 2:
        health = {}
        for mode in modes:
            d = df[df["Mode"] == mode]
            touch = d[d["RowType"].isin(["Poke", "TouchMiss"])]
            health[mode] = (
                (touch["RowType"] == "TouchMiss").mean() * 100.0 if len(touch) else float("nan")
            )
        if "Awake" in health and "LightSleep" in health:
            if pd.notna(health["Awake"]) and pd.notna(health["LightSleep"]):
                if health["LightSleep"] - health["Awake"] >= 15:
                    out.append(
                        f"[modes] TouchMiss is {health['LightSleep']:.0f}% asleep vs "
                        f"{health['Awake']:.0f}% awake - light sleep, benchmark, or latch "
                        f"path, not the pad."
                    )

    if not droughts.empty:
        worst = droughts.sort_values("DroughtMinutes", ascending=False).head(3)
        for _, r in worst.iterrows():
            out.append(
                f"[{r['Device']}] poke drought of {r['DroughtMinutes']:.0f} min from "
                f"{r['DroughtStart']} ({int(r['TouchMissesInGap'])} TouchMiss, "
                f"{int(r['RecharsInGap'])} Rechar inside it)."
            )

    if not out:
        out.append("No interpretation rule fired - baselines and poke deltas look stable.")
    return out


# ---------------------------------------------------------------------------


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Analyse FED4 touch diagnostic logs (*_T.CSV)."
    )
    ap.add_argument("inputs", nargs="+", help="folders to glob for *_T.CSV, or files")
    ap.add_argument("-o", "--out", default="touch_analysis_out", help="output folder")
    ap.add_argument(
        "--drought-minutes",
        type=float,
        default=60.0,
        help="gap between resolved pokes counted as a drought (default 60)",
    )
    args = ap.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    print("Loading touch logs:")
    files = find_files(args.inputs)
    df = load(files)

    print(
        f"\n{len(df)} rows | {df['DeviceUID'].nunique()} device(s) | "
        f"rows by type: {df['RowType'].value_counts().to_dict()}"
    )

    df.to_csv(out_dir / "combined_touch_log.csv", index=False)
    print(f"  -> {out_dir / 'combined_touch_log.csv'}")

    print("\nPanels:")
    panel_baseline_drift(df, out_dir)
    panel_sensitivity(df, out_dir)
    boot_table, poke_table = panel_cross_device(df, out_dir)
    droughts = panel_failure_signature(df, out_dir, args.drought_minutes)
    mode_summary = panel_sleep_vs_awake(df, out_dir)

    if not boot_table.empty:
        boot_table.to_csv(out_dir / "bootchar_table.csv", index=False)
        print(f"  -> {out_dir / 'bootchar_table.csv'}")
        print("\nBootChar characterization (idle / std / wakeAbs by device × port):")
        print(boot_table.to_string(index=False))
    if not poke_table.empty:
        poke_table.to_csv(out_dir / "poke_table.csv", index=False)
        print("\nPoke summary by device × port:")
        print(poke_table.to_string(index=False))
    if not mode_summary.empty:
        print("\nMode summary:")
        print(mode_summary.to_string(index=False))
    if not droughts.empty:
        droughts.to_csv(out_dir / "poke_droughts.csv", index=False)
        print("\nPoke droughts:")
        print(droughts.to_string(index=False))

    print("\n=== Interpretation ===")
    for line in interpret(df, droughts):
        print(f"  - {line}")
    print(
        "\nReview all five panels before changing any threshold - the recalibration\n"
        "policy is meant to come from these numbers, not the other way round."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
