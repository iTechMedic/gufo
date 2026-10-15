#!/usr/bin/env python3
"""Resolve timing rows that the compare.py screen left inconclusive.

compare.py remains the gate's screen and its verdict on the original pair is
reported unchanged. Its range rule cannot resolve a flag: once max(PR) -
min(main) exceeds the margin, more observations never pass it. This resolves
flagged rows from independent matched repetitions of the same histories:

- Same builds, matched histories, coverage and 5% / 3 ms margins as
  compare.py. Any quality or coverage change fails. Nothing is discarded.
- Location: the distribution-free one-sided 95% upper bound of the median
  paired difference (PR - main), an order statistic of the differences, must
  be within max(noise floor, 5% of the main median). It needs at least five
  pairs. Intermittent events on either build bias a mean, so they are
  assessed as tails instead.
- Tails: an observation above max(1.25x, +10 ms) of the main median is a
  stall. Every PR stall is listed. One blocks its row unless main or control
  shows a stall in the same row, and a significantly higher PR stall rate
  (one-sided Fisher exact test, Bonferroni over rows with stalls) fails.
- Rows the screen passed keep that result. Their repetitions are still
  checked for unexplained PR stalls and for regressions (lower bound beyond the
  margin, Bonferroni over all repeated rows).
- cache_snapshot_ms is launch-to-join wall time of an asynchronous capture,
  not request latency. It is reported with its stalls and does not gate; a
  capture that blocks a request shows in that request's latency phases.
- queue_ms of concurrent requests with identical cache work, whose admission
  order is a client race, is compared by admission rank instead of by body.
"""

import argparse
import json
import math
from pathlib import Path
import statistics
import sys

from metrics import NOISE_MS, SLOWDOWN, comparison_status

ALPHA = 0.05
MIN_PAIRS = 5
DIAGNOSTIC = ("cache_snapshot_ms",)


def order_index(n, alpha):
    """Largest k with P(Binomial(n, 1/2) <= k) <= alpha, or None."""
    total, k = 0.0, None
    for i in range(n + 1):
        total += math.comb(n, i) / 2 ** n
        if total > alpha:
            break
        k = i
    return k


def fisher_greater(a, n_a, b, n_b):
    """One-sided p that the second group's event rate exceeds the first's."""
    total = a + b
    def pmf(x):
        return math.comb(n_b, x) * math.comb(n_a, total - x) / math.comb(n_a + n_b, total)
    return sum(pmf(x) for x in range(b, min(total, n_b) + 1))


def admission_ranks(directory):
    """Per concurrent group of equal cache work: rank -> (body, queue_ms)."""
    ranks = {}
    for path in sorted(Path(directory).glob("*.requests.json")):
        rows = json.loads(path.read_text())["requests"]
        groups = []
        for row in rows:
            label = row.get("case", row.get("index"))
            if not groups or groups[-1][0] != label:
                groups.append((label, []))
            groups[-1][1].append(row)
        seen = {}
        for label, group in groups:
            occurrence = seen.get(label, 0)
            seen[label] = occurrence + 1
            work = {tuple(row.get("metrics", {}).get(f) for f in
                          ("prompt_tokens", "prefill_tokens", "cached_tokens")) for row in group}
            order = [row.get("metrics", {}).get("resident_requests_at_admission") for row in group]
            if len(group) < 2 or len(work) != 1 or None in order or len(set(order)) != len(order):
                continue
            ordered = sorted(group, key=lambda row: row["metrics"]["resident_requests_at_admission"])
            ranks[(path.name, str(label), occurrence)] = [
                (row["request_sha256"], row["metrics"]["queue_ms"]) for row in ordered]
    return ranks


def resolve(comparisons, controls, runs, flagged, min_pairs=MIN_PAIRS):
    """comparisons/controls: compare_runs() results, original first; runs:
    their (main, PR) directories in the same order; flagged: screen rows
    (request_key + metric tuples) to resolve."""
    rows, issues = {}, []
    for index, comparison in enumerate([*comparisons, *controls]):
        control = index >= len(comparisons)
        if comparison.get("slowdown_tolerance") != SLOWDOWN \
                or comparison.get("noise_ms") != NOISE_MS:
            raise ValueError("timing margins must remain 5% and 3 ms")
        issues.extend(comparison["quality_or_coverage_changes"])
        for item in comparison["measurements"]:
            key = (*item["request_key"], item["metric"])
            if index and key not in rows:
                raise ValueError("follow-up contains a scenario absent from the initial run")
            row = rows.setdefault(key, {
                "case": item["case"], "request_key": item["request_key"], "metric": item["metric"],
                "history_sha256": item.get("history_sha256"), "noise_floor": item["noise_floor"],
                "samples": [], "controls": []})
            if row["history_sha256"] != item.get("history_sha256") \
                    or row["noise_floor"] != item["noise_floor"]:
                raise ValueError(f"unmatched history or token count: {item['case']}")
            row["controls" if control else "samples"].append(
                {"comparison": index, "baseline": item["baseline"], "candidate": item["candidate"]})
    # Concurrent queue waits by admission rank; their body rows are superseded.
    superseded = set()
    ranked = [(admission_ranks(a), admission_ranks(b)) for a, b in runs]
    for group in ranked[0][0]:
        bodies = {body for body, _ in ranked[0][0][group]}
        for rank in range(len(ranked[0][0][group])):
            key = (group[0], f"{group[1]}:admission-{rank + 1}", group[2], "queue_ms")
            row = {"case": f"{group[0]}:{group[1]}:{group[2]}:admission-{rank + 1}",
                   "request_key": list(key[:3]), "metric": "queue_ms", "noise_floor": NOISE_MS,
                   "samples": [], "controls": [], "admission_rank": rank + 1}
            for index, (main, pr) in enumerate(ranked):
                if group in main and group in pr and len(main[group]) == len(pr[group]):
                    row["samples"].append({"comparison": index, "baseline": main[group][rank][1],
                                           "candidate": pr[group][rank][1]})
            rows[key] = row
        bodies_rows = [key for key in rows if key[0] == group[0] and key[1] in bodies
                       and key[3] == "queue_ms"]
        superseded.update(bodies_rows)
        if any(key in flagged for key in bodies_rows):
            flagged = {*flagged, *((group[0], f"{group[1]}:admission-{rank + 1}", group[2],
                                    "queue_ms") for rank in range(len(ranked[0][0][group])))}
    repeated = [row for row in rows.values() if len(row["samples"]) >= min_pairs]
    stalled = []
    for key, row in rows.items():
        main = [s["baseline"] for s in row["samples"]]
        median = statistics.median(main)
        row["margin"] = max(row["noise_floor"], SLOWDOWN * median)
        threshold = row["stall_threshold"] = max(1.25 * median, median + 10)
        stock = main + [v for s in row["controls"] for v in (s["baseline"], s["candidate"])]
        candidate = [s["candidate"] for s in row["samples"]]
        row["candidate_stalls"] = [s["comparison"] for s in row["samples"] if s["candidate"] > threshold]
        row["stock_stalls"] = sum(v > threshold for v in stock)
        row["differences"] = d = [s["candidate"] - s["baseline"] for s in row["samples"]]
        row["n"] = len(d)
        if row["candidate_stalls"]:
            stalled.append(row)
        row["_counts"] = (row["stock_stalls"], len(stock), len(row["candidate_stalls"]), len(candidate))
    for row in rows.values():
        d, n = row["differences"], row["n"]
        k = order_index(n, ALPHA)
        kr = order_index(n, ALPHA / max(1, len(repeated)))
        row["median_upper95"] = sorted(d, reverse=True)[k] if k is not None else None
        row["lower_bonferroni"] = sorted(d)[kr] if kr is not None and n >= min_pairs else None
        a, n_a, b, n_b = row.pop("_counts")
        row["stall_p"] = fisher_greater(a, n_a, b, n_b) if b else 1.0
        key = (*row["request_key"], row["metric"])
        unexplained = bool(row["candidate_stalls"]) and row["stock_stalls"] == 0
        regression = (row["lower_bonferroni"] is not None and row["lower_bonferroni"] > row["margin"]) \
            or row["stall_p"] < ALPHA / max(1, len(stalled))
        if row["metric"] in DIAGNOSTIC:
            row.update(status="diagnostic", reason="asynchronous launch-to-join time; not gated")
        elif key in superseded:
            row.update(status="superseded", reason="concurrent queue wait compared by admission rank")
        elif regression:
            row.update(status="failed", reason="slowdown or excess stalls beyond the margin")
        elif unexplained:
            row.update(status="inconclusive", reason="candidate stall without a stock stall")
        elif key not in flagged:
            row.update(status="passed", reason="screen passed; repetitions show no regression")
        elif row["median_upper95"] is None or n < min_pairs:
            row.update(status="inconclusive", reason=f"fewer than {min_pairs} matched pairs")
        elif row["median_upper95"] <= row["margin"]:
            row.update(status="passed", reason="median bound within margin")
        else:
            row.update(status="inconclusive", reason="median bound exceeds margin")
        row["flagged"] = key in flagged
    gating = [row for row in rows.values() if row["status"] not in ("diagnostic", "superseded")]
    return {"status": comparison_status(gating, issues), "policy": "screen-resolution",
            "slowdown_tolerance": SLOWDOWN, "noise_ms": NOISE_MS, "min_pairs": min_pairs,
            "flagged": sorted(map(list, flagged)), "quality_or_coverage_changes": issues,
            "measurements": list(rows.values())}


def main():
    from compare import evidence, matched
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pair", nargs=2, type=Path, action="append", required=True,
                        metavar=("MAIN", "PR"), help="Original pair first, then repetitions")
    parser.add_argument("--control", nargs=2, type=Path, action="append", default=[],
                        metavar=("MAIN", "MAIN_REPEAT"), help="The original unchanged-main control")
    parser.add_argument("--flag", nargs=4, action="append", default=[],
                        metavar=("FILE", "BODY", "OCCURRENCE", "METRIC"),
                        help="Extra row to resolve, beyond the screen's flags")
    parser.add_argument("--output", type=Path, required=True, help="New JSON evidence file")
    args = parser.parse_args()
    try:
        screen = evidence(args.pair[:1], args.control)
        flagged = {(*row["request_key"], row["metric"]) for row in screen["measurements"]
                   if row["status"] != "passed"}
        flagged |= {(f, b, int(o), m) for f, b, o, m in args.flag}
        comparisons, controls, sources = matched(args.pair, args.control)
        result = resolve(comparisons, controls, args.pair, flagged)
        result["screen"] = {"status": screen["status"], "flags": sorted(map(list, flagged))}
        result["evidence"] = sources
    except (ValueError, KeyError, OSError) as error:
        result = {"status": "failed", "error": str(error), "measurements": []}
    try:
        with args.output.open("x") as output:
            output.write(json.dumps(result, indent=2) + "\n")
    except OSError as error:
        print(str(error), file=sys.stderr)
        return 1
    counts = {}
    for row in result["measurements"]:
        counts[row["status"]] = counts.get(row["status"], 0) + 1
    print(f"{result['status']}: {counts}; {args.output}")
    return {"passed": 0, "failed": 1, "inconclusive": 2}[result["status"]]


if __name__ == "__main__":
    sys.exit(main())
