# Qwen3.8 Flash-Next quality

**PR #518 retains main's full logits at 64K/128K:** 512 fixed code/prose token
positions, each checked at verification widths 1–8; all 1,017,118,720 logit
comparisons are byte-exact (KL/TV 0, top-1 agreement 100%). Prefill and 56
partial rollback/replay checks are exact too. October 10, 2026, candidate
`25cdb2a1` versus main `a41e88da`.
Unsloth UD-Q4_K_XL target, shared Q8_0 MTP;
[identities](artifacts/model-identities.json) · [review evidence](artifacts/decode-kernels-review.json).
These regression and consistency checks do not qualify original unquantized-model
accuracy or GGUF conversion.

**The functional timing gate is not clean.** All 182 AR/MTP request cases pass
correctness in both paired captures, with exact output and prompt/cache/prefill
counts. After an additional unchanged-main control, AR `legacy-tool-cancel`
single-token decode remains 44.63 ms versus 38.88–38.92 ms, and MTP
`disk-spacing` 289-token continuation prefill remains 557.21 ms versus
516.68–526.89 ms. Initial candidate samples were 159.66/951.43 ms.
Other queue/snapshot timing flags remain inconclusive; the initial 58.25 s
queue stall did not recur. The gate remains 5% and 3 ms per request/phase.
Concurrent setup and maximum-token-gap flags are also retained in the evidence.

**Prompt lookup: correctness passes; the timing gate stays inconclusive.**
Correctness: every functional case passes on both builds with unchanged
output and cache/prefill counts, and greedy HTTP output is byte-identical to
main in every compared pair. Fixed timing failure: the earlier branch slowed
`tool-agent` `agent_read_after_edit` 338 → 395–412 ms in five runs, because its
first ≥12-token match ended MTP drafting. That 13-token match landed in the
previous `edit` call, the copy was rejected on its second token, and the
24-token reply needed five cycles instead of four. Copies now replace pending
MTP drafts only when more of them are expected to be accepted, so this reply
uses main's four fully accepted cycles: 342/341 ms on main, 341/366 ms on the
retained build, and 408 ms on the earlier branch. The 366 ms run's prefill
was also 14–20% slower on every request, which lookup does not touch.
Inconclusive: in the full four-suite gate, an unchanged-main control raised 13
flags (snapshot-copy jitter) and the earlier branch 22. The focused tool-agent
reruns flag machine-wide prefill and snapshot timing and restart load time
(25–46 s on main, 38–48 s on the candidate); the full gate was not rerun on
the retained build. On this
host the disk cache stalled for up to 18.5 s per write, so
`thinking-tool-cancel-disk`, `legacy-tool-cancel-disk` and `disk-spacing-disk`
failed intermittently on both main and the candidate; with the cache on
RAM-backed storage all 11 cache cases passed twice on each build.

| Check | Result |
| --- | --- |
| Prompt lookup, October 12 | Branch `codex/flash-next-prompt-lookup` on main `f37fb012`, matched ROCm 10 container build; rechecked after copies were gated on expected acceptance. All 15 Flash-Next CTest cases (including the copy-evidence checks), the full session test with no exact check failing, and the snapshot test pass. Greedy MTP with copies equals AR, including stop-terminated replies; seeded sampled replay is exact after restoring onto a diverged session and from a mid-copy snapshot; batched sessions copy (8 sampled copies) and match isolated tokens, RNG, acceptance and snapshot bytes. HTTP greedy output is byte-identical to main in every per-pass pair: 20/20 and 10/10 ABBA cells, and 18/18 cold 2-session pairs before and after the gate (the later run also matches an unchanged-main control). `state-edges`, `tool-agent`, `long-context` and `cache` pass; `tool-agent` was rerun on the retained build. The timing gate stays inconclusive (above). [Evidence](artifacts/decode-prompt-lookup.json). |
| Decode-kernel review, October 10 | All 13 operator tests and native snapshot/session/deep-execution checks pass. Independent MTP formulas: worst relative RMS 0.0007655 (limit 0.002), 41 reported comparisons over eight text/image states. Sampled replay, cancellation, C2/4/6/8 and RAM/disk restoration pass. [Evidence](artifacts/decode-kernels-review.json). |
| Focused HTTP regression, October 10 | `state-edges`, `tool-agent`, `long-context`, `cache`, Flash AR/MTP only. Two main/PR pairs plus unchanged-main controls; no output or expected cache/prefill-work changes. Timing failures remain visible above and in the [evidence](artifacts/decode-kernels-review.json). |
| Concurrency, October 10 | All 63 tg128 requests match fresh AR completions: 21 AR, 21 mixed MTP and 21 repetitive MTP at C1/2/4/6/8. Prepared prefixes are reused; mixed/repetitive MTP propose 882/2170 drafts and accept 517/1959. [Evidence](artifacts/decode-kernels-review.json). |
| Real-image regression checks | 1024×1024 encoding passes. Nine main/candidate HTTP image, retry and continuation responses have identical outputs and token/cache counts. AR/MTP C4 image batching, sampled replay, cancellation and RAM/disk restoration pass. The disk fixture reprocesses only its seven-token assistant suffix; identical RAM retries require zero prefill. [Evidence](artifacts/prefill-long-context.json). |
| Prefill kernels | Final build: 7,946,240 logits after 32,640 prompt tokens remain byte-identical to main (KL 0). The earlier deep-context gate reaches 259,938 tokens with exact logits. Independent GDN/projection/selector checks, ragged chunks and graph replay pass. [Evidence](artifacts/prefill-long-context.json). |
| Mapped input and MTP catch-up | Exact Q8 projections, full/tail predictor stages and candidates; sampled output, acceptance, RAM/serialized replay and C2/4/6/8 state/RNG checks pass. Default sampling and thinking-off penalties are covered at d0/32K/128K. [Evidence](artifacts/prefill-long-context.json). |
| HTTP depth sweep, October 10 | All 24 pp2048/tg128 AR and mixed/repetitive MTP completions through d128K match current main's output hashes and prompt/cache/draft counts. [Evidence](artifacts/decode-kernels-review.json). |
| Unused final-layer outputs | Full-vocabulary logits match unpruned execution byte-for-byte through 133,120 tokens, also with mixed code/Unicode text and image input. Mixed batching and sampled RAM/serialized snapshot replay preserve logits and RNG state. [Evidence](artifacts/prefill-final-rows.json). |
| Bounded selector scratch | After 133,824 prompt tokens, 15,892,480 logits are byte-identical to the previous build. Ragged chunks retain exact masks and the independent FP64 score check. [Evidence](artifacts/prefill-deep-context.json). |
| Unused normalization removal | 15,892,480 logits are byte-identical to main; ragged residual-only fusion matches normalized fusion and the separate epilogue/combine. [Evidence](artifacts/prefill-normalization.json). |
| MTP versus scalar CPU formulas, eight text/image states | Fusion/attention relative RMS <0.0008 (limit 0.002); full-width normalization, split projections, recursive carry and full Q8 head checked |
| Batched MTP/AR, C2/C4/C6/C8 | Logits, tokens, acceptance, RNG and every 1–8-token rollback prefix match isolated execution |
| Sampling | 25 AR/MTP configurations, including top-p zero; FP64 target filtering/CDF, p/q acceptance, residual correction, seeded replay and short token budgets pass |
| Greedy penalties | GPU selection matches CPU at every 1–7-row verification width, including FP32-sensitive ties; native tool/JSON constraints remain active. C2/4/6/8 outputs match the previous implementation; image/tool cancellation, RAM/disk restore and OpenAI SDK checks pass. [Evidence](artifacts/penalty-verification.json). |
| Prefill and cache | Full logits match across tested chunk boundaries, short tails and restored state through 4096 tokens |
| Seeded MTP cache rebuilding | Two seeds × 200 tokens replay exactly after different prefill splits, cache bypass and replacement. K/V-only prefill and compact catch-up preserve full-head candidates across 1/8/9/32/33-row chunks. [Evidence](artifacts/mtp-cache-replay.json). |
| Scalar versus bulk prefill, 2176 tokens | Same top-1; logit RMSE 0.18, not bit-identical |
| Serving | Cancellation, three-turn continuation, reasoning/tool history, concurrent image/text isolation and disk restart pass |
| Prompt checkpoints | In-pass and borrowed checkpoints restore exactly in AR/MTP, including rewinds, branches, resets and destruction; 14 functional cache jobs pass correctness. [Details](PROMPT-CHECKPOINTS.md) |
| Sparse attention | Independent FP64 operator error ≤2.83e-7 (limit 1e-6). At 32K/128K, 256 fixed-token code/prose rows: mean KL 5.82e-4 and 256/256 top-1 agreement with an FP64-attention diagnostic. [Evidence](artifacts/attention-tiles-review.json). |

The attention diagnostic retains the quantized weights and other native
operators. Regrouping FP32 sums can change long-context text across builds;
matched-prefill AR/MTP logits and sampled snapshot replay are exact at
32K/128K. Session tests also cover image/text restoration and C2/4/6/8.

Sampled MTP can consume different RNG draws from AR. Seeded replay requires
the same build, request budget, capacity and sampling configuration; live cost
timings only steer greedy decoding. Draft sampling uses the full Q8 head's
top 64 logits; upstream draft-sampler equivalence is not claimed.
Prompt-lookup copies are point-mass proposals: accepted with the target
probability, with the residual excluding the copied token. They change which
draws a seeded request consumes relative to earlier builds; replay within one
build stays exact and does not depend on batching.
MTP cache projections now keep the same arithmetic across prompt splits.
Older Flash-Next disk checkpoints are rejected and rebuilt once.

## Vision

**One Gufo encoder comparison fails:** relative L2 **6.47%** versus official
Transformers BF16, above the **5%** limit, on a 1024×1024 synthetic texture.
Both use the same converted GGUF weights. Against FP32, Gufo and Transformers
BF16 differ by **7.83% / 8.10%**, respectively. This is numerical drift, not an
image-answer score; **llama.cpp was not tested**. Optimizations retain native
embedding bytes but do not resolve this gap. [Evidence](artifacts/vision-parity.json).

## Reproduce

Tests live in [`tests/models/qwen38_flash_next`](../../../tests/models/qwen38_flash_next).
Use `--batch-only`, `--sampling-only`, `--prefill-only` or `--cache-only` on the
session test; the snapshot test covers persistent image/text state.

`--execution-only 133120` runs the focused deep AR/MTP logit and snapshot-replay
check without the other session suites.

For independent MTP checks:

```sh
nix develop -c cmake --build --preset gpu-test \
  --target qwen38_flash_next_model_tests qwen38_flash_next_gpu_probe
nix develop -c build/gpu-test/tests/models/qwen38_flash_next/qwen38_flash_next_gpu_probe \
  --model "$MODEL" --mtp-model "$MTP" --batch 2048 --mtp-audit
```

The [vLLM](https://github.com/vllm-project/vllm/blob/751f6807d9cb3de50c27a5f27188c4fb04fe0e2b/vllm/models/qwen4_exp/amd/mtp.py)
and [SGLang](https://github.com/sgl-project/sglang/blob/993d1fccbaafe3e79d91567d2fc1d665cc94fa50/python/sglang/srt/models/qwen4_exp_mtp.py)
formulas supply independent predictor checks; pinned Transformers ignores MTP
weights. [Vision reproduction](../qwen3.8-27b/QUALITY.md#vision).

## Benchmark method

Gufo refreshed October 10, 2026 using production Nix build `25cdb2a1`,
PR #518 rebased onto main `a41e88da` with all 16 patches unchanged.
Matched main controls use the same pinned toolchain, model and ordered workload
histories. Binary and source hashes are retained in
[model identities](artifacts/model-identities.json).
All llama.cpp data retain September 22–23 provenance, unchanged at the user's
request; no reference benchmark refresh was run.
The largest displayed PP decrease versus main is 3.77%, at 64K MTP; no
single-user PP row drops more than 5% versus main or the previous published
table. A focused C1 PP-only retry at that frontier measured AR
1467.30 → 1497.60 tok/s (+2.1%) and MTP 1451.39 → 1463.63 tok/s (+0.8%).
Both builds used the saved prefix/messages and exact original token/cache counts,
with one generated token. Fresh prefix/continuation pairs omit earlier cached
branches, so these controls remain separate from the original ordered-sweep
samples rather than replacing or averaging them. [Evidence](artifacts/decode-kernels-review.json).
One warmed sample per point, greedy, thinking off, penalties disabled.
Single-user uses pp2048/tg128; MTP pp is the maximum across mixed/repetitive
workloads. Gufo capacity is 133760; reference capacity is 35456 through 32K,
68224 at 64K and 133760 at 128K. C1/2/4/6/8 use the same d0 prompts; every
session prefills before measured tg128, with at most four prompt-tail tokens
reevaluated. Rates sum individual decode rates.
Depth calibration depends on the ordered sweep. Paired speed controls use
the same depth list. Deep AR/MTP HTTP cache frontiers differ by one token;
exact replay is checked separately with identical prefill boundaries.
AR reference is llama.cpp b11069; historical MTP data use pinned `6fcaa16f`.
A separate prerequisite check on October 10 used that same MTP binary in AR
and MTP modes at pp2048/tg128, C1/capacity 4096. Mixed-prose fresh and prepared
MTP outputs matched each other but differed from its own AR output; both
generated 128 tokens and MTP proposed 152 drafts. Repetitive MTP was not
attempted after this failure. The retained historical speed values are not
new equivalence-qualified measurements. [Evidence](artifacts/decode-kernels-review.json).
Loading: cold files verified with `POSIX_FADV_DONTNEED` and `mincore` on every
target/sidecar file, C1/MTP/capacity 262144; runtime libraries may remain warm.
Memory: C1/AR/capacity 133121,
peak global HIP allocation including idle memory. Full commands, counts and
identities remain in [artifacts](artifacts/bench.json) and the
[benchmark workflow](../../../.agents/skills/benchmark-model/SKILL.md).
