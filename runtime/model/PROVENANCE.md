# MozcIME AI v1.0 model provenance

- Base model: `sbintuitions/modernbert-ja-30m`
- Base revision family: ModernBERT-Ja 30M
- Base license: MIT (`MODEL_LICENSE.txt`)
- Fine-tuning task: Mozc N-best contextual cross-encoder ranking
- Fine-tuning dataset: public-source contextual dataset assembled from the
  licensed/public-domain sources documented in `Mozc-Ai-Training`
- Private usage fine-tune: **not included**
- Export: ONNX fp32 with tokenizer and a fixed margin policy

The distributable v1.0 model is the public-data `track30m_ctx` model. The
separate `usage30m_v1` model and all personal conversion logs remain local and
must never be copied into this directory or uploaded to GitHub.
