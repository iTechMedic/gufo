# Qwen3.8 Flash-Next benchmarks

AMD Strix Halo `gfx1151`, 128 GB unified memory. Unsloth `UD-Q4_K_XL` target and
shared-Q8_0 MTP sidecar; Gufo uses adaptive MTP. HTTP, greedy, thinking off.
Gufo single-user AR and MTP: October 13, 2026, production Nix builds of
PR #571, every depth: AR d16K/d64K and MTP d64K use `ac51031a`, which adds
the short W8A8 tail path those prompts (2,051–2,058 tokens) take; the other
rows use `0951ea2f`. Other Gufo tables retain October 10's
production Nix review build `25cdb2a1` of PR #518.
Reference results remain unchanged from September 22–23.
llama.cpp uses `b11069` for AR and `6fcaa16f` for MTP.

Positive gain favors Gufo.
[Quality and measurement details](QUALITY.md#benchmark-method) · [Model identities](artifacts/model-identities.json)

Gufo numerical regression checks pass; two focused functional phases still
exceed the timing gate. Historical llama.cpp MTP values are retained as requested:
a separate check of that pin did not match its own AR output, so this review
does not requalify those comparisons. [Review evidence](artifacts/decode-kernels-review.json).

## Single user, autoregressive

Approximately pp2048 / tg128; depth is the cached prefix in tokens.
Context capacities differ between engines; see the measurement details.

<!-- bench:single-ar -->
| Flash-Next Q4 AR<br>Depth (tokens) | Gufo pp (tok/s) | llama.cpp pp (tok/s) | Gain | Gufo tg (tok/s) | llama.cpp tg (tok/s) | Gain |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 1773.82 | 489.59 | +262.3% | 28.22 | 22.20 | +27.1% |
| 4,096 | 1694.96 | 455.24 | +272.3% | 28.20 | 21.21 | +33.0% |
| 8,192 | 1686.00 | 428.85 | +293.1% | 28.20 | 20.40 | +38.2% |
| 12,288 | 1669.01 | 400.21 | +317.0% | 28.18 | 19.64 | +43.5% |
| 16,384 | 1624.22 | 375.89 | +332.1% | 28.19 | 18.95 | +48.8% |
| 32,768 | 1648.69 | 301.31 | +447.2% | 28.05 | 16.54 | +69.6% |
| 65,536 | 1548.05 | 221.94 | +597.5% | 27.83 | 11.62 | +139.5% |
| 131,072 | 1529.63 | 144.78 | +956.5% | 27.24 | 7.98 | +241.4% |
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
| 0 | 1689.43 | 468.76 | +260.4% | 33.29 | 31.73 | +4.9% | 65.21 | 48.12 | +35.5% |
| 4,096 | 1632.12 | 423.79 | +285.1% | 35.60 | 34.48 | +3.2% | 62.42 | 45.71 | +36.6% |
| 8,192 | 1622.93 | 394.98 | +310.9% | 35.11 | 32.20 | +9.0% | 53.04 | 44.31 | +19.7% |
| 12,288 | 1614.15 | 365.91 | +341.1% | 37.61 | 32.32 | +16.4% | 45.56 | 43.53 | +4.7% |
| 16,384 | 1604.17 | 341.98 | +369.1% | 38.18 | 32.07 | +19.1% | 54.59 | 43.66 | +25.0% |
| 32,768 | 1603.98 | 277.61 | +477.8% | 37.02 | 25.41 | +45.7% | 48.55 | 39.28 | +23.6% |
| 65,536 | 1560.11 | 208.04 | +649.9% | 32.60 | 19.65 | +65.9% | 48.18 | 27.75 | +73.6% |
| 131,072 | 1488.23 | 135.42 | +999.0% | 36.10 | 14.63 | +146.8% | 47.77 | 20.96 | +127.9% |
<!-- /bench -->

![Single user, MTP](artifacts/charts/single-mtp.svg)

## Multiple users, autoregressive

Same pp2048 prose prompt as single-user d0, tg128, context 4096 per user.
All sessions prefilled before timed decoding; throughput sums individual rates.
llama.cpp re-evaluates its four-token checkpoint tail.

<!-- bench:multi-ar -->
| Flash-Next Q4 AR<br>Users | Gufo AR (tok/s) | llama.cpp AR (tok/s) | Gain |
| ---: | ---: | ---: | ---: |
| 1 | 28.17 | 22.34 | +26.1% |
| 2 | 50.19 | 37.08 | +35.4% |
| 4 | 84.72 | 54.82 | +54.5% |
| 6 | 105.97 | 65.34 | +62.2% |
| 8 | 122.48 | 68.79 | +78.0% |
<!-- /bench -->

![Multiple users, autoregressive](artifacts/charts/multi-ar.svg)

## Multiple users, MTP

Same pp2048 mixed/repetitive prompts as single-user d0, tg128, context 4096
per user. All sessions prefilled before timed decoding; rates sum individual
request decode rates. C1 cross-checks the single-user table.

<!-- bench:multi-mtp -->
| Flash-Next Q4 MTP<br>Users | Gufo mixed (tok/s) | llama.cpp mixed (tok/s) | Gain | Gufo repetitive (tok/s) | llama.cpp repetitive (tok/s) | Gain |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 33.37 | 31.77 | +5.0% | 64.92 | 46.93 | +38.3% |
| 2 | 57.20 | 42.79 | +33.7% | 93.02 | 52.57 | +76.9% |
| 4 | 83.99 | 48.78 | +72.2% | 146.39 | 48.23 | +203.5% |
| 6 | 105.52 | 52.59 | +100.6% | 166.28 | 48.51 | +242.8% |
| 8 | 121.15 | 61.92 | +95.7% | 184.74 | 58.13 | +217.8% |
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
| Q4 | 14.94 | 117.83 | +688.7% |
<!-- /bench -->

![Loading time](artifacts/charts/loading.svg)

## Memory occupation

C1, context capacity 133121, AR. Peak memory reported by HIP.

<!-- bench:memory -->
| Flash-Next Q4 AR<br>Workload | Gufo GiB | llama.cpp GiB | Gain |
| --- | ---: | ---: | ---: |
| pp2048 + tg128 | 86.18 | 84.84 | -1.6% |
| 16K prefix, pp4096 + tg128 | 87.34 | 85.31 | -2.3% |
<!-- /bench -->

![Memory occupation](artifacts/charts/memory.svg)
