# MozcAI public-data listwise model — 2026-09-30

- Base: `sbintuitions/modernbert-ja-30m`, 10 transformer layers, MIT license (`MODEL_LICENSE.txt`).
- Training repository: `Mozc-Ai-Training-v2`, branch `v2/contextual-ranking-reset`;
  entry point `scripts/modal_listwise_optimization.py`, implementation `tools/rerank/listwise.py`.
- Dataset: frozen public `contextual_ranking_v2_production_runtime_context`;
  train SHA256 `b8ae9a55f2bb083fe0b15eb8f79be19d3bdfaeb16de85cbc00da64cc7c844d29`,
  validation SHA256 `087008d04e769006561721ec10306c7edb48811abf645bc9d2f7cbc1023a628b`.
  47,589 source training rows, 31,665 NEURAL_ELIGIBLE rows, 30,411 usable groups
  with at least two unique surfaces. Gold is never injected.
- Recipe: group-normalized listwise CE + 0.15 candidate BCE; fresh 30M initialization,
  seed 20260930, 384 candidate sequences per packed batch, learning rate 2e-5.
  The first epoch is the frozen teacher. The selected model continues from it for
  one epoch with lr 2e-6 and distribution KL weight 2, temperature 2.
- Selected Modal artifact: `mozc-artifacts:/optimization_20260930/listwise_refine/epoch1`;
  checkpoint SHA256 `ea935215b2c0148bae56277ddd49b3735c04f678b9c996f5d2f273ac281bbd87`.
- Input: canonical newline-separated reading / context / candidate fields. Training
  and inference normalize reading and apply the daemon context cleaner, including
  trimming spaces left by the C++ 50-character clipping step.
- Export: FP32 ONNX, opset 17, TorchScript exporter (`dynamo=False`), dynamic batch
  and sequence dimensions. ONNX SHA256:
  `b2971435a9934c71d8fecbdfd50611c355c4dd12cba32b3244073e4fb8702a6c`.
- CPU parity: 800 conversions / 9,158 texts; tokenizer IDs all identical, argmax and
  tau=1.5 decisions all identical to PyTorch FP32; maximum score difference 2.13e-5.
  Tested with ORT 1.30.0 and SentencePiece 0.2.2.
- Policy: tau 1.5, 30 candidates, max_len 128, context 50 characters, safety guard,
  200ms deadline, intra_op 4. CLI `--intra-op` overrides policy. Identical candidate
  texts share one forward result within a conversion; original candidate order and
  multiplicity are restored for the gate.
- CPU validation: 5,974 groups, guarded Hit@1 77.12%, helped 180, hurt 29;
  baseline 76.36%, helped 153, hurt 47. These are validation results without a
  transport deadline, not field-test accuracy. CPU/TCP results are in
  `MozcAI_improvement_20260930.md` at the project root.
- INT8, 7-layer distillation, and alternative checkpoints were measured. FP32 was
  selected to retain accuracy. Quantized weights are not bundled.
- `final_test` was not opened, scored, or mounted. Personal usage logs were not used
  in training and never uploaded. Only the public corpus and code went to Modal.
  The authored short-context diagnostic was not used for training.

`MODEL_MANIFEST.json` stores training identity. `SHA256SUMS` pins every asset; the
Windows bundle build and MSI audit verify them. This updates the source payload;
an existing installed MSI is unchanged until rebuilt and installed.
