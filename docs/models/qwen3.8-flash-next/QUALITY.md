# Qwen3.8 Flash-Next quality

**PR #571's wave-owned prefill weights retain byte-exact logits:**
99,328,000 F32 values after a 34,159-token code/prose prompt and 400 fixed
continuation steps, batch 4096, capacity 65536, match the original PR control
`f06f32b9`. October 13, 2026 (Europe/Paris), kernel snapshot `a772fae6` atop
`33a91366`, with the same pinned toolchain. Independent FP64 routed checks,
unsaturated activations, odd outputs and guarded partial tiles pass. Fused SSM
convolution, output, state and replay remain exact, including 2049-token tails.
Deferred residual, F16 norm, packed Q8 bytes and independent host FP32 inject
checks pass unchanged. All 16 measured AR/MTP HTTP completions at d0/d32K
retain exact outputs and token/cache/draft counts, with no per-request
prefill/decode timing flag. The first unusually slow mixed-MTP control is
excluded as effect-size evidence.
These are regression checks. The older functional timing failures below remain
unresolved, and upstream model/conversion accuracy is not requalified.

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

**Prompt lookup on v0.15.0 (October 15, 2026): qualified for the affected
gate under the screen-resolution timing policy below; the unchanged
`compare.py` screen alone stays inconclusive.** Candidate `9129bb2`: v0.15.0
with the prompt-lookup commits and a shared disk-checkpoint spacing fix. The
matched baseline is v0.15.0 plus the same fix (`7e129b5`), not unchanged stock.
Pinned Nix production builds (flake.lock `faabad82`): baseline `548d9390`,
candidate `ec9a81a9`, Intel SSD on native PCIe 3.0 x4.

- **Spacing fix:** stock v0.15.0 fails `check_disk_spacing`. A published disk
  checkpoint whose RAM copy is still resident stopped counting for the 2,048
  token step, so every growing turn was written. `PrefixIndex::DurablePrefixTokens`
  restores the step; `PublishedResidentCheckpointKeepsDiskStep` fails before and
  passes after. `checks.pr` passes (59/59). Every later gate run stores
  `[2447, 6278]` and writes the learned branch boundary.
- **Correctness:** unchanged from the October 14 record below; every gate and
  follow-up run passes outputs, exact restores, cache/prefill work and spacing.
- **Cold start (10 alternating pairs, 5%/3 ms paired t, unchanged):** all 13
  primaries pass; first-request prefill U95 +9.92 ms against 13.35.
- **Affected gate:** state-edges, tool-agent, long-context and cache.
  - The `compare.py` screen on the original pair: 969 passed, 7 inconclusive
    (`queue_ms` ×3, `cache_snapshot_ms` ×4). It cannot resolve a flag: its
    range only widens, and stock fails it against itself on 6 rows.
  - `tests/functional/resolve.py` keeps that verdict and resolves flags from
    matched repetitions: 7 follow-up pairs (BA AB BA AB BA AB BA) plus the 10
    cold pairs for shared state-edges rows. Margins stay 5% / 3 ms.
  - Resolved rows: `legacy-tool-cancel` request 1 `queue_ms` (median bound
    +0.43 ms, 8 pairs); state-edges `thinking_tool_one_token` `queue_ms` (+0.08,
    18); first-request `max_inter_token_ms` (+3.53 against 8.84, 18);
    concurrent `agent_sampled_peers` `queue_ms` by admission rank (leader +0.43,
    follower +0.99, 8). No unexplained candidate stall; no regression.
  - `cache_snapshot_ms` is reported, not gated: it is launch-to-join wall time
    of an asynchronous capture. Diagnostic builds showed the large
    `agent_sampled_peers` value is a follower left parked about 57 ms after its
    4.4 ms capture had finished, while the scheduler ran the leader's decode
    step. This delays that request's second token. It occurs on stock too and
    is assessed through each request's latency phases.
  - The first-request token gap contains a one-time ~90 ms start of the first
    capture worker in each process (all four diagnostic runs, both builds).
    When that cost lands before the first token, TTFT rises and the gap shrinks
    by the same amount; the second token's time does not change.
  - Fourteen screen-passed rows (disk-spacing turns 4/6, text-cancel 0,
    legacy-tool-cancel 6) are bimodal in the follow-ups on both builds. A
    once-per-process ~80 ms capture cost lands either on disk-spacing or on
    `long_cancel_resume` (slow disk-spacing runs: stock 4/9, candidate 5/8; slow
    captures per run 7.44 against 7.5). Within each state the builds differ by
    less than the margin. The follow-up regression check has no power at
    Bonferroni level with 8 pairs, so these rows rest on the screen.
- **Realistic rewrite (unchanged, October 14):** 20–27% less wall time, paired
  median 25.2%, on one 230-line greedy fixture. This does not measure general
  coding performance, sampled decoding or concurrency.
- Evidence: [prompt-lookup-v015-qualification.json](artifacts/prompt-lookup-v015-qualification.json),
  [prompt-lookup-v015.json](artifacts/prompt-lookup-v015.json).

**Earlier record, October 14, 2026 (2c4039e, before the spacing fix):**
correctness passed and the rewrite took about 25% less wall time; cold start
was inconclusive and the gate was blocked by stock `check_disk_spacing`.
Candidate `2c4039e`: stock v0.15.0 `89d6185` merged with the prompt-lookup
commits; its `src/` and `tests/` diff equals the reviewed lookup diff. Prompt
lookup is the only Flash-Next customization; the legacy disk-cache patch
below is retired. Pinned Nix production builds (flake.lock `faabad82`): stock
`8bd1567a`, candidate `2b2960c1`.
- **Original cache failure:** stock v0.15.0 serves Flash-Next through the
  component cache and passes the stalled-write reproduction below unchanged
  (first cache `fsync` held 15 s, `cache` suite, 2 of 2 runs). Both
  `*-tool-cancel-disk` cases pass, all 5 checkpoints are restored from disk,
  and there are no `staging_capacity` skips.
- **Correctness (candidate):** `checks.pr` passes (59/59 hosted CPU tests).
  The Flash-Next CPU tests pass, and so do the GPU session, continuation-adapter
  and snapshot tests. They cover exact greedy lookup against AR, sampled replay,
  diverged and mid-generation restores, and batched sampled copies. 20 focused
  HTTP processes give identical outputs, token counts and cache state for both
  builds.
- **Focused timing (unchanged 5%/3 ms paired protocol, 10 pairs):** warm
  (fully cached, non-copying) decode passes. Cold start is inconclusive, with
  no regression demonstrated. Candidate-only cold flags match the host
  drive's slow read state, which also hits stock processes. This history
  makes no lookup copies, so it measures overhead, not acceleration.
- **Realistic rewrite (4 matched pairs, AB BA BA AB, greedy):** a 230-line
  Python file with one function rename, returned complete (1,949 tokens). The
  output is byte-identical between builds and exactly the expected edit.
  Stock takes 28.6–29.3 s per request and the candidate 21.4–22.9 s: 20–27%
  less wall time in every pair (paired median 25.2%, decode rate 1.36×).
  Pairs 1–3 are clean. Pair 4 is reported but not interpretable: its candidate
  process hit the drive's slow read state, which added about 1 s of prefill.
  A counter-only diagnostic build, not part of the candidate, counted 1,147
  copies proposed and 1,142 accepted, 58.6% of generated tokens.
- **Scope:** one rewrite fixture. This does not measure general coding
  performance, sampled decoding or concurrency.
- **Separate upstream issue:** on stock v0.15.0 and on the candidate alike,
  the ungated `cache` suite fails `check_disk_spacing`. Disk checkpoints are
  stored within 2,048 tokens of an already published one (stock `[2447,
  3029, 3611, 6278]`), while every cache case and restore passes. It is
  independent of prompt lookup and of the retired patch.
- Evidence: [prompt-lookup-v015.json](artifacts/prompt-lookup-v015.json).

The records below are historical (v0.13.0 and main `2599ad6`). Their gains
are relative to v0.13.0 and do not carry over to v0.15.0.

**Prompt lookup is not qualified: correctness passes, including on current
main; timing qualification is inconclusive.** Final source `dcd7a8c`:
implementation `a0aee02`, batch-cost accounting fix `3c7922f`, anchor-lookup
fix `8297750` + `0821c8a`, tests `9e7784f`/`dcd7a8c`; versus v0.13.0
`fcf8bf7`.
- **Pinned Nix production build** (flake.lock nixpkgs `2fcb964d`):
  final `dcd7a8c` `5922399c` (`checks.pr` passes); timed binary `0821c8a`
  `221ef3b5`, whose production sources are identical; base `c4c55240`.
- **Rebased onto main `2599ad6`** (October 13): the same 14 commits apply
  without conflicts. The `src/` and `tests/` diff is byte-identical to
  `fcf8bf7..dcd7a8c`. Main adds managed-allocation fallback `248d902`
  (Flash-Next rollback, batch-logit and scratch allocations), CI and README;
  none touches the lookup files.
- **Integrated build:** Nix production `70315c24`. `checks.pr` passes (57/57
  hosted CPU tests, including main's `heap_placement_test`). The session test
  on the pinned `nix develop` toolchain (`7108cda1`) passes. Its log matches
  `dcd7a8c`'s line for line, including every lookup counter; only the
  snapshot-budget lines, which follow free host RAM, differ. In the cache
  runs below, both main-based builds report the v0.13.0 device footprint
  (88,338 MiB), so the fallback did not change placement. The throughput
  rows below are relative to v0.13.0 and were not remeasured on main.

Correctness passes:
- **Tests, both toolchains:** on the pinned `nix develop` toolchain, the 15
  Flash-Next CTest cases and the snapshot test pass (`0821c8a`), and the
  session test passes (`dcd7a8c`, 61 exact checks); the container session
  test also passes.
- **Session test coverage:** greedy MTP-with-copies equals AR including
  stop-terminated replies. Sampled replay is exact after restore and from a
  mid-copy snapshot, and so are cancellation and rollback. Batched C2–C8
  logits, RNG, acceptance and lookup counters match isolated execution.
- **Nix `checks.pr`:** passes, 56/56 hosted CPU tests.
- **HTTP output:** greedy output is byte-identical to the baseline within
  each toolchain.

**Quoted-argument regression: fixed.**
- **Original defect** (controlled clocks): `quoted_call_argument_*` decode
  was +8.6% to +14.4% from one extra cycle (8 vs 7 rounds; 12 vs 11).
- **Cause (Nix traces):** after a fully accepted copy cycle, the next first
  MTP draft was wrong. Lookup was only tried after a draft, so the wrong
  draft hid a 16-token context match for that cycle.
- **First attempt (`8297750`, superseded):** lookup also at the anchor. It
  fixed the quoted cases but regressed `agent_read_after_edit` (+22%) and
  four `state-edges` schema/tool cases (+13%), because 12–15-token template
  matches displaced first drafts that had never been rejected.
- **Final rule (`0821c8a`):** at the anchor a copy competes only when the
  first copy's survival estimate exceeds the controller's first-draft
  acceptance, so only after first drafts were rejected.
- **Result** (Nix production binaries, GPU level `auto`, three alternating
  pairs, same rounds/drafts as base everywhere checked):

| Case | Base | Fix |
| --- | ---: | ---: |
| `quoted_call_argument_complete` | 857–858 ms | 857–859 ms |
| `quoted_call_argument_stop_streamFalse` | 531–532 ms | 530–532 ms |
| `quoted_call_argument_stop_streamTrue` | 519–524 ms | 519–520 ms |
| `agent_read_after_edit` | 339–340 ms | 340 ms |

- **Regression tests:**
  - **Pass with the fix:** the unit anchor-gate checks, and a session
    scenario that quotes an arbitrary value then diverges. In it, greedy
    output equals AR and copies are accepted, and seeded sampled runs replay
    exactly and must copy from the anchor after rejected first drafts.
  - **Fail without the fix:** the same tests with only the anchor calls
    removed.
- **Review:** CodeRabbit raised no findings on these commits.

Batch-cost accounting (CodeRabbit, valid):
- **Problem:** copy cycles, and the catch-up cycle after them, were learned
  as the cost of the chosen MTP depth. The candidate learned depth 5 at
  93.9 ms against ~62 ms ordinary cycles at C2, and ~147 ms at C4.
- **Fix `3c7922f`:** skips the cost sample for both cycles while keeping
  cycle counts, exploration and cohort tracking; a regression test is in the
  hosted suite.
- **Measured impact:** none in these workloads; copies fill the chains
  whatever depth is chosen. See [EXPERIMENTS](EXPERIMENTS.md).

Confirmed improvements (Nix, fix `0821c8a`):
- **Rewrite:** +40.3% / +51.3% greedy and +48.9% / +48.0% sampled
  (shallow / 32K).
- **Small edit:** +28.0% / +34.2% greedy and +28.3% / +33.5% sampled.
- **Prose:** within ±1.1%.
- **Repetitive C2–C8 (earlier container runs):** +24–79%.

Timing (inconclusive):
- No repeatable candidate-only flag in the focused pairs. Remaining flags
  are snapshot/startup/TTFT ones, also seen on base controls.
- **The two decode flags are machine state** (fix run 6 of round 37, from
  existing data): `zero_argument_tool_after_limit` +6.9% and
  `thinking_tool_one_token` +7.0%. Both builds use identical rounds, drafts
  and outputs, and `thinking_tool_one_token` has no draft round, so lookup
  cannot run. 33 retained runs are classified by `execution_mode_probe`
  prefill: 18 fast (319–351 ms) and 15 slow (364–432 ms). In the fast state
  base takes 227.1–232.3 / 37.0–37.8 ms and the candidates 226.7–230.8 /
  37.1–37.6 ms. In the slow state both builds spread widely: base
  244.2–272.4 / 38.6–45.3 ms, candidates 242.6–284.7 / 41.2–48.4 ms. The
  flagged run is slow-state, and so was its recorded baseline.
- **Missing condition:** a stable GPU clock state. With level `auto`, DVFS
  switches speed states within runs on both builds; level `high` reduced but
  did not remove them. No clock change was authorized, so the final gate was
  not run, as further uncontrolled runs cannot qualify per-request timing.

Host-disk cache failures (a shared cache defect, exposed by write stalls):
- **Trigger, environmental and on every build:** the store's own write and
  `fsync` occasionally stalls 4–18.5 s. Normal writes of 120–290 MB take
  75–300 ms. Across ~80 retained server logs, every failed restore follows
  such a stall. An 8 GiB direct-write probe ran at 5.45 GB/s; nothing was
  deleted.
- **Defect:** admission ignored queued and in-flight saves. During a stall,
  checkpoints within one step of the write in progress filled the 1 GiB
  staging budget, about 125–145 MB each. The writer later discarded them as
  `min_step`, but meanwhile the next conversation's only checkpoints were
  skipped for `staging_capacity`, so its restart restore missed. This is the
  same failure as v0.13.0 `legacy-tool-cancel-disk` and the earlier
  `thinking-tool-cancel-disk` failures on main, base and candidate.
- **Reproduced deterministically** with the `cache_disk_faults` fsync gate
  holding the first cache write 15 s, `cache` suite, main `2599ad6`
  (`3fadd157`): both `*-tool-cancel-disk` restores fail, with 36
  `staging_capacity` skips.
- **Fix (separate branch `codex/disk-cache-pending-step`, `7e72453`):**
  queued saves are step bases. Admission drops a checkpoint that repeats or
  extends one by less than a step, and applies the stored-entry rule before
  staging. With the same held write (`9afcd67`, `a723b461`; production
  sources identical to `7e72453`), every case passes with no staging skip.
  Request-path enqueue stays ≤0.38 ms (main ≤0.32 ms). A new store test
  fails on the unfixed store and passes with the fix. Prompt lookup does not
  depend on this fix.
- **Retired for Flash-Next (October 14):** on v0.15.0, Flash-Next no longer
  uses this store, and stock v0.15.0 passes the same reproduction. The fix
  stays on its branch and is not part of the v0.15.0 candidate.

Evidence: [decode-prompt-lookup.json](artifacts/decode-prompt-lookup.json),
`final_v013` (`upstream_integration_round_42_43`,
`quoted_argument_fix_round_33_41`, `final_validation_round_30_32`,
`batch_cost_accounting_fix`,
`nix_gpu_correctness`, `environment_build_round_23_25`).

| Check | Result |
| --- | --- |
| Wave-owned prefill weights, October 13 | Routed WMMA, projection/convolution, MoE epilogue and HC mix checks pass with unchanged tolerances. Routed inputs end at their allocation boundary; independently reset output tails retain their guards. Fused SSM output/state/replay and full fixed-history logits are exact. HTTP C1 pp2048/tg128, two AR captures with two repetitions and two mixed/repetitive MTP captures: 16 exact measured completions, including actual drafts. All request phases pass the timing gate; the anomalous first mixed-MTP control is excluded. |
| Prompt lookup on main, October 13 | Rebased onto main `2599ad6` with an identical `src/`/`tests/` diff. Nix production build `70315c24` and `checks.pr` (57/57) pass. The pinned-toolchain session test matches the `dcd7a8c` log, including lookup counters. Timing remains inconclusive without stable GPU clocks. Host-disk restore failures are a shared staging-admission defect, reproduced on main and fixed separately (`7e72453`). [Evidence](artifacts/decode-prompt-lookup.json). |
| Prompt lookup, October 12 | Final candidate `a0aee02` (binary `14a7cc1f`) versus v0.13.0 `fcf8bf7` (`d781bceb`), matched ROCm 10 container build. All 15 Flash-Next CTest cases (including the copy-evidence checks) pass. The session test's 61 exact checks match the earlier build's log, and snapshot round trips are exact. Greedy MTP with copies equals AR, including stop-terminated replies. Seeded sampled replay is exact after restoring onto a diverged session and from a mid-copy snapshot. Batched sessions copy (8 sampled copies) and match isolated tokens, RNG, acceptance and snapshot bytes. HTTP greedy output is byte-identical to v0.13.0 in 10/10 ABBA cells and 18/18 cold three-way per-pass pairs (with an unchanged-base control). Same-build seeded replay is exact in 20/20 seed cells per build. C1–C8 mixed/repetitive MTP completions match the same-toolchain AR reference. `state-edges`, `tool-agent`, `long-context` and `cache` pass with the cache on RAM and on host disk. C2/C4 mixed batches make no copies and match base cycle costs. Final source `9e7784f`: Nix production build and `checks.pr` pass, and Flash-Next CTest, session and snapshot tests pass on the pinned `nix develop` toolchain. The quoted-argument regression is fixed (`0821c8a`); timing qualification is inconclusive and host-disk cases fail on the baseline (above). [Evidence](artifacts/decode-prompt-lookup.json). |
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

The October 13 single-user refresh uses production Nix builds of PR #571:
`0951ea2f` (binary SHA-256 prefix `61d0583912d9e2c8`), and `ac51031a`
(`d6cc00786ffc8786`) for AR d16K/d64K and MTP d64K, whose prompts end a few
tokens past a 128-token tile and take its W8A8 tail path. Both run through
`tools/bench/model-bench.py run --target gufo --table single-ar,single-mtp`:
HTTP C1, capacity 133760, greedy, thinking off, pp2048/tg128 at
every depth from 0 to 131072, one warmed sample per point, platform profile
`performance`. Token and cache counts pass the driver's calibration gates.
The single-user kernels behind it retain byte-identical logits for 718 fixed
continuation rows after a 35,559-token prompt at batch 2048 and 4096 against
the previous PR head ([experiments](EXPERIMENTS.md)). Multi-user, loading and memory
tables and all reference values keep their earlier provenance.

The October 10 sweep follows:

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
