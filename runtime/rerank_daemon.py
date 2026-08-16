#!/usr/bin/env python3
"""Local-only ONNX reranker daemon bundled with MozcIME AI.

The server binds to loopback, keeps the model resident, and speaks one-line
JSON over TCP. It intentionally has no network client and does not persist
request text.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import socketserver
import sys
import time
import unicodedata
from pathlib import Path
from typing import Any

import numpy as np
import onnxruntime as ort
import sentencepiece as spm

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 17890
DEFAULT_TAU = 2.5
DEFAULT_CAND_CAP = 30
DEFAULT_MAX_LEN = 128
_KYUJITAI_CHARS = frozenset("實舊讃與學體廣應藝縣擧瀧靜")
_SENT_END = re.compile(r"[。！？!?]")
_WIKI_EDIT = re.compile(r"\[\s*(?:edit|編集)\s*\]", re.IGNORECASE)
_WIKI_HEADING = re.compile(r"={2,}[^=\n]*={2,}")
_WIKI_LINK = re.compile(r"\[\[(?:[^|\]]+\|)?([^\]]+)\]\]")
_WIKI_REF = re.compile(r"\[\d+\]|<ref\b[^>]*>.*?</ref>", re.IGNORECASE | re.DOTALL)
_BULLET_MARK = re.compile(r"(?:^|\s)[*＊#＃・]+(?=\s|$)")
_EQ_RUN = re.compile(r"={2,}")
_MULTI_SPACE = re.compile(r"[ \t\u3000]+")


def runtime_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


def clean_context(text: str, max_chars: int = 50) -> str:
    if not text:
        return ""
    value = str(text).replace("\r\n", "\n").replace("\r", "\n")
    value = value.replace("\n", " ").replace("\t", " ")
    value = _WIKI_EDIT.sub("", value)
    value = _WIKI_HEADING.sub(" ", value)
    value = _WIKI_LINK.sub(r"\1", value)
    value = _WIKI_REF.sub("", value)
    value = _EQ_RUN.sub(" ", value)
    value = _BULLET_MARK.sub(" ", value)
    value = _MULTI_SPACE.sub(" ", value).strip(" \u3000")
    if not value:
        return ""
    last = -1
    for match in _SENT_END.finditer(value):
        last = match.end()
    sentence = (value[last:] if last >= 0 else value).lstrip(" \u3000")
    return sentence if len(sentence) <= max_chars else sentence[-max_chars:]


def normalize_reading(text: str) -> str:
    out: list[str] = []
    for char in unicodedata.normalize("NFKC", str(text or "")):
        code = ord(char)
        out.append(chr(code - 0x60) if 0x30A1 <= code <= 0x30F6 else char)
    return "".join(out)


def has_linguistic_content(text: str) -> bool:
    for char in text or "":
        code = ord(char)
        if (
            0x3040 <= code <= 0x30FF
            or 0x31F0 <= code <= 0x31FF
            or 0x3400 <= code <= 0x9FFF
            or 0xF900 <= code <= 0xFAFF
            or 0xFF66 <= code <= 0xFF9D
            or (char.isalpha() and not char.isdigit())
        ):
            return True
    return False


def is_junk_surface(surface: str) -> bool:
    value = surface or ""
    if not value or any(char in _KYUJITAI_CHARS for char in value):
        return True
    if all(0xFF61 <= ord(char) <= 0xFF9F for char in value):
        return True
    kana = 0
    other = 0
    for char in value:
        code = ord(char)
        if char == "ー" or 0x30A0 <= code <= 0x30FF or 0xFF66 <= code <= 0xFF9D:
            kana += 1
        elif not char.isspace():
            other += 1
    return kana > 0 and other == 0


def skip_reason(reading: str, context: str) -> str | None:
    if len(reading) <= 2:
        return "reading_too_short"
    if not context or not has_linguistic_content(context):
        return "context_empty_or_symbol"
    return None


def build_pair_text(reading: str, context: str, candidate: str) -> str:
    return f"読み: {reading}\n文脈: {context}\n候補: {candidate}"


class OrtScorer:
    def __init__(self, model: Path, tokenizer_dir: Path, max_len: int, intra: int):
        # The training tokenizer is LlamaTokenizer over this SentencePiece
        # model with BOS=1, EOS=2, PAD=3.  Direct SentencePiece encoding is
        # byte-for-byte identical for the plain reranker pair text and avoids
        # shipping the unrelated Transformers model registry.
        self.tokenizer = spm.SentencePieceProcessor(
            model_file=str(tokenizer_dir / "tokenizer.model")
        )
        self.max_len = max_len
        options = ort.SessionOptions()
        options.intra_op_num_threads = max(1, intra)
        options.inter_op_num_threads = 1
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        options.enable_mem_pattern = True
        options.enable_cpu_mem_arena = True
        options.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        self.session = ort.InferenceSession(
            str(model), sess_options=options, providers=["CPUExecutionProvider"]
        )

    def score(self, texts: list[str]) -> list[float]:
        rows = [
            [1] + self.tokenizer.encode(text, out_type=int)[: self.max_len - 2] + [2]
            for text in texts
        ]
        width = max(len(row) for row in rows)
        input_ids = np.full((len(rows), width), 3, dtype=np.int64)
        attention_mask = np.zeros((len(rows), width), dtype=np.int64)
        for index, row in enumerate(rows):
            input_ids[index, : len(row)] = row
            attention_mask[index, : len(row)] = 1
        result = self.session.run(
            None, {"input_ids": input_ids, "attention_mask": attention_mask}
        )[0]
        return [float(value) for value in np.asarray(result).reshape(-1).tolist()]


def rerank(req: dict[str, Any], scorer: OrtScorer, tau: float, cand_cap: int) -> dict[str, Any]:
    reading = normalize_reading(req.get("reading") or "")
    context = clean_context(req.get("context_prev") or req.get("context") or "")
    candidates = [str(value) for value in (req.get("nbest") or req.get("candidates") or []) if value]
    if not reading or not candidates:
        return {"ok": False, "ranked_surfaces": []}
    candidates = candidates[:cand_cap]
    mozc_top = candidates[0]
    reason = skip_reason(reading, context)
    if reason:
        return {
            "ok": True,
            "ranked_surfaces": candidates,
            "rerank_top1": mozc_top,
            "final_top1": mozc_top,
            "overwritten": False,
            "guard_skip": True,
            "reason": reason,
        }
    scores = scorer.score(
        [build_pair_text(reading, context, candidate) for candidate in candidates]
    )
    best_index = max(range(len(candidates)), key=lambda index: scores[index])
    rerank_top = candidates[best_index]
    margin = scores[best_index] - scores[0]
    overwrite = best_index != 0 and margin >= tau and not is_junk_surface(rerank_top)
    final_top = rerank_top if overwrite else mozc_top
    ranked_indices = sorted(range(len(candidates)), key=lambda index: scores[index], reverse=True)
    ranked = [final_top] + [
        candidates[index] for index in ranked_indices if candidates[index] != final_top
    ]
    return {
        "ok": True,
        "ranked_surfaces": ranked,
        "rerank_top1": rerank_top,
        "final_top1": final_top,
        "overwritten": overwrite,
        "guard_skip": False,
        "margin": margin,
    }


class Handler(socketserver.StreamRequestHandler):
    def handle(self) -> None:
        server: "RerankServer" = self.server  # type: ignore[assignment]
        while raw := self.rfile.readline():
            started = time.perf_counter()
            try:
                req = json.loads(raw.decode("utf-8"))
                if str(req.get("op") or "").lower() == "ping":
                    response: dict[str, Any] = {"ok": True, "op": "pong"}
                else:
                    response = rerank(req, server.scorer, server.tau, server.cand_cap)
                    response["daemon_ms"] = round(
                        (time.perf_counter() - started) * 1000.0, 3
                    )
            except Exception as exc:  # fail-safe: Mozc retains its native order
                response = {"ok": False, "error_type": type(exc).__name__, "ranked_surfaces": []}
            self.wfile.write(
                (json.dumps(response, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
            )
            self.wfile.flush()


class RerankServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address: tuple[str, int], scorer: OrtScorer, tau: float, cand_cap: int):
        super().__init__(address, Handler)
        self.scorer = scorer
        self.tau = tau
        self.cand_cap = cand_cap


def main() -> int:
    base = runtime_dir()
    parser = argparse.ArgumentParser(description="MozcIME AI local reranker v1.0")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--model", default=str(base / "model" / "cross_encoder_fp32.onnx"))
    parser.add_argument("--tokenizer", default=str(base / "model" / "tokenizer"))
    parser.add_argument("--policy", default=str(base / "model" / "margin_policy.json"))
    parser.add_argument("--intra-op", type=int, default=max(1, os.cpu_count() or 1))
    args = parser.parse_args()
    if args.host not in {"127.0.0.1", "localhost", "::1"}:
        print("refusing non-loopback bind", file=sys.stderr)
        return 2
    policy = json.loads(Path(args.policy).read_text(encoding="utf-8"))
    tau = float(policy.get("tau", DEFAULT_TAU))
    cand_cap = int(policy.get("cand_cap", DEFAULT_CAND_CAP))
    max_len = int(policy.get("max_len", DEFAULT_MAX_LEN))
    scorer = OrtScorer(Path(args.model), Path(args.tokenizer), max_len, args.intra_op)
    server = RerankServer((args.host, args.port), scorer, tau, cand_cap)
    print(f"MozcIME AI v1.0 ready on {args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
