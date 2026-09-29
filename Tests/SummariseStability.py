#!/usr/bin/env python3
"""Summarise raw characterisation CSVs; never normalize or alter measurements.

Run after every mode, with the Release or ASan measurement directory as argument.
Exit success means complete data was summarised, not that all DSP cases passed.
"""
import csv
import math
from pathlib import Path
import sys

NAMES = ["SVF: LP", "SVF: HP", "SVF: BP", "SVF: BR", "Sallen-Key: LP",
         "Sallen-Key: HP", "Transistor ladder: LP", "Diode ladder: LP"]
RATES = [44100, 48000, 88200, 96000, 192000]


def read(root, name):
    with (root / (name + ".csv")).open() as stream:
        return list(csv.DictReader(stream))


def finite(row):
    return all(row[key] == "1" for key in ("finite", "coeff_finite", "state_finite"))


def trials(rows):
    result = []
    for row in rows:
        if row["phase"] == "excite":
            result.append({"initial": row, "high": [], "recovery": []})
        else:
            result[-1][row["phase"]].append(row)
    return result


def number(value):
    return f"{value:.6g}"


def summarise(root):
    root = root.resolve()
    build = (Path(__file__).resolve().parents[1] / "build").resolve()
    if build not in (root, *root.parents):
        raise ValueError("Measurements and summary must stay beneath build")
    data = {name: read(root, name) for name in
            ("matrix", "boundaries", "oscillation", "extended", "out_of_spec")}
    # Incomplete runs must not silently become a successful report.
    for name, expected in (("matrix", 7040), ("boundaries", 240), ("out_of_spec", 1152)):
        if len(data[name]) != expected:
            raise ValueError(f"Incomplete {name}: {len(data[name])}, expected {expected}")
    for name, expected in (("blocks", 640), ("automation_events", 1280),
                           ("smoothing", 20), ("settling", 50), ("internal_cutoffs", 10)):
        if len(read(root, name)) != expected:
            raise ValueError(f"Incomplete {name}")
    ordinary = trials(data["oscillation"])
    long = trials(data["extended"])
    if len(ordinary) != 336 or len(long) != 126:
        raise ValueError("Incomplete oscillation trials")
    for trial in ordinary + long:
        if finite(trial["initial"]) and all(finite(r) for r in trial["high"] + trial["recovery"]):
            minimum = 8 if trial in ordinary else 12
            recovery = 6 if trial in ordinary else 10
            if len(trial["high"]) < minimum or len(trial["recovery"]) != recovery:
                raise ValueError("Incomplete finite dwell/recovery")
    supported = sum((data[name] for name in ("matrix", "boundaries", "oscillation", "extended")), [])
    lines = ["# Generated measurement summary", "", "## Collection counts", "",
             "| Dataset | Rows | Non-finite output/coefficient/state rows | Peak |",
             "| --- | ---: | ---: | ---: |"]
    for name, rows in data.items():
        lines.append(f"| {name} | {len(rows)} | {sum(not finite(r) for r in rows)} | "
                     f"{number(max(float(r['peak']) for r in rows))} |")
    lines += ["", "## Supported rates", "",
              "8s RMS uses resonance 1.1, drive 1 at pitches 12 / 69 / 135. Recovery is the worst final one-second window across ordinary and extended trials. Peak covers all supported measurement datasets; finite includes sampled coefficients and histories. Boundary means all three dynamic boundary schedules.", "",
              "| Hz | Configuration | Finite | Peak | 8s RMS: low / mid / high | Final recovery peak / RMS | Boundary |",
              "| ---: | --- | --- | ---: | --- | --- | --- |"]
    for rate in RATES:
        for configuration, name in enumerate(NAMES):
            def matches(row):
                return int(row["rate"]) == rate and int(row["configuration"]) == configuration
            rows = [r for r in supported if matches(r)]
            ts = [t for t in ordinary + long if matches(t["initial"])]
            osc = [next(float(t["high"][-1]["rms"]) for t in ordinary
                        if matches(t["initial"]) and int(t["initial"]["cutoff"]) == pitch
                        and float(t["initial"]["resonance"]) > 1.09)
                   for pitch in (12, 69, 135)]
            final = [t["recovery"][-1] for t in ts if t["recovery"]]
            recovery_peak = max(float(r["peak"]) for r in final)
            recovery_rms = max(float(r["rms"]) for r in final)
            boundary = all(finite(r) for r in data["boundaries"] if matches(r))
            lines.append(f"| {rate} | {name} | {'yes' if all(map(finite, rows)) else 'NO'} | "
                         f"{number(max(float(r['peak']) for r in rows))} | "
                         f"{' / '.join(map(number, osc))} | {number(recovery_peak)} / {number(recovery_rms)} | "
                         f"{'finite' if boundary else 'NON-FINITE'} |")
    lines += ["", "## Long buildup follow-ups", "",
              "| Hz | Configuration | Resonance | RMS at 8 / 30 / 60 seconds | Last-window peak | Recovery final RMS |",
              "| ---: | --- | ---: | --- | ---: | ---: |"]
    for t in long:
        if len(t["high"]) == 60:
            r = t["initial"]
            lines.append(f"| {r['rate']} | {NAMES[int(r['configuration'])]} | {float(r['resonance']):.3g} | "
                         f"{' / '.join(number(float(t['high'][i-1]['rms'])) for i in (8,30,60))} | "
                         f"{number(float(t['high'][-1]['peak']))} | {number(float(t['recovery'][-1]['rms']))} |")
    lines += ["", "## Recovery", "",
              "Relative attenuation uses final high-resonance RMS as reference, only trials above 1e-8 RMS (a reporting floor, not a pass threshold). Time to -60 dB is the end of the first entire one-second recovery window at or below that relative level; it is not an exact decay time.", "",
              "| Configuration | Slowest measured -60 dB window end (s) | Least final attenuation (dB) | Final absolute RMS (max) |",
              "| --- | ---: | ---: | ---: |"]
    for c, name in enumerate(NAMES):
        ts = [t for t in ordinary + long if int(t["initial"]["configuration"]) == c
              and t["high"] and float(t["high"][-1]["rms"]) > 1e-8 and t["recovery"]]
        ends, attenuations = [], []
        for t in ts:
            reference = float(t["high"][-1]["rms"])
            ends.append(next((i+1 for i,r in enumerate(t["recovery"]) if float(r["rms"]) <= reference*.001), math.inf))
            ratio = float(t["recovery"][-1]["rms"]) / reference
            attenuations.append(20*math.log10(ratio) if ratio else -math.inf)
        lines.append(f"| {name} | {max(ends)} | {max(attenuations):.3f} | "
                     f"{number(max(float(t['recovery'][-1]['rms']) for t in ts))} |")
    lines += ["", "## Out-of-spec failures", "",
              "| Hz | Configuration | Failed / total cases | First bad sample (range) | Largest finite value recorded |",
              "| ---: | --- | ---: | --- | ---: |"]
    for rate in (32000, 176400, 384000):
        for c, name in enumerate(NAMES):
            rows = [r for r in data["out_of_spec"] if int(r["rate"]) == rate and int(r["configuration"]) == c]
            bad = [r for r in rows if not finite(r)]
            if bad:
                positions = [int(r["first_bad_frame"]) for r in bad]
                lines.append(f"| {rate} | {name} | {len(bad)} / {len(rows)} | {min(positions)}–{max(positions)} | "
                             f"{number(max(float(r['peak']) for r in bad))} |")
    lines += ["", f"Subnormal output samples: {sum(int(r['subnormal_output']) for r in supported)}. "
              f"Block-end subnormal history observations: {sum(int(r['state_subnormal_observations']) for r in supported)}.", ""]
    (root / "summary.md").write_text("\n".join(lines))
    print("Summary written; inspect findings, not just process exit status.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 Tests/SummariseStability.py build/<results>/<release-or-asan>")
    summarise(Path(sys.argv[1]))
