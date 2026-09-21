# Mozc Contextual Ranking v2 — Best Practices and Restart Plan

Status: canonical design guidance for the v2 restart  
Scope: `shumaimai/Mozc-Ai` runtime/integration and `shumaimai/Mozc-Ai-Training` model/data work

## 1. Why v2 is a restart

The project has accumulated useful infrastructure, but several experiments optimized the wrong layer before the production contract was stable.

The v2 restart keeps the proven integration/runtime work and discards assumptions that made model results hard to trust.

Core rule:

> Do not replace Mozc's knowledge with AI. Use a contextual model only to correct Mozc when there is evidence that a correction is useful and safe.

A second rule is equally important:

> Training, evaluation, runtime input construction, candidate generation, context clipping, and logging must use the same production contract before model optimization begins.

## 2. What the repository history taught us

### 2.1 The original asynchronous LLM rewriter solved latency by delaying usefulness

The early `AIRewriter` architecture was intentionally non-blocking:

- cache lookup in the IME path
- background Ollama / API request on a miss
- AI result available on a later conversion

This protected IME responsiveness, but it was poorly matched to context-sensitive conversion. A result generated for one context could not reliably help the current conversion.

A later cache fix disabled context in the default cache key because context-aware keys changed too often. That improved reuse, but weakened the relationship between the cached answer and the active context.

Conclusion: generative/cache-first AI is not the primary v2 architecture.

### 2.2 The 30M reranker was closer to the right production shape

The v1 contextual reranker introduced several good practices:

- actual Mozc N-best candidates
- local-only persistent ONNX runtime
- one batched forward for all candidates
- dynamic padding in the runtime
- preceding-context clipping
- a confidence / margin gate
- strict timeout and Mozc-order fail-safe
- opt-in logging and offline/privacy-friendly packaging

The 30M ModernBERT model also established a strong latency baseline.

These are assets to preserve.

### 2.3 Train/serve parity was not guaranteed

The v1 training formatter used literal ` [SEP] ` delimiters:

```text
読み: ...
 [SEP] 文脈: ...
 [SEP] 候補: ...
```

The shipped runtime formatter used newlines:

```text
読み: ...
文脈: ...
候補: ...
```

Because the model uses SentencePiece tokenization, these are not guaranteed to produce equivalent token IDs.

v2 requirement: input construction must be defined once and verified by token-level parity tests across training, evaluation and runtime.

### 2.4 Reranker placement did not sufficiently protect user intent

The shipped reranker was integrated near the tail of the rewriter chain, after Mozc user-history rewriters.

That means a neural reranker could overturn a candidate that Mozc had promoted because of explicit user history.

Mozc's own user-history logic is deliberately conservative. It considers POS grouping, functional value, context-sensitive attributes, punctuation, numbers, and other safety conditions.

v2 requirement: explicit user preference/history must have higher authority than contextual AI correction, or must be protected by an equivalent hard constraint.

### 2.5 Production logging must identify the exact target segment

The reranker scores the last conversion segment, but historical logging code can read the committed result from conversion segment 0.

This makes multi-segment replay ambiguous.

v2 requirement: every logged decision must carry a stable conversion ID and target segment index, and the committed candidate must be read from the same segment that was scored.

### 2.6 Offline benchmark quality was not enough

The v1 contextual model looked strong on Wikipedia-derived evaluations, but real-usage replay exposed regressions.

The safety guard reduced damage mainly by refusing to rerank most cases.

This taught two things:

1. aggregate Hit@1 on a corpus is not sufficient for IME deployment;
2. `hurt` and overwrite behavior must be first-class metrics.

### 2.7 The Sarashina/JEV PoC optimized compression before validating the task

The JEV experiment produced useful technical findings about QAT, INT8, FP16 embeddings, layer pruning and vocabulary size, but the task setup drifted away from production:

- candidates were generated from corpus-frequency groups instead of real Mozc N-best
- context clipping differed from production
- train/eval initially came from only a few source articles
- the model was often evaluated by unconditional argmax rather than Mozc-aware correction
- fixed `[5,128]` inference wasted sequence compute
- layer/vocab/quantization optimization happened before generalization was established

The fresh article-disjoint holdout exposed the problem.

Conclusion: freeze the JEV branch as research evidence. Do not continue shrinking it as the production path.

## 3. External design lessons that align with the history

### Upstream Mozc

Mozc already treats preceding text and internal conversion history as structured state. Surrounding text is used to detect stale history and reconstruct limited history when possible.

v2 should therefore use surrounding text as part of Mozc state, not merely as a long text prompt.

### User segment history

Mozc's history rewriter is selective about what can safely replace the current best candidate.

v2 should inherit this philosophy:

- prefer corrections over replacement
- respect POS/function equivalence where possible
- protect user-selected/history candidates
- be conservative around numbers, punctuation and short functional strings

### Mozkey-style safeguards

Practical Mozc forks use deterministic syntax/functional-word guards for cases such as particles losing to homophonous kanji.

This is a good fit for errors like `に` vs `二`: use dictionary/POS/syntax rules before invoking a neural model.

### Zenzai-style context ordering

Modern contextual KKC systems put context before the input/output task.

If v2 uses an autoregressive or causal teacher, the preferred information order is:

```text
context -> reading/input -> candidate/output decision
```

not candidate first followed by a long context.

## 4. v2 architecture

The production pipeline should be layered:

```text
Input
  -> upstream Mozc converter
  -> real Mozc candidates + costs + POS + attributes
  -> deterministic safeguards / syntax fixes
  -> optional contextual neural correction
  -> Mozc-score + neural-delta fusion
  -> confidence gate
  -> user-history / explicit preference protection
  -> final candidates
```

The neural component should not learn a fresh absolute ranking from strings alone.

Preferred target:

```text
final_score = normalized_mozc_score + alpha * neural_delta
```

or an equivalent learned correction formulation.

The model should receive structured Mozc evidence where feasible:

- Mozc rank
- relative cost / cost delta
- POS IDs or POS groups
- candidate attributes
- reading
- clipped preceding context
- candidate surface/content value

## 5. Deterministic layer before neural inference

Not every mistake deserves model inference.

Examples to handle first with dictionary/POS/syntax logic:

- particles/function words vs homophonous kanji
- obvious numeric conflicts
- punctuation/symbol behavior
- known pathological segmentation patterns
- cases protected by user dictionary/history

The neural model should focus on genuinely ambiguous context-sensitive ranking.

## 6. Context contract

Use one canonical context policy.

Initial production baseline:

- use Mozc/request preceding text and internal history consistently
- keep only the active sentence or equivalent local context
- clip to at most 50 Unicode characters
- test 20/30/50-character ablations later
- never silently use a different training-time clip

The exact context function must be shared or parity-tested across:

- dataset generation
- training
- evaluation
- Python runtime
- C++ integration

## 7. Dataset v2

The old proxy dataset is not a production benchmark.

Dataset v2 must be generated from actual Mozc candidates.

Required pipeline:

```text
licensed/public Japanese source text
  -> derive reading/gold
  -> query real Mozc
  -> collect visible top-K candidates
  -> attach production-identical context
  -> store Mozc metadata
  -> split by source document before training
```

Required split properties:

- train / validation / final test are document-disjoint
- source ID overlap must be zero
- cap examples per document
- report source concentration statistics
- final test is not used for model selection

Candidate 0 must mean actual Mozc top-1, not corpus-frequency top-1.

Dataset reports must include:

- row count
- distinct source count
- max/median rows per source
- top source contributions
- Mozc top-1 accuracy
- top-K oracle coverage
- context-length distribution
- candidate-count distribution
- split overlap checks

## 8. Evaluation contract

Do not approve a model on Hit@1 alone.

Every experiment should report:

- Mozc top-1 accuracy
- model/fused final top-1 accuracy
- delta vs Mozc
- helped count
- hurt count
- net helped-minus-hurt
- overwrite count/rate
- regression rate
- context-sensitive subset
- short reading subset
- numeric/function-word subset
- proper-noun subset where available
- user-history protected subset
- p50 / p95 latency
- model size
- process RAM

For deployment, `hurt` is more important than raw number of changed candidates.

A candidate model that helps often but frequently destroys correct Mozc choices is not acceptable.

## 9. Real-usage replay contract

Real usage is a release gate, not an optional afterthought.

Each logged decision should include a privacy-conscious schema such as:

- conversion/session ID
- target segment index
- reading
- Mozc top-K surfaces
- Mozc ranks/cost deltas/POS/selected attributes
- model scores or correction deltas
- model proposed top-1
- final top-1
- whether an overwrite occurred
- actually committed candidate
- context length and non-reversible context class
- timing information

Avoid storing raw preceding text by default.

Multi-segment logging must be tested explicitly.

## 10. Model comparison plan

Start with the simplest competitive baseline.

### Baseline A: ModernBERT-ja-30M cross encoder

Purpose:

- known small CPU model
- strong latency reference
- simple production integration

### Baseline B: small page-wise encoder

One sequence represents the visible candidate page:

```text
[CTX] ...
[READING] ...
[C1 rank/cost/POS] ...
[C2 rank/cost/POS] ...
...
```

Output: one logit/correction per candidate.

This avoids repeating the same context for each candidate.

### Teacher C: Sarashina/JEV or another stronger Japanese model

Use only if it produces a meaningful quality ceiling.

A large teacher does not need to ship. Distill useful behavior into the small student.

## 11. Inference best practices

Production inference requirements:

- persistent process/model
- local-only by default
- one forward per visible candidate group where practical
- dynamic sequence length
- no fixed 128-token padding unless benchmarked and justified
- bounded candidate count
- strict timeout
- fail safe to unchanged Mozc order
- warmup before benchmark
- record CPU model, ORT version and thread count
- compare variants in the same container/process environment

Do not interpret latency measured on different Modal hosts as an architectural difference.

## 12. Quantization and compression policy

Compression happens after the task works.

Correct order:

1. validate dataset and train/serve parity
2. beat Mozc on document-disjoint validation
3. pass real-use replay with acceptable hurt rate
4. establish a stable FP32/BF16 model
5. optimize architecture
6. quantize
7. prune vocabulary/layers only if still needed

Useful prior research to retain:

- FP16 embedding storage can save substantial space without observed accuracy loss in the JEV PoC
- ORT dynamic INT8 requires activation-aware QAT considerations
- activation fake quant was important in the previously reproducible QAT-v2 path
- weight-only-style QAT did not reproduce good exported INT8 accuracy
- environment-sensitive ORT INT8 behavior requires reproducibility checks

These are implementation lessons, not reasons to keep the old JEV model.

## 13. CI invariants

v2 CI should fail if any of the following break:

### Input parity
Given a fixture, training/eval/runtime token IDs must match exactly for the same model interface.

### Context parity
Canonical context cleaner/clipping fixtures must match across Python and C++.

### Candidate parity
A recorded Mozc fixture must reproduce the same candidate order and metadata for the pinned Mozc revision.

### Segment parity
The segment scored, logged and committed must use the same target index.

### Safety fallback
Timeout, runtime failure, malformed output or low confidence must preserve Mozc order.

### User intent protection
Candidates protected by explicit user history/dictionary rules must not be silently overturned.

## 14. Repository policy

### `Mozc-Ai`

Owns:

- upstream Mozc pin/integration
- runtime candidate extraction
- deterministic rules/guards
- score fusion
- user-history protection
- logging contract
- Windows MSI/runtime/CI
- real-usage replay fixtures where privacy-safe

### `Mozc-Ai-Training`

Owns:

- Dataset v2 generator
- source/split manifests
- training/evaluation code
- model experiments
- distillation
- ONNX export/optimization
- reproducible benchmark reports

The production contract is shared between the repositories and must have parity tests.

## 15. What is frozen

Do not delete old research.

Freeze and label as historical:

- async Ollama/DeepSeek AIRewriter path
- old v1 contextual benchmark reports
- Sarashina/JEV PoC branch and artifacts
- QAT/vocab/layer ablations

They remain valuable evidence.

Do not use them as the new production baseline without revalidation.

## 16. Restart phases

### Phase 0 — Forensic closure

Before training a new model:

- measure the v1 train/runtime formatting mismatch
- fix/measure target-segment logging mismatch
- document current rewriter ordering and user-history interaction
- reproduce the 30M production latency baseline under controlled conditions

No new model training in this phase.

### Phase 1 — Production contract and Dataset v2

Build:

- canonical context function
- actual Mozc top-K extraction with metadata
- document-disjoint data split
- dataset statistics and leak checks
- Mozc-only baseline evaluator

### Phase 2 — Rebuild 30M baseline

Train/evaluate ModernBERT-ja-30M on Dataset v2.

Goal: demonstrate a reliable positive delta vs real Mozc on unseen documents without unacceptable regressions.

### Phase 3 — Page-wise small model

Implement and compare a candidate-page model against the 30M cross encoder.

Adopt it only if quality/latency is better on the same dataset and runtime contract.

### Phase 4 — Strong teacher

Evaluate Sarashina/JEV or another strong Japanese model as a teacher/upper bound.

Distill only if the teacher adds useful, generalizable corrections.

### Phase 5 — Mozc fusion and user-history integration

Tune score fusion, confidence gating and history protection.

### Phase 6 — Real-use replay

Run privacy-safe replay and inspect helped/hurt examples.

No release until this is stable.

### Phase 7 — Optimization

Only now:

- quantization
- pruning
- vocabulary reduction
- architecture reduction
- packaging optimization

## 17. Definition of success

A v2 release is successful when it is:

- better than Mozc on real, unseen input
- conservative when uncertain
- strongly protected against harmful overwrites
- respectful of user history
- low-latency on CPU
- local/offline by default
- reproducible
- measurable end to end
- easy to disable/fail-safe

The target is not "the smartest standalone model."

The target is:

> Mozc remains the reliable IME, and the contextual model makes a small, measurable, safe improvement exactly where Mozc is weak.
