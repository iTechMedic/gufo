# Qwen3.8 Flash-Next benchmarks

AMD Strix Halo `gfx1151`, 128 GB unified memory. Unsloth `UD-Q4_K_XL` target and
shared-Q8_0 MTP sidecar; Gufo uses adaptive MTP. HTTP, greedy, thinking off.
Gufo single-user and concurrency tables measured October 16, 2026, with
PR #622 (compacted sparse-attention selections) rebased on `22e36769`; loading
and memory retain the same-day `fdc21849` captures.
Reference results remain unchanged from September 22–23.
llama.cpp uses `b11069` for AR and `6fcaa16f` for MTP.

Positive gain favors Gufo.
[Quality and measurement details](QUALITY.md#benchmark-method) · [Model identities](artifacts/model-identities.json)

Gufo numerical regression checks pass; individual functional timing limits
remain documented in [QUALITY.md](QUALITY.md). Historical llama.cpp MTP values are retained as requested:
a separate check of that pin did not match its own AR output, so this review
does not requalify those comparisons. [Review evidence](artifacts/decode-kernels-review.json).

PR #622 speeds up decode at depth: at 128K, tg improves 0.7% AR, 1.9% mixed
MTP and 2.1% repetitive MTP over the `fdc21849` rows, with identical completions
and draft counts. Prefill is unchanged by the PR; pp differences are run-level
variation. Mixed MTP C2 alternates between two draft-controller trajectories
(about 54 and 60 tok/s, identical text) on main and the PR alike.

## Single user, autoregressive

Approximately pp2048 / tg128; depth is the cached prefix in tokens.
Context capacities differ between engines; see the measurement details.

<!-- bench:single-ar -->
| Flash-Next Q4 AR<br>Depth (tokens) | Gufo pp (tok/s) | llama.cpp pp (tok/s) | Gain | Gufo tg (tok/s) | llama.cpp tg (tok/s) | Gain |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 1768.66 | 489.59 | +261.3% | 28.31 | 22.20 | +27.5% |
| 4,096 | 1697.96 | 455.24 | +273.0% | 28.27 | 21.21 | +33.3% |
| 8,192 | 1691.35 | 428.85 | +294.4% | 28.28 | 20.40 | +38.6% |
| 12,288 | 1672.13 | 400.21 | +317.8% | 28.25 | 19.64 | +43.8% |
| 16,384 | 1672.92 | 375.89 | +345.1% | 28.22 | 18.95 | +48.9% |
| 32,768 | 1643.71 | 301.31 | +445.5% | 28.10 | 16.54 | +69.9% |
| 65,536 | 1577.99 | 221.94 | +611.0% | 27.90 | 11.62 | +140.1% |
| 131,072 | 1517.44 | 144.78 | +948.1% | 27.49 | 7.98 | +244.5% |
<!-- /bench -->

![Single user, autoregressive](artifacts/charts/single-ar.svg)

## Single user, MTP

pp is the highest measured rate per engine and depth across mixed/repetitive
text, including Gufo predictor catch-up.

Prompt lookup is not reflected in these Nix-build rows. See
[Prompt lookup on v0.15.0](#prompt-lookup-on-v0150-file-rewrite) for the current
same-release rewrite comparison and
[Prompt lookup, ROCm container build](#prompt-lookup-rocm-container-build-historical-v0130)
for the historical v0.13.0 measurements.

<!-- bench:single-mtp -->
| Flash-Next Q4 MTP<br>Depth (tokens) | Gufo pp (tok/s) | llama.cpp pp (tok/s) | Gain pp | Gufo tg mixed (tok/s) | llama.cpp tg mixed (tok/s) | Gain mixed | Gufo tg repetitive (tok/s) | llama.cpp tg repetitive (tok/s) | Gain repetitive |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 1722.29 | 468.76 | +267.4% | 35.71 | 31.73 | +12.5% | 73.44 | 48.12 | +52.6% |
| 4,096 | 1655.05 | 423.79 | +290.5% | 37.62 | 34.48 | +9.1% | 70.47 | 45.71 | +54.2% |
| 8,192 | 1641.90 | 394.98 | +315.7% | 36.85 | 32.20 | +14.4% | 58.12 | 44.31 | +31.2% |
| 12,288 | 1624.20 | 365.91 | +343.9% | 39.18 | 32.32 | +21.2% | 48.55 | 43.53 | +11.5% |
| 16,384 | 1611.73 | 341.98 | +371.3% | 40.53 | 32.07 | +26.4% | 59.96 | 43.66 | +37.3% |
| 32,768 | 1594.19 | 277.61 | +474.3% | 39.72 | 25.41 | +56.3% | 52.59 | 39.28 | +33.9% |
| 65,536 | 1531.42 | 208.04 | +636.1% | 38.99 | 19.65 | +98.4% | 52.68 | 27.75 | +89.8% |
| 131,072 | 1477.08 | 135.42 | +990.7% | 39.36 | 14.63 | +169.0% | 53.77 | 20.96 | +156.5% |
<!-- /bench -->

![Single user, MTP](artifacts/charts/single-mtp.svg)

## Multiple users, autoregressive

Same pp2048 prose prompt as single-user d0, tg128, context 4096 per user.
All sessions prefilled before timed decoding; throughput sums individual rates.
llama.cpp re-evaluates its four-token checkpoint tail.

<!-- bench:multi-ar -->
| Flash-Next Q4 AR<br>Users | Gufo AR (tok/s) | llama.cpp AR (tok/s) | Gain |
| ---: | ---: | ---: | ---: |
| 1 | 28.27 | 22.34 | +26.5% |
| 2 | 50.68 | 37.08 | +36.7% |
| 4 | 85.65 | 54.82 | +56.2% |
| 6 | 107.53 | 65.34 | +64.6% |
| 8 | 125.08 | 68.79 | +81.8% |
<!-- /bench -->

![Multiple users, autoregressive](artifacts/charts/multi-ar.svg)

## Multiple users, MTP

Same pp2048 mixed/repetitive prompts as single-user d0, tg128, context 4096
per user. All sessions prefilled before timed decoding; rates sum individual
request decode rates. C1 cross-checks the single-user table.

<!-- bench:multi-mtp -->
| Flash-Next Q4 MTP<br>Users | Gufo mixed (tok/s) | llama.cpp mixed (tok/s) | Gain | Gufo repetitive (tok/s) | llama.cpp repetitive (tok/s) | Gain |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 35.66 | 31.77 | +12.2% | 73.17 | 46.93 | +55.9% |
| 2 | 54.35 | 42.79 | +27.0% | 97.55 | 52.57 | +85.6% |
| 4 | 86.42 | 48.78 | +77.2% | 157.76 | 48.23 | +227.1% |
| 6 | 108.58 | 52.59 | +106.5% | 176.70 | 48.51 | +264.3% |
| 8 | 125.47 | 61.92 | +102.6% | 194.29 | 58.13 | +234.2% |
<!-- /bench -->

![Multiple users, MTP](artifacts/charts/multi-mtp.svg)

## Prompt lookup on v0.15.0, file rewrite

October 14, 2026, pinned Nix production builds (flake.lock `faabad82`):
stock v0.15.0 `89d6185` (`8bd1567a`) against candidate `2c4039e`
(`2b2960c1`).
- **Workload:** the greedy `rewrite` request of the historical runs below. A
  230-line Python file with one function rename, returned complete, 1,949
  generated tokens.
- **Method:** `--sessions 1 --context 65536 --think off`, MTP. Each sample is
  a fresh process, with one identical warm-up request and no disk cache. The
  times cover the complete rewrite request, including 1,994 uncached prefill
  tokens (about 1.3 s on both builds).
- **Correctness:** the output is byte-identical between builds and exactly
  the expected edit.

| Pair | Order | Stock wall / decode (s) | Candidate wall / decode (s) | Wall change | Decode rate |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | AB | 29.01 / 27.69 | 21.82 / 20.50 | −24.8% | 1.35× |
| 2 | BA | 29.34 / 28.03 | 21.36 / 20.05 | −27.2% | 1.40× |
| 3 | BA | 28.70 / 27.39 | 21.35 / 20.03 | −25.6% | 1.37× |
| 4¹ | AB | 28.64 / 27.32 | 22.86 / 20.58 | −20.2% | 1.33× |

Paired median: −25.2% wall time, 1.36× decode rate. Per request, stock
proposes 1,772 MTP drafts and accepts 1,639 in 310 rounds. The candidate
proposes 1,710 and accepts 1,704 in 245 rounds; 1,142 of those accepted are
prompt-lookup copies. This is a focused 4-pair experiment on one fixture,
not general coding performance or a full qualification; cold-start timing
remains inconclusive ([QUALITY](QUALITY.md)).

¹ The candidate process hit the host drive's slow read state, which added
about 1 s of prefill. The pair is reported but not interpretable; pairs 1–3
are clean.

[Evidence](artifacts/prompt-lookup-v015.json).

## Prompt lookup, ROCm container build (historical, v0.13.0)

Not comparable with the Nix tables above or with the v0.15.0 rewrite: matched
ROCm 10 container builds, GCC host compiler, October 12, 2026.
- **Base:** v0.13.0 `fcf8bf7`, binary `d781bceb`.
- **Candidate:** `a0aee02`, binary `14a7cc1f`.

Same workloads, `tools/bench/model-bench.py`. Each cell is two passes in
alternating order (pass 1 / pass 2). Greedy outputs are byte-identical
between builds, and every multi-user completion matches the same-toolchain
AR reference. Nix cells for the candidate are pending.

Single user, MTP, pp2048/tg128. The gain is from pass 2.

| Depth (tokens) | Text | Base pp (tok/s) | Candidate pp (tok/s) | Base tg (tok/s) | Candidate tg (tok/s) | Gain tg |
| ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 0 | mixed | 1,552.10 / 1,570.17 | 1,573.79 / 1,565.18 | 30.54 / 30.51 | 30.57 / 30.53 | +0.1% |
| 0 | repetitive | 884.34¹ / 1,566.59 | 1,561.11 / 1,567.07 | 59.21¹ / 64.55 | 100.25 / 100.11 | +55.1% |
| 32,768 | mixed | 1,437.77 / 1,430.71 | 1,438.90 / 1,441.10 | 33.16¹ / 38.67 | 38.63 / 38.39 | −0.7% |
| 32,768 | repetitive | 971.67¹ / 1,450.06 | 1,381.75 / 1,446.12 | 41.49¹ / 44.49 | 98.13 / 98.01 | +120.3% |

¹ This base pass ran in a slow machine state:
- Its first 2K prefill took 3,065 ms against 2,038 ms, and lookup does not
  run during prefill.
- Its 32K mixed run had identical drafts.

Multiple users, MTP, tg128, context 4096 per user. Gains are per pass.

| Users | Base mixed (tok/s) | Candidate mixed (tok/s) | Gain | Base repetitive (tok/s) | Candidate repetitive (tok/s) | Gain |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 27.83 / 30.44 | 30.43 / 30.40 | +9.3% / −0.1% | 60.75 / 63.60 | 98.98 / 101.22 | +62.9% / +59.2% |
| 2 | 52.90 / 50.77 | 50.67 / 45.11 | −4.2% / −11.1% | 78.79 / 81.95 | 140.76 / 144.46 | +78.7% / +76.3% |
| 4 | 75.92 / 70.36 | 84.13 / 69.68 | +10.8% / −1.0% | 136.94 / 126.80 | 183.37 / 200.18 | +33.9% / +57.9% |
| 6 | 97.00 / 97.17 | 97.11 / 97.28 | +0.1% / +0.1% | 132.58 / 148.81 | 199.90 / 215.00 | +50.8% / +44.5% |
| 8 | 111.67 / 112.34 | 111.16 / 111.43 | −0.5% / −0.8% | 177.16 / 163.64 | 219.29 / 231.05 | +23.8% / +41.2% |

Mixed C2 and C4 swings are machine state, not lookup:
- Traces show no copies in either workload.
- Three alternating untraced C2 pairs: base 53.03 / 51.07 / 44.41, candidate
  51.07 / 51.05 / 47.79 tok/s.
- At identical drafts the builds matched (51.05 vs 51.07).
- Slow runs on either build have every cycle about 12% slower.

Pinned Nix production builds (candidate `5ba35a3b`, base `c4c55240`;
disposable container, not the build behind the Nix tables above) were checked
over HTTP. Order was base, candidate, candidate, base, with greedy output
byte-identical. Nix tables still need refreshing.

| Workload | Greedy, shallow / 32K | Sampled, shallow / 32K |
| --- | ---: | ---: |
| Rewrite | +40.1% / +48.3% | +49.0% / +50.5% |
| Small edit | +28.7% / +39.3% | +28.8% / +24.7% |
| Mixed prose | 0.0% / +0.1% | 0.0% / −0.1% |
| Fresh code | +0.7% / +0.4% | +2.4% / −2.0% |

Batch-cost accounting fix (`3c7922f`, container binary `3647d37e`) versus
`a0aee02`, three alternating pairs (mean tok/s):

| Case | `a0aee02` | Fix | Change |
| --- | ---: | ---: | ---: |
| Repetitive C2 | 151.16 | 149.06 | −1.4%¹ |
| Repetitive C4 | 192.62 | 200.28 | +4.0%¹ |
| Mixed C2 | 49.24 | 48.92 | −0.7% |
| Mixed C4 | 78.11 | 80.56 | +3.1% |

¹ Repetitive draft counts are identical in every run, and the samples agree
within 0.2% apart from one slow sample per build. Mixed draft counts vary
by run on both builds. Single-user rewrite and prose are within 0.2%.

Final source `dcd7a8c` (timed Nix binary `221ef3b5` of `0821c8a`, base
`c4c55240`):

| Workload | Greedy, shallow / 32K | Sampled, shallow / 32K |
| --- | ---: | ---: |
| Rewrite | +40.3% / +51.3% | +48.9% / +48.0% |
| Small edit | +28.0% / +34.2% | +28.3% / +33.5% |
| Prose | −0.0% / +0.9% | −0.1% / −1.1% |

The quoted-argument tool calls match the baseline again. Concurrency rows
above predate the anchor fix, which does not change batched copying of
repetitive text (no anchor copies there).

Token gaps come from the HTTP A/B runs, not this driver:
- **ABBA runs:** the largest gap was 166 ms on the candidate and 158 ms on
  base (both 32K prose). On rewrites and small edits it was 111 ms against
  126 ms.
- **Cold 2-session run** (262K context, disk cache): 194 ms on the
  candidate, against 197 ms on base and 283 ms on an unchanged-base control.

These gains are relative to v0.13.0 and were not remeasured on main
`2599ad6`. The rebase leaves the lookup sources unchanged. Main's only
decode-path change is allocation placement, which kept the same device
footprint on this host; see [QUALITY](QUALITY.md).

[Evidence](artifacts/decode-prompt-lookup.json) (`final_v013`).

## Loading time

C1, context capacity 262144, MTP. Cold target/sidecar files to HTTP readiness.

<!-- bench:loading -->
| Flash-Next Q4<br>Target | Gufo ready (s) | llama.cpp ready (s) | Gain |
| --- | ---: | ---: | ---: |
| Q4 | 15.27 | 117.83 | +671.6% |
<!-- /bench -->

![Loading time](artifacts/charts/loading.svg)

## Memory occupation

C1, context capacity 133121, AR. Peak memory reported by HIP.

<!-- bench:memory -->
| Flash-Next Q4 AR<br>Workload | Gufo GiB | llama.cpp GiB | Gain |
| --- | ---: | ---: | ---: |
| pp2048 + tg128 | 88.75 | 84.84 | -4.4% |
| 16K prefix, pp4096 + tg128 | 90.73 | 85.31 | -6.0% |
<!-- /bench -->

![Memory occupation](artifacts/charts/memory.svg)
