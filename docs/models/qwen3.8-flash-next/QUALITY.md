# Qwen3.8 Flash-Next quality

**Issue #632, October 17:** compared with merged main `8497711e`, indexer plan
preparation moves the first hipBLASLt module load before readiness without
running trial GEMMs. The independent F64 projection check and prepared/lazy
F16/BF16 replay pass for 45 shapes, including ragged widths. Native AR/MTP
snapshot, borrowed-state, sampled/serialized replay and device-exhaustion
checks remain exact. Cache backing tests cover real long-term pins, allocations
larger than 1 GiB, GPU/CPU bytes, failed registration, lifetime and low-memlock
fallback; a separate process also passes with the io_uring syscalls denied.

The new explicit `cache-stalls` suite uses a public equivalent of the report's
24 ordered requests through 56K tokens. The supplied archive omits the inner
prompt script, so this is not an exact reporter replay. Final candidate
correctness and all 345 compared phase measurements pass. The standard d0
pp2048/tg128 control retains all 12 outputs and token/cache/draft counts, with no
timing flags. First-prompt PP drops from 257–260 to 145–146 ms; measured TG is
28.27 → 28.29 tok/s AR, 35.79 → 35.76 mixed MTP, and 116.95 → 116.88 repetitive
MTP. A real Pi conversation passes tools, editing and code generation with
thinking, 10 HTTP requests, 9 successful tool calls and executed tests.

Cache correctness also passes, including cancellation, image continuation and
disk restart. Timing is **not globally qualified**: after two final/main pairs
and an unchanged-main control, 843 of 860 measures pass and 17 remain
inconclusive. The queue outlier overlaps main's 9.30–13.70 ms range. One image
resume takes 181 versus 167 ms of decode in one pair and matches in the other;
individual thinking/token-gap and disk-snapshot variation remains recorded.
No timing margin was widened.

An earlier main run had a 2.83-second token gap during page migration and
reclaim. A matched control holding another 4 GiB retains all 24 outputs and
phases within the timing gate, but neither arm repeats that large gap. Long-term
pins prevent migration of the cache pages; these observations do not prove the
entire original pause came from their migration. Container syscall restrictions
can disable this extra protection; see [cache pinning](../../KV-CACHE.md).
The cold-prompt setup fix also works when pinning is unavailable.

**PR #613 review, October 17:** prompt lookup remains local to each Flash-Next
MTP session and now works in batched decoding. Greedy requests can bypass the
neural head; sampled requests retain a neural proposal first and require a
24-token match. Fewer than half the copied proposals being accepted disables
short resumption. Matches count only tokens generated since the request's first
speculative step, so shared chat wrappers cannot qualify a new answer.
Sampled copy sources stop at the request's starting context; greedy requests
can also copy repetition within their current answer. This avoids displacing
useful neural drafts with superficially similar parts of a new answer.
Fully reused prompts reset this state even without `Sync`. Snapshots retain
the request boundary and resume cursor; the payload compatibility version is 18.
Version-17 disk entries are not reused after upgrading, so their first request
needs a fresh prefill. New version-18 entries support normal restart reuse.

The prefill follow-up replaces the per-gather allocating n-gram map with
reusable scratch prepared before serving. It preserves token-to-row hashing, first
occurrences, I/O order, dequantization and copied values. The independent
n-gram tests pass, including 32,768-row, three-row and 32,769-row gathers
through the same storage, with reference BF16 values and untouched guards.
The repeated 8K MTP prefill flag is fixed: the same 2,042 input tokens after
8,189 cached tokens take 1,220 ms rather than 1,330 ms, matching main's
1,219 ms. The focused history retains every completion hash and cache/token
count. This fixes prefill allocation stalls; speculative output still arrives
in bursts, whose largest gaps are reported separately from total decode time.
At 12K repetition, the largest gap is 73.86 ms versus main's 63.58 ms, while
the same 128 output tokens take 1.218 seconds versus 2.634 seconds.
The full-width copy path is retained for throughput; that accepted 10 ms
burst-gap increase is not reported as a timing pass.
The final AR, mixed MTP and repetitive MTP sweeps cover all eight depths
from 0 to 128K. All 24 completion hashes and prompt/cache counts match main
`3389a585`; prefill, total decode and first-token phases pass the same gate.
The 12K burst-gap flag remains in the full sweep (64.02 → 70.19 ms).

The final C1/C2/C4/C6/C8 sweep retains all 63 AR-equivalent completions and
expected cache work. C8 ordinary/repetitive MTP reaches 126.87/245.11 versus
main's 124.94/196.31 tok/s. Five preparation flags compare different admission
positions: corresponding server admission, cache and batch roles agree across
all 60 C>1 preparations, including conservative log-rounding bounds.
The original request-index flags are retained.

C8 AR has a separate maximum-gap limitation: the initial pair measures about
72 → 103 ms, and a reverse-order repeat about 73 → 104 ms, with total
throughput within 0.3%. A matched pair recording individual HTTP events gives
72.74–72.87 ms on main and 72.36–72.52 ms on the candidate; its largest gap is
on the first generated step. Output and cache work remain identical.
This does not establish the cause of the earlier spikes: their attribution
remains inconclusive, and they are not reported as a timing pass.

Native tests with the same C8 model configuration compare isolated execution
with C2/C4/C6/C8 cohorts: tokens, full logits, RNG, accepted/proposed counts and
snapshot bytes agree exactly. They mix greedy and sampled requests,
vary budgets and peer order, and require accepted copies in both modes
(118 greedy and 43 sampled copies in the final run). Restoring into a used
session and rewinding into a copy also replay exactly. Direct, RAM and serialized
200-token continuations pass, including learned policies and both real sampling
presets. Regressions cover the first step, rejected-copy resumption, a shared
wrapper followed by a short answer, reindexing, boundary restoration, exclusion
of a sampled answer's own repetitions and eligibility on the next request.

The independent MTP oracle passes normalization, attention, recursive carry,
full Q8 head, ragged catch-up and independent batches. Point-mass frequency and
filter-boundary tests retain the target sampling distribution; target arithmetic
and the neural sampler are unchanged throughout the follow-up. Native EOS and
ignore-EOS checks pass. The final HTTP lookup test reaches every physical width
C2/C4/C6/C8 and accepts 3,172 copied tokens, including an all-greedy C8 mix of
copies and new text whose outputs match isolated execution. With the vision
projector loaded, greedy and default-sampled image requests cancel after 40 and
16 accepted copies respectively; both replay the exact uninterrupted response
from RAM with zero prefill.

A focused production run records 219 requests across sampling defaults, stops,
image inputs and cache histories. Sampling overrides, thinking, concurrent
seeded requests, Unicode stops, tools, PNG/JPEG/WebP and the Chat, Responses
and Messages image paths pass. Text, thinking, tool and image cancellation
histories also pass after a process restart with actual disk-cache hits and
exact replay. These are correctness checks, not matched timing comparisons.
The preceding batch, tool-agent and long-context checks remain applicable;
this was not an all-model or all-functional-suite run.

The expanded ordinary corpus uses real non-thinking defaults (temperature 0.7,
top-p 0.8, top-k 20, presence penalty 1.5), seed 31 and main `3389a585`.
All nine ordinary answers now match main exactly: aggregate decode is
46.57 → 46.60 tok/s on the final scratch build. Copying from the current
sampled answer had changed two
answers, slowed C++ generation by about 4% and made a debugging answer 5.5%
longer in wall time with an unsupported statistic. Restricting the sources
removes both divergences. All 14 corpus/coding/tool outputs retain exact text,
reasoning and tool arguments; prefill, total decode and first-token phases pass
the unchanged 5% and 3 ms gate. One JSON response's largest gap increases
130.96 → 150.03 ms. Replaying its complete preceding request history gives
154.84 ms on unchanged main and 150.77 ms on the candidate, with identical work
and no phase flags. The initial gap remains inconclusive.
Generated C++ passes an independent
deque oracle, wraparound and move-only checks; generated Python LRU code passes
independent replacement, eviction and value-preservation checks.

Three existing-file edits retain the exact intended content and improve
85.44 → 105.93 tok/s in aggregate (9.06 → 7.54 seconds for the requests).
Fresh Python LRU generation remains exact at 74.82 → 75.51 tok/s.
A separate tool-write case omits the final newline on both main and candidate;
its strict byte-preservation check fails equally and is not a qualified
copy-speed result. The JSON schedule's original checker incorrectly required
a particular goal field name absent from the prompt; saved outputs pass the
corrected checker, with the original failure reports retained.

The Pi and 115K measurements below precede the scratch-only follow-up; the
final row-value, corpus, image/cancellation and benchmark checks above qualify
that unchanged arithmetic and state handling.
Real Pi 0.87.0 runs read/edit/write/test tools across five continued tasks.
With thinking off, all 21 requests and 16 tool calls retain identical assistant
content, tool arguments and tool results. Agent-process time is 26.12 → 25.92
seconds; the original identical-code slowdown is gone. Precise child-exit
timestamps exclude the functional harness's 500 ms polling granularity.
A reverse-order repeat retains every observation: one prefill delay also
appears on unchanged main, while maximum-gap flags move between requests and
overlap passing observations. These phases remain inconclusive, not a global
timing pass.

With thinking enabled and about 25K source tokens, all five tasks pass
(17 tool calls): 107.36 → 104.96 seconds. File timestamps are fixed before each
task because an earlier `ls -la` result changed the prompts between arms;
that earlier comparison is retained but cannot attribute latency to the PR.
The corrected code-creation task is 33.45 → 33.37 seconds with identical work.
The bug-fix task produces 35 extra sampled tokens and takes 21.35 → 22.22
seconds; the following literal-protocol task needs one fewer request.
The whole-agent result includes those different sampled choices and is not an
isolated inference speedup. Every comparable request phase passes the gate.
Sessions and actual tool results are retained outside Git.

New public workflow fixtures substitute for the author's unavailable private
prompts. Extraction checks exact records; generated URI tests must pass correct
code and detect three deliberate defects. A frozen 115K-token source archive
precedes a tool-driven edit that must preserve every unrelated byte, including
literal thinking/tool tags. Default sampling is used with fixed seeds:

| Request | Main tok/s | Candidate tok/s | Quality/work |
| --- | ---: | ---: | --- |
| Extraction, thinking | 84.49 | 84.59 | Exact answer and reasoning, 707 tokens |
| URI test generation, no thinking | 82.99 | 83.47 | Exact tests, all three mutants detected, 435 tokens |
| 115K agent edit, no thinking | 63.26 | 73.69 | Exact file/tool arguments, 187 tokens; cached continuation |
| 115K agent edit, thinking | 67.50 | 75.18 | Exact file/tool arguments and reasoning, 219 tokens |

The final scratch build repeats extraction and URI generation with exact
outputs; all eight captured phases pass the same timing gate.
The first final 115K run flags one maximum token gap (120 → 137 ms);
prefill, total decode time, output and expected cache work pass.
A focused main/candidate repeat of the same warmup and cached edit passes all
32 measured phases: the maximum gaps are 120.07/120.18 ms and decode rates
63.37/75.14 tok/s. The original gap remains inconclusive; the table retains
the first observation rather than substituting the faster repeat.
Tool-call IDs are generated independently and excluded from content comparison.
Changing thinking mode changes the rendered prefix, so its full prefill is
expected. The original main run's erroneous cache-hit assertion is retained;
its saved outputs and cache work were independently revalidated after fixing
that assertion, without rerunning or replacing the measurements. A second seed
also passes extraction and URI quality checks on the preceding candidate.

Before the scratch follow-up, canonical prepared pp2048/tg128 repetition
reaches 115.77 versus 73.18 tok/s at C1.
At C6, aggregate ordinary/repetitive rates are 109.54/231.31 versus
108.74/177.98; all measured completion hashes and cache/prefill counts agree.
Five C6 preparation flags remain in the raw request-index comparison: the
identical requests were admitted in different orders. Server logs show the
same admission/cache roles and costs (the first cached seven-token prefill is
102.23 versus 102.13 ms). Comparing each corresponding admission role passes
without averaging or changing limits. Earlier first-token variability reversed
in the unchanged-main control and remains inconclusive, not a global timing
pass.

A separate C8 history runs copies, then new text, then four of each. Its mixed
copy step initially verified 44 rows and raised the longest pause from main's
157–163 ms to 200 ms. Bounding greedy copied suffixes in mixed cohorts reduces
that pause to 145 ms. Copying/new-text rates in the final mixed cohort are
28.08/18.58 versus main's 21.63/17.95 tok/s; all-copy cohorts retain full width
and reach 31.42 versus 25.29 tok/s. All 24 responses retain exact text and
expected cache work. Ordinary generation immediately after copies remains
18.00 → 17.56 tok/s per session, with its longest pause reduced from 215 to
162 ms. Small first-token flags remain inconclusive: main's 23–31 ms variation
changes which request indices flag across unchanged controls, while the
candidate takes about 29–31 ms. No decode or maximum-gap phase flags remain
against either final main control. This is not a global timing pass.
These focused observations do not replace the complete benchmark table or
establish statistical language quality.

llama.cpp `10b8b755` also verifies context proposals against the target, but its
ngram path samples the target directly and compares the token. Gufo retains
point-mass rejection sampling for copies alongside its neural p/q proposals.
Both preserve the target distribution; identical seeded streams or a speed
advantage over llama.cpp are not claimed. Target arithmetic and weights are
unchanged.

**PR #626 follow-up, October 17:** sparse attention bounds temporary K-fragment
scheduling while retaining every dot product and softmax operation. Healthy
MTP cache-slot reassignment retains bounded rollback scratch and its graphs;
failure and explicit reset still free it. Prepared Qwen vision weights use
coarse managed storage, removing the reproduced text-dispatch stalls after
sidecar upload. Image activations and encoder arithmetic are unchanged.
The focused numerical, cache and image checks are listed below.

The complete HTTP C1 sweep retains all 24 completion hashes and prompt/cache/
draft counts. Its 12 AR requests match current main `78248de2` and pass every
measured phase at 5% and 3 ms. The affected mixed-MTP history was replayed
through 64K; two unprofiled follow-ups reach 1551.64 and 1558.51 tok/s. The
second pins the initial run's exact cache budget and matches all 11 request
identities with no phase flags against that candidate control. That request
shows no reclaim, swap or compaction activity. The table uses the faster
follow-up; neither a specific driver cause nor worst-case latency is claimed.
Raw attempts and diagnostics remain outside Git.

The canonical repetitive d0 measurement is 73.80 tok/s with the full vision
sidecar ready. In the isolated allocation control it is 73.77, versus 73.35
on the exact published `32c4ece8` binary and 42.53 with the current raw device
uploads. Four image/text HTTP controls preserve outputs and cache/draft work
and pass the per-request phase gate. This focused evidence does not replace
the older functional timing limits or upstream accuracy qualifications below.

**PR #603's reduced draft vocabulary preserves the target distribution and
supports the real sampling defaults.** The final review keeps the established
length controller and AR backoff for every request; it removes the proposed
confidence/calibrated stopping policy. That policy lost learned state across
native sampled snapshots and caused repeatable mixed-session and short greedy
slowdowns. The reduced head is used for C1 sampled requests, including penalties
and constraints, and eligible greedy requests. Unplanned multi-session requests
and modified greedy sampling retain the full draft head.

October 15, 2026, against `f6ca085a`, with the same pinned Nix toolchain and
Unsloth UD-Q4_K_XL/shared Q8_0 MTP weights:
25,328,640 full-vocabulary target logits after a 28,092-token prompt and
102 fixed continuation steps are finite and byte-identical. Independent MTP
formula checks retain their 0.002 relative-RMS limit (worst 0.0007655).
Native session, snapshot, component-restore, probability-correction and mixed
head checks pass. New regressions cover actual default sampling, complete
sampler/RNG replay, used-session table restoration and the subset path with
penalties. All 12 independent native sampled replay cases pass; the original
PR failed all 12.

HTTP requests that omit sampling controls exercise both real presets. All
16 short and two 35K-context default completions pass the timing gate; 17/18
retain identical output and draft counts. Python/story generation improves
3–10% at short context and the deep Python cases improve 5–8%. Translation
defaults are mostly level. The standard C1 AR/MTP captures, including warmups,
retain exact output/work with no phase flags; greedy MTP gains 5–12% at
d0/d32K. Focused AR/MTP API, cancellation, long-context and RAM/disk cache
checks pass correctness. Pi completes four continued coding tasks with
17 default-sampling requests and 13 tool calls; actual edits and test results
were inspected.

This is **not a global timing or individual-answer quality pass**. API queue
flags include simultaneous requests admitted in a different order; the same
order reversal occurs within main's own captures. Small snapshot/cache-resume
flags remain inconclusive: the MTP image snapshot is 83.84 → 88.97 ms, while
unchanged main reaches 87.40 ms. The AR one-token cache-resume outlier
(51.31 → 73.80 ms) does not recur: the final pair is 51.29 → 54.06 ms,
and all 737 phases in that repeat pass. Raw review captures are retained
outside the repository. A non-default sampled
translation repeats about 9% slower with different wording and acceptance;
its translation is correct, but its extra grammar explanation mislabels a
declension. Changed proposals can consume RNG differently across builds.
These numerical, replay and functional checks establish regression evidence
against main, not upstream unquantized-model accuracy or a statistical
language-quality result. The Gufo benchmark refresh below retains all llama.cpp
data unchanged.

The cache follow-up in `fdc21849` changes storage and bookkeeping while
preserving model arithmetic and sampling. Large pinned segments populate with
at most four workers and disable transparent huge pages before GPU registration
(incorporating #605). Capture retains its prepared shared chunks across allocation
retries, avoids repeated history scans and impossible partial allocations, and
allocates larger payloads first. Shared KV rows and private states use opposite
ends of the same bounded pool; KV rows retain ascending addresses for contiguous
restores. Payload owners shrink from 128 to 16 bytes. Existing reader pins,
atomic validity publication and generation checks remain authoritative.

Byte-pressure retry admission preserves the reused frontier, forks, last copies
and current edit boundaries. A retry adding at most 64 tokens can reclaim covered
older history; longer resumed turns can reclaim only their own covered ancestors.
Cold side requests prefer shallow live slots, preserving deep continuations.
Native regressions reproduced and then fixed fragmented private-state allocation,
reverse KV placement, lost retry reuse and premature deep-slot eviction.

Aligned registered snapshot buffers use a vectorized integer copy through their
GPU aliases; other buffers retain HIP copies. All 150 independent size/offset/
storage cases pass, including ragged allocation tails and unregistering a source.
Real AR/MTP snapshots at 2047 and 8203 tokens match pageable copies byte-for-byte.
Restored full logits, sampled/greedy continuations, learned drafts, real images,
mutation/failure guards and copying during a peer's graph capture pass.

An already-warm target pass can need its first graph or a replacement after
releasing MTP rollback storage. The new path enqueues that pass once and records
its graph while the GPU executes. The graph is not launched a second time;
explicit reset still releases all rollback storage. This removed reproduced
8–10 ms MTP and 5 ms AR short-request regressions in preceding cache candidates.
The affected AR pressure prefix has 21 exact requests and 316 passing timing
phases; the flagged decode is 76.25 ms on main and 73.02 ms with this change.
Deferring graph recording to the following request was rejected because it
moved the delay there.

The final native session suite passes: 65 exact full-logit AR/MTP frontier
comparisons, eight bitwise fresh-load replays, 25 sampling configurations per
mode, eight 200-token cache replays, image attachment/replacement, cancellation,
rollback release and C2/4/6/8 isolation. The focused cache, backing, retention,
component-runner and snapshot tests pass without relaxed tolerances or missing
model skips. Pi on the final `fdc21849` production binary completes four
continued coding tasks with 17 HTTP requests and 13 paired tool calls/results,
using default sampling and low thinking. All 16 follow-up requests reuse cache;
their first-token times are 145–488 ms. Tasks take 7.0–13.1 seconds. These are
observed smoke timings, not a matched-main timing gate. Actual edits, expected
failing tests and successful repairs were inspected; no orphan tool results or
literal framing tags appear in assistant text. The preceding `e0bead8b` run's
eight tasks, 33 requests and 25 tool pairs remain in the retained evidence.

The final production depth sweep has 32 captured requests with exact outputs,
prompt/cache/draft counts and all 256 timing phases passing against post-#603
main. All 63 measured concurrent completions match fresh AR, and the complete
128-request concurrency history retains exact output. Concurrent preparation
can change which identical request supplies the shared prefix; cohort prefill
work is unchanged. Loading is 15.27 s, and the two memory rows are 88.75/90.73
GiB, matching post-#603 main within 0.01 GiB. The older published memory rows
came from an earlier source revision.

Completed MTP continuation and small-budget pressure captures have 219 requests,
including 208 complete responses with exact request/output/draft identities and
no lost reuse. The final AR pressure capture has 100 exact requests and no lost
reuse. Four timing flags concern one short decode: 79.36 ms here versus an older
73.06 ms control, while fresher unchanged main takes 79.06 ms. Both controls and
their concurrent admission histories are retained; this remains inconclusive.
The prepared C6/C8 MTP repeat confirms a small first-token cost: approximately
23 → 35 ms and 31 → 59 ms. Aggregate decode is 176.28 → 175.84 tok/s and
194.16 → 194.12 tok/s; C8 whole-request time increases about 0.5%. This startup
tradeoff was accepted for publication. It remains a timing-gate failure, not a
global timing pass. GPU traces locate the wait before session reset; changing
the copy method or queue limit did not resolve it, and neither experiment was
retained. All original timing flags and rejected candidates remain retained in
the [compact review record](artifacts/cache-checkpoint-review.json) and raw archives.
Flash is the only production consumer of this component cache; other text
models retain the legacy path.

**PR #596 repairs the complete functional inventory:** all 48 registered
Flash-Next suites pass in AR and MTP at `9985ff0a`, including all four deferred
cache workloads (122 expanded checks, 3,334 requests). The final allocation
follow-up retains the same harness and logical cache fixes and receives focused
continuation/cache revalidation. Existing numerical tolerances are unchanged;
CPU regressions, GPU continuation/snapshot checks and actual Pi tool/image
results are retained in [the qualification evidence](artifacts/functional-fixes.json).
This is functional regression evidence, not upstream model/conversion accuracy
or a clean global timing gate. The unreproduced five-minute MTP snapshot stall
and individual timing flags remain visible.

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

**Upstream sync onto main `1f97092` (October 18, 2026): a checked
integration, not a new timing qualification.** Source `9546bae` is an ordinary
merge of upstream main `1f97092` into the previous sync `5df6da3`: `3389a58`
plus #613 `8497711` (upstream prompt lookup) and #633 `1f97092` (long-term
page pins for cache backing, cold-prompt hipBLASLt preparation). Upstream's
prompt lookup replaces the fork's, and the Flash-Next model sources are
identical to upstream. The fork keeps only `--prepare-structured-output` and
`--cache-ram-commit eager`. Eager commitment grows through #633's pinning
path, and `backing_prepared` also reports `long_term_pinned_bytes`.

- **Build:** pinned flake; release `bin/gufo` sha256 `14beb7ed…5ea24e`.
  Formatting (584 files), the hosted PR suite (59 tests) and the CPU
  `prompt_lookup`/`ngram`/`draft_vocab`/`mtp_sampling` tests pass.
- **GPU checks, each run once (1,061 s in total, including diagnosis):**
  - `cache_backing_test` passes normally and with `--deny-long-term-pins`.
    The container refuses the long-term pin itself (8 MiB memlock, seccomp),
    so the pinned-lifetime case is skipped and the registered fallback runs.
  - `session_test --cache-only` and `continuation_adapter_test --reuse-only`
    pass, and so do the 27 checks of upstream's `prompt_lookup.py`.
  - `session_test --batch-only`: serial and batched greedy C2–C8 and seeded
    runs are exact. Upstream's lookup batch check is exact at C2–C8, with 43
    sampled and 118 greedy accepted copies, batched snapshots, request reset,
    and restore into a used session with rewind. Greedy lookup matches AR.
    The run then fails in `CheckDraftVocabLifecycle` ("tabled text did not
    return to the small head"), an upstream test inconsistency: #613 loads
    the test model with `decode_concurrency = 8`, under which unplanned
    requests keep the full draft head by design, and greedy copy cycles skip
    the draft head. The function and the engine are identical to upstream.
    Diagnostic copies, not committed, pass every exactness check in the
    function; with `decode_concurrency = 1` the small head is used again.
- **262K deployment configuration** (one session, vision, 16 GiB eager RAM
  cache, structured-output preparation; the command below):
  - The harness passes over discovery, state-edges, tool-agent,
    tool-choice-cache, image-inputs, and cache with its disk restart. Against
    `bf6e11a`, all 136 requests have identical bodies and outputs apart from
    the known `agent_sampled_peers` 372/374 peer swap.
  - Preparation order is unchanged: vision, then the eager backing (15/15
    segments), then structured output, all before readiness.
    `long_term_pinned_bytes` is 0, the documented fallback. After the commit,
    17,491 MiB host memory and 18.09 GB device heap are free; the lowest
    MemAvailable is 16,765 MiB. No backing growth, limits, reclaim,
    compaction, PSI or major faults.
  - The screenshot answer is identical on the first request, the cache-hit
    repeat (2,077 cached), after restart, and to `bf6e11a`'s; so are the tool
    call, structured output and continuation.
  - A B B A against `ae530008`, with identical outputs: warm-up prefill
    247 → 164 ms (#633's cold-prompt preparation), rewrite decode
    19.15 → 17.22 s with identical drafts, prose unchanged. The coding-agent
    history drafts differently (94 against 84 drafts), so its timing is not
    comparable.
- **Compatibility:** the snapshot payload version changes from 17 to 18.
  Checkpoints from earlier builds are not reused, and a Flash-Next server
  removes earlier-release checkpoint files from its `--cache-disk` directory.
  The deployment command is unchanged and has no disk cache.
- **Not covered:** a timing qualification, timing at 262K, the long-term pin
  path itself, two-session serving with this binary, and concurrent vision.

[Evidence](artifacts/upstream-sync-1f97092.json).

**Fork history.** The records from here to the end of this section describe the
fork's own prompt lookup, `--prepare-structured-output` and
`--cache-ram-commit eager` through `5df6da3`. In the sync onto upstream main
`1f97092`, upstream's prompt lookup (#613) replaced the fork's lookup; the two
options remain. Lookup results below are historical evidence for the replaced
implementation.

**Upstream sync onto main `3389a58` (October 17, 2026): a checked
integration, not a new timing qualification.** Source `bf6e11a` is an
ordinary merge of upstream main `3389a58` into the previous sync `825b714`.
The target is release v0.17.0 `2af314b` (#623, #621 and #625 over `baf5a1b`)
plus #626 `5a1669a` and #630 `3389a58`, which were merged after the release.
It keeps prompt lookup, `--prepare-structured-output` and
`--cache-ram-commit eager`. None of them has an upstream equivalent, and #613
is not imported. The remaining diff against upstream is line-for-line the
previous diff against `baf5a1b`. Prompt lookup's state is cleared in #626's
`Session::ResetImpl`, so it is cleared both by explicit reset and by healthy
slot reassignment, which now keeps MTP rollback scratch.

- **Build:** pinned flake; release `bin/gufo` sha256 `ae530008…56ec63`.
  Formatting (584 files), the hosted PR suite and the CPU
  `prompt_lookup`/`draft_vocab`/`mtp_sampling` tests pass.
- **GPU checks, each run once (1,014 s in total):**
  - `continuation_adapter_test --reuse-only` (#626's regression, here with
    lookup present): fresh and reused AR and MTP slots give identical logits,
    greedy and sampled tokens and draft counts. Retained scratch stays within
    `SessionBytes`, and a cancelled slot and `Reset` release it.
  - `session_test --batch-only`: serial and batched greedy C2–C8 and seeded
    runs are exact; 5 sampled lookup copies.
  - `session_test --cache-only`: lookup replay after diverged and
    mid-generation restores is exact (17 greedy and 174 sampled copies), and
    so are MTP cache replay and the draft-policy snapshots.
- **262K deployment configuration** (the command below, Intel 670p models):
  - The harness passes over discovery, state-edges, tool-agent,
    tool-choice-cache, image-inputs, and cache with its disk restart.
  - Against `510f37e`, the 117 shared requests match except where #623
    changes them: 10 state-edges prompts with a required or named tool choice
    are 17 tokens shorter (one free-form tool argument differs), and
    `agent_limit` now reuses 505 cached tokens instead of prefilling 522. The
    known `agent_sampled_peers` 372/374 peer swap recurs. Accepted drafts are
    930 on both.
  - Preparation order is unchanged: vision, then the eager backing (15/15
    segments), then structured output, all before readiness.
  - Screenshot: the answer is identical on the first request, the cache-hit
    repeat (2,077 cached), after restart, and to `510f37e`'s. The tool call,
    structured output and continuation are identical to `510f37e`'s.
  - Memory: #626 moves the 862 MiB of vision weights from device-heap GTT to
    managed memory. Server GTT at readiness is 946 MiB lower and RssAnon is
    864 MiB higher; vision upload takes 148 ms instead of 58 ms. The eager
    commit is unchanged (15,220,277,248 bytes). At least 17.2 GiB host and
    18.1 GB device heap remain at preparation (8 GiB required). Zero direct
    reclaim, compaction, PSI and major faults; image queue 51–53 ms.
- **Performance sanity** (fresh processes, A B B A; A = `510f37e`
  `a4d404ed` with the same command): identical outputs and work in every
  request, and no request is slower by more than 5% and 3 ms in both pairs.
  - The warm-up, the first text request after vision preparation, decodes 64
    tokens in 759.6–761.0 ms (A: 759.7–759.9 ms), and `prose` in
    10,007–10,012 ms (A: 9,985 ms). #626's reported vision-preparation
    slowdown does not appear in these requests.
  - The rewrite decodes in 19,137–19,156 ms (A: 19,230–19,235 ms).
  - The deep rewrite (decode_ab's 46,739-token prefix, then the rewrite;
    1,704 of 1,708 drafts accepted) prefills in 27,579–27,969 ms
    (A: 27,895–27,946 ms) and decodes in 18,720–18,725 ms
    (A: 18,857–18,860 ms).
- **Not covered:** the 35-pair timing qualification; timing at 262K;
  full-context capacity; the long-context suite at 262K; concurrent or
  multi-session vision; images above 1920×1088.
- **Deployment command:** unchanged from the record below, including
  `--max-request-bytes 67108864`.
- Evidence: [upstream-sync-v017.json](artifacts/upstream-sync-v017.json).

**Upstream sync onto main `baf5a1b` (October 16, 2026): a checked
integration, not a new timing qualification.** Source `510f37e` is an
ordinary merge of upstream main `baf5a1b` (release v0.16.0 `b1c46d6` plus
#614, #616 and #622, merged after the release) into the v0.15.1 release
`1785483`. It keeps prompt lookup, `--prepare-structured-output` and
`--cache-ram-commit eager` (with `CommitAll`). Upstream replaced two of our
patches: #616's automatic vision preparation (`--prepare-vision` is removed)
and #615's backing population (instead of our page-by-page commit). #613 is not
imported. The 913-row 32K qualification below remains evidence for `8b138ee`
only; the new binary was not re-timed against it.

- **Build:** pinned flake; release `bin/gufo` sha256 `a4d404ed…cb218`.
  Formatting, the hosted PR suite and the CPU `prompt_lookup`/`draft_vocab`
  tests pass.
- **GPU checks, each run once (982.7 s in total):**
  - `cache_backing_test` ×3: commit 29–31 ms, longest concurrent mapping
    14.9–16.1 ms (bound 30 ms).
  - `session_test --batch-only`: serial and batched greedy C2–C8 and seeded
    runs are exact, including lookup and small-head counters; 5 sampled
    lookup copies.
  - `session_test --cache-only`: lookup replay after diverged and
    mid-generation restores is exact (17 greedy and 174 sampled copies), and
    so are #603's draft-policy snapshots.
- **262K deployment configuration** (Intel 670p models, 1 session, vision,
  16 GiB eager cache, structured-output preparation):
  - The harness passes over discovery, state-edges, tool-agent, image-inputs
    and cache with its restart. All 117 requests match the `8b138ee` run in
    output and token/cache counts; only the 3 discovery outputs differ, by the
    served model name.
  - In every process, preparation runs vision, then the eager backing (15/15
    segments), then structured output, all before readiness.
  - Screenshot: the answer is identical on the first request, the cache-hit
    repeat (2,077 cached, 0 prefilled) and after restart, and to `8b138ee`'s.
    The tool call, schema-valid structured output (identical after restart)
    and a continuation (2,077 cached) succeed. GTT grows +438 MiB at the first
    image and ≤ 105 MiB at later requests. At least 16.4 GiB host and 17.1 GB
    device heap remain at preparation. Zero direct reclaim, compaction,
    container PSI and major faults; image queue 53–56 ms.
- **Performance sanity** (fresh processes, order A B B A; A = `8b138ee` with
  its own command): identical outputs and work. The rewrite decodes in
  19,294 ms (A: 19,312–19,331 ms) with 1,704 of 1,710 drafts accepted. The
  `prose` request decodes in 10,020–10,027 ms (A: 10,037–10,042 ms).
  - Stock `baf5a1b` control: `prose` 10,017 ms, and the rewrite 27,054–27,063
    ms with 1,639 of 1,772 drafts accepted. So lookup cuts the rewrite's
    decode by 28.7%.
  - #603's reported greedy gain does not appear in this request on stock
    either.
- **Not covered:** the 35-pair timing qualification; timing at 262K;
  full-context capacity; concurrent or multi-session vision; images above
  1920×1088.
- **Deployment command:** `gufo serve --host … --port …
  --max-request-bytes 67108864 llm --model
  UD-Q4_K_XL/Qwen3.8-Flash-Next-UD-Q4_K_XL-00001-of-00004.gguf
  --served-model-name Qwen3.8-Flash-Next-UD-Q4_K_XL --mmproj mmproj-BF16.gguf
  --speculative mtp --mtp-model MTP/unsloth/mtp-Qwen3.8-Flash-Next-shared-Q8_0.gguf
  --sessions 1 --context 262144 --cache-ram-bytes 17179869184
  --cache-ram-commit eager --prepare-structured-output`. It drops
  `--prepare-vision`, which no longer exists.
  - `--max-request-bytes 67108864` was added after the checks above, which
    used the 8 MiB default. In live use, a Pi coding session with screenshots
    reached about 7.3 MB per request at 165K tokens. The next request was
    rejected with HTTP 413 `payload_too_large`.
  - The option raises only the HTTP request-body limit to 64 MiB. Image-byte,
    pixel and context limits are unchanged.
  - Commands in the records below apply to their own revisions.
- Evidence: [upstream-sync-v016.json](artifacts/upstream-sync-v016.json).

**Prompt lookup on v0.15.1 (October 15, 2026): timing-qualified for the
2-session 32K gate configuration. The final candidate adds opt-in vision
preparation, with deployment compatibility evidence at 1 session and a
configured 262K context for the tested image workloads.** Final source `8b138ee`
(release `bin/gufo` sha256 `e31a72ae…`) is upstream v0.15.1 with the shared
serving fixes, the four prompt-lookup files, and `--prepare-vision`. The
baseline `924010e` differs from it only by the prompt-lookup files.

- **Timing (screen-resolution-v2, 5%/3 ms, unchanged):** 35 alternating matched
  pairs of `6c1838b` against `7a4c5bb`. Configuration: 2 sessions, 32768
  context, harness disk cache, 16 GiB eager RAM cache and
  `--prepare-structured-output`, over state-edges, tool-agent, long-context and
  cache. Result: 913 rows pass, 0 inconclusive, 0 failed (63 diagnostic, 2
  superseded).
  - The 27 pairs added to sp71's 8 were fixed before data. sp71's 3
    inconclusive rows now resolve, with bounds 1.09/3.32 and 0.10/3.00 ms.
  - `long_schema_cold`'s slow state occurred in 6/35 A and 11/35 B runs
    (p = 0.26). A modest increase in its frequency is not excluded.
  - **The final binaries were not measured in this comparison.** Their only
    addition runs only with `--prepare-vision`, which this configuration does
    not pass.
- **Vision preparation (`--prepare-vision`):**
  - Without the flag, the first image request of each process hashes the 907 MB
    sidecar in its queue (371–490 ms). After admission and the cache were sized,
    it also uploads 862 MiB of weights.
  - With the flag, loading does both before readiness.
  - Cost per process start: 0.39–0.42 s of hashing and 39–42 ms of upload.
    Device memory at readiness rises by 1,004 MiB: 862.1 MiB of weights plus
    141.9 MiB of one-time GPU runtime state that otherwise appears at an early
    request. RSS includes about 866 MiB of clean sidecar file pages.
- **262K vision compatibility:** two harness runs with 128×128 fixtures, and one
  1920×1080 screenshot (2,040 image tokens).
  - The 117 harness requests are identical to the run before the fix.
  - The first image request no longer waits or allocates.
  - About 25 repeated encodes per process do not accumulate memory.
  - The screenshot is described correctly and its repeat is reused.
  - Host and GTT headroom stay at least 10.5 and 33.9 GiB.
  - There is no direct reclaim, compaction, or container memory pressure in any
    of these runs.
- **Specification errors accepted in the release decision:** two sp73 checker
  criteria and one in the screenshot check. Their original failed results are
  retained.
  - The readiness memory expectation omitted the 141.9 MiB.
  - System memory pressure counted kswapd. In that run, ~200 ms slower requests
    coincided with the server faulting file-backed model data back in after a
    cold page cache.
  - A 50 ms queue limit was calibrated on 128×128 images. The screenshot's
    per-request decode, resize and hash take 51–58 ms.
- **Not tested:** timing at 262K; full-context capacity; images above 1920×1088
  (up to 16.7 MP; per-encode scratch up to ~3.4 GB, not reserved by admission);
  concurrent or multi-session vision; image requests without
  `--prepare-vision`.
- **Tested deployment command:** `gufo serve --host … --port … llm --model
  UD-Q4_K_XL/Qwen3.8-Flash-Next-UD-Q4_K_XL-00001-of-00004.gguf
  --served-model-name Qwen3.8-Flash-Next-UD-Q4_K_XL --mmproj mmproj-BF16.gguf
  --speculative mtp --mtp-model MTP/unsloth/mtp-Qwen3.8-Flash-Next-shared-Q8_0.gguf
  --sessions 1 --context 262144 --cache-ram-bytes 17179869184
  --cache-ram-commit eager --prepare-structured-output --prepare-vision`.
- Evidence: [prompt-lookup-v0151-release.json](artifacts/prompt-lookup-v0151-release.json).

**Correction (October 15, 2026): the timing qualification recorded below
is withdrawn; timing qualification of prompt lookup on v0.15.0 is
inconclusive.** This withdraws an unsupported claim and qualifies no new
release; production source is unchanged from `51de7b1` (candidate `9129bb2`).
The `resolve.py` used for that claim kept the screen's pass for rows it had
not flagged, unless a regression check fired that cannot fire with 8 pairs over
about 900 rows. Judging every repeated row on its repetitions (the
`fix(functional): judge every repeated timing row on its repetitions` commit
on `testing`), the same retained evidence leaves 14 latency rows
inconclusive: disk-spacing turns 4 and 6 and text-cancel 0 (token gap, wall
and server time) and legacy-tool-cancel 6 (queue, TTFT, wall and server
time) exceed their margins in 2–4 of 8 pairs. No row fails and no regression
is established. The disk-spacing and text-cancel rows switch between two
states about 75 ms apart on both builds; resampling the retained runs gives
at most a 2% chance that 10 more pairs would pass, so no further measurement
was run. Correctness, the spacing fix, cold start, the resolved flags and the
rewrite result are unaffected.
[Correction evidence](artifacts/prompt-lookup-v015-correction.json).

**Superseded record — Prompt lookup on v0.15.0 (October 15, 2026): qualified
for the affected gate under the screen-resolution timing policy below; the
unchanged `compare.py` screen alone stays inconclusive.** Candidate `9129bb2`: v0.15.0
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
| Vision weight storage, October 17 | Flash-Next and 27B each retain 29 finite, byte-identical encoder stages on a deterministic 1024² texture: 134,742,016 and 137,363,456 F32 values respectively. Prepared upload, repeated upload, resident-byte accounting and later encoding pass with both projectors. Four matched production image/text requests retain exact outputs and token/cache/draft counts, with no phase exceeding both 5% and 3 ms; cached image retry performs zero prefill. The original uncached repetitive text history recovers 73.77 tok/s with the full vision sidecar prepared, versus 73.35 on the exact published binary. Native storage parity does not resolve the existing image accuracy gap against the reference. |
| Sparse scheduling, October 17 | Against PR #626 head `c1088f89`, 17,134,080 finite full-vocabulary logits are byte-identical after 133,945 prompt tokens and 69 fixed continuation steps. The complete independent FP64 attention error report is identical, including ragged and 128K cases; all 12 saved real selection masks retain exact attention outputs. |
| Healthy MTP slot reassignment, October 17 | The new adapter regression fails before the fix and passes afterward in AR/MTP. Reused and fresh sessions agree on full logits, greedy/sampled tokens and draft counts; memory stays within the existing reservation, while cancellation and explicit reset release scratch. Guard lifetime and seeded native RAM/serialized cache replay pass. The focused HTTP `cache` and `long-context` suites pass with the Qwen sampling preset, 624 actual drafts and 458 accepted drafts, including text/thinking/tool/image cancellation and disk resume. These are correctness results, not a global timing pass. |
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
timings only steer greedy decoding. Draft sampling uses the selected Q8 head's
top 64 logits, with proposals corrected against the full target distribution;
upstream draft-sampler equivalence is not claimed.
Prompt-lookup copies are point-mass proposals: accepted with the target
probability, with the residual excluding the copied token. They change which
draws a seeded request consumes relative to builds without lookup; replay
within one build stays exact and does not depend on batching.
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

Single-user and concurrency tables were refreshed on October 17, 2026, with
production Nix build `3b02d877` plus the reusable PLE gather scratch
(binary SHA-256 prefix `93ec6640be94adb9`). The artifacts retain the dirty
build identity, and the production file hashes identify the exact source.
Loading and memory use the
October 16 `fdc21849` capture (binary prefix `b1ab0e9795df6fba`).
The pinned toolchain is unchanged. Every single-user row reproduces main's
completion hashes and prompt/cache counts; prompt lookup changes draft counts.
Four prior alternating mixed
MTP C2 repeats gave 110 drafts (about 54 tok/s) on main twice and the PR once,
and 144 drafts (about 60 tok/s) on the PR once, all with identical text.
The checked platform profile is `performance`, with tuned
`accelerator-performance`. Temperature telemetry is retained throughout.
Model jobs run sequentially without overlapping builds or profiling.
Commands, binary/model identities, counts and
retained samples are in [artifacts](artifacts/bench.json) and
[model identities](artifacts/model-identities.json). Full raw requests, phase
timings, diagnostic profiles and repeated controls are retained outside Git.

The driver is `tools/bench/model-bench.py run --target gufo --fresh`.
One warmed sample per point, HTTP, greedy, thinking off, penalties disabled;
automatic cache RAM budget. Single-user uses ordered pp2048/tg128 histories at
every depth from 0 to 131072, with capacity 133760. MTP pp is the maximum across
the independently retained mixed/repetitive workloads. Diagnostic profiled
runs and abandoned candidates do not supply published cells. Matched AR and
MTP controls for this refresh use main `3389a585`.
Controls preserve prompts, settings and ordered cache histories;
per-request phase flags require both a 5% and a 3 ms increase.

C1/2/4/6/8 use the same d0 prompts. Every session prefills before measured tg128,
with at most four prompt-tail tokens reevaluated. Rates sum individual decode
rates. Fresh C1 AR controls provide completion hashes for both workloads.
Depth calibration depends on the ordered sweep; paired speed controls retain
that order. Deep AR/MTP HTTP cache frontiers differ by one token, so exact
snapshot replay is checked separately with identical prefill boundaries.

All llama.cpp data retain September 22–23 provenance, unchanged at the user's
request; no reference benchmark or quality refresh was run. AR uses `b11069`;
historical MTP uses `6fcaa16f`. Reference capacity is 35456 through 32K, 68224
at 64K and 133760 at 128K. A separate October 10 check of that MTP pin at
pp2048/tg128, C1/capacity 4096 found that fresh and prepared mixed-prose MTP
outputs matched each other but differed from its own AR output. Both generated
128 tokens and MTP proposed 152 drafts; repetitive MTP was not attempted after
that failure. The historical speed values are not newly qualified equivalence
measurements. [Evidence](artifacts/decode-kernels-review.json).

Loading measures cold target/sidecar files to HTTP readiness, verified with
`POSIX_FADV_DONTNEED` and `mincore` on every file, C1/MTP/capacity 262144;
runtime libraries may remain warm. Memory uses C1/AR/capacity 133121 and peak
global HIP allocation, including idle memory. See the
[benchmark workflow](../../../.agents/skills/benchmark-model/SKILL.md).
