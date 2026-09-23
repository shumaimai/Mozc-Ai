# MozcIME AI v1.0 model provenance

- Base model: `sbintuitions/modernbert-ja-30m`
- Base revision family: ModernBERT-Ja 30M
- Base license: MIT (`MODEL_LICENSE.txt`)
- Fine-tuning task: Mozc N-best contextual cross-encoder ranking
- Fine-tuned model: **Phase 2 `format_v2` frozen cross-encoder**
  (`shumaimai/Mozc-Ai-Training` branch `v2/contextual-ranking-reset`,
  commit `b7ae817`, frozen record `FROZEN_RECORD.json`,
  checkpoint sha256 `0b74f6f9...`)
- Fine-tuning dataset: production Dataset v2
  (`contextual_ranking_v2_production_runtime_context`, public sources only;
  train `b8ae9a55...`, validation `087008d0...`)
- Private usage fine-tune: **not included**
- Export: ONNX fp32, opset 17, TorchScript exporter (`dynamo=False` — torch
  2.14 dynamo output is rejected by ONNX Runtime). ONNX sha256
  `00738ecfd6ee63e25cbc9cb6cfdb2976c381c930e423fc1401d356ef62a04a25`.
  Verified against the PyTorch training forward pass on 9,158 texts /
  800 groups: max score diff 4.15e-05, 800/800 argmax and tau=2.5 final
  selection identical; tokenizer IDs 9,158/9,158 exact vs the training
  tokenizer; CPU resident-daemon p95 78.0 ms with 0/5,974 timeouts at the
  200 ms deadline (details: `docs/contextual_ranking_v2/PHASE2_PARITY_REPORT.md`
  in the training repo).
- Runtime policy (`margin_policy.json`): tau 2.5, cand_cap 30, max_len 128,
  timeout 200 ms, context clip 50, `guard_mode: safety` (MOZC_RERANK_GUARD_MODE
  env overrides the policy file; built-in default is also safety).

The distributable v1.0 model is the public-data Phase 2 `format_v2` model.
The separate `usage30m_v1` model and all personal conversion logs remain local
and must never be copied into this directory or uploaded to GitHub.

`SHA256SUMS` pins every file in this directory; `scripts/build_runtime_bundle.ps1`
and the CI MSI audit fail the build on any mismatch.
