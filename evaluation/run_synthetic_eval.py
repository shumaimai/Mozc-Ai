#!/usr/bin/env python3
"""Run the fixed synthetic context evaluation set against the rerank daemon.

Everything in the evaluation set is invented synthetic text — nothing here is
user data, and the script records only model decisions (top1 / margin /
overwrite / latency).  It never reads or writes user input files.

Modes:
  --daemon 127.0.0.1:17890  Query a running daemon (real model).
  --offline                 No network: run with the deterministic offline
                            scorer to validate report structure and the
                            expected-top1 table without onnxruntime.

Output: JSON report on stdout (or --out FILE), JSONL per-case lines with
stage=case plus one stage=summary line, mirroring the runtime diag schema.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import socket
import sys
import time
from pathlib import Path

SET_PATH = Path(__file__).resolve().parent / "synthetic_context_set.json"


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def clean_context(text: str, max_chars: int = 50) -> str:
    # Mirrors runtime/rerank_daemon.py clean_context (fixed synthetic input,
    # so plain truncation suffices for this set).
    value = str(text or "").replace("\r\n", "\n").replace("\r", "\n")
    value = value.replace("\n", " ").replace("\t", " ").strip()
    return value[-max_chars:]


class DaemonClient:
    def __init__(self, addr: str, timeout_ms: int = 120000):
        host, _, port = addr.rpartition(":")
        if not host or not port:
            raise ValueError(f"bad daemon addr: {addr}")
        self.host = host
        self.port = int(port)
        self.timeout_ms = timeout_ms
        self.next_req_id = 0

    def request(self, payload: dict) -> dict:
        self.next_req_id += 1
        payload = dict(payload)
        payload["req_id"] = self.next_req_id
        raw = (json.dumps(payload, ensure_ascii=False) + "\n").encode("utf-8")
        with socket.create_connection(
            (self.host, self.port), timeout=self.timeout_ms / 1000.0
        ) as sock:
            sock.settimeout(self.timeout_ms / 1000.0)
            sock.sendall(raw)
            reader = sock.makefile("rb")
            line = reader.readline()
        if not line:
            raise RuntimeError("empty daemon response")
        return json.loads(line.decode("utf-8"))

    def ping(self) -> dict:
        return self.request({"op": "ping"})


class OfflineScorer:
    """Deterministic stand-in scorer for structural validation only.

    It does NOT evaluate language quality; it just exercises the full report
    path where no ONNX runtime is available (e.g. sandboxed CI lint steps).
    Scores are stable pseudo-values derived from case id + candidate order.
    """

    def score(self, texts: list[str]) -> list[float]:
        return [
            1.0 - 0.01 * index - (len(text) % 7) * 0.001
            for index, text in enumerate(texts)
        ]


def run_case(case: dict, client: DaemonClient, tau: float) -> dict:
    context = clean_context(case["context_prev"])
    started = time.perf_counter()
    resp = client.request(
        {
            "reading": case["reading"],
            "context_prev": context,
            "nbest": case["candidates"],
        }
    )
    cpp_ms = (time.perf_counter() - started) * 1000.0
    mozc_top1 = case["candidates"][0]
    neural_top1 = resp.get("rerank_top1", "")
    final_top1 = resp.get("final_top1", "")
    expected = case.get("expect_neural_top1", "")
    round_trip = resp.get("req_id") == client.next_req_id
    return {
        "stage": "case",
        "case_id": case["id"],
        "mozc_top1": mozc_top1,
        "neural_top1": neural_top1,
        "margin": resp.get("margin"),
        "final_top1": final_top1,
        "overwritten": resp.get("overwritten"),
        "guard_skip": resp.get("guard_skip"),
        "reason": resp.get("reason", ""),
        "infer_ms": resp.get("infer_ms"),
        "daemon_ms": resp.get("daemon_ms"),
        "cpp_ms": round(cpp_ms, 3),
        "model_sha256": resp.get("model_sha256", ""),
        "round_trip": round_trip,
        "expect_neural_top1": expected,
        "match": neural_top1 == expected if expected else None,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--daemon", default="", help="host:port of running daemon")
    parser.add_argument("--offline", action="store_true", help="no network; structural validation")
    parser.add_argument("--out", default="", help="write JSONL report here (default stdout)")
    parser.add_argument("--set", dest="set_path", default=str(SET_PATH))
    args = parser.parse_args()

    spec = json.loads(Path(args.set_path).read_text(encoding="utf-8"))
    tau = float(spec.get("tau", 2.5))

    lines: list[str] = []

    def emit(record: dict) -> None:
        lines.append(json.dumps(record, ensure_ascii=False, separators=(",", ":")))

    if args.offline:
        # Structural validation with the deterministic offline scorer.
        scorer = OfflineScorer()
        model_sha = "offline"
        results = []
        for case in spec["cases"]:
            context = clean_context(case["context_prev"])
            texts = [
                f"読み: {case['reading']}\n文脈: {context}\n候補: {c}"
                for c in case["candidates"]
            ]
            scores = scorer.score(texts)
            best = max(range(len(scores)), key=lambda i: scores[i])
            margin = scores[best] - scores[0]
            overwrite = best != 0 and margin >= tau
            record = {
                "stage": "case",
                "case_id": case["id"],
                "mozc_top1": case["candidates"][0],
                "neural_top1": case["candidates"][best],
                "margin": round(margin, 4),
                "final_top1": case["candidates"][best] if overwrite else case["candidates"][0],
                "overwritten": overwrite,
                "guard_skip": False,
                "reason": "",
                "infer_ms": 0.0,
                "daemon_ms": 0.0,
                "cpp_ms": 0.0,
                "model_sha256": model_sha,
                "round_trip": True,
                "expect_neural_top1": case.get("expect_neural_top1", ""),
                "match": None if not case.get("expect_neural_top1") else case["candidates"][best] == case["expect_neural_top1"],
            }
            results.append(record)
            emit(record)
        summary = {
            "stage": "summary",
            "mode": "offline",
            "cases": len(results),
            "matches": sum(1 for r in results if r["match"]),
            "overwrites": sum(1 for r in results if r["overwritten"]),
            "model_sha256": model_sha,
        }
        emit(summary)
    else:
        if not args.daemon:
            parser.error("either --daemon host:port or --offline is required")
        client = DaemonClient(args.daemon)
        pong = client.ping()
        if not pong.get("ok") or pong.get("op") != "pong":
            print("EVAL_FAIL daemon ping failed", file=sys.stderr)
            return 2
        model_sha = pong.get("model_sha256", "")
        results = []
        for case in spec["cases"]:
            record = run_case(case, client, tau)
            results.append(record)
            emit(record)
        summary = {
            "stage": "summary",
            "mode": "daemon",
            "cases": len(results),
            "matches": sum(1 for r in results if r["match"]),
            "overwrites": sum(1 for r in results if r["overwritten"]),
            "round_trips": sum(1 for r in results if r["round_trip"]),
            "model_sha256": model_sha,
            "infer_ms_p50": _percentile([r["infer_ms"] or 0.0 for r in results], 0.5),
            "infer_ms_p95": _percentile([r["infer_ms"] or 0.0 for r in results], 0.95),
            "infer_ms_p99": _percentile([r["infer_ms"] or 0.0 for r in results], 0.99),
        }
        emit(summary)

    report = "\n".join(lines) + "\n"
    if args.out:
        Path(args.out).write_text(report, encoding="utf-8")
    else:
        sys.stdout.write(report)

    if not args.offline:
        summary = json.loads(lines[-1])
        if summary["round_trips"] != summary["cases"]:
            print("EVAL_WARN some requests failed req_id round trip", file=sys.stderr)
    return 0


def _percentile(values: list[float], frac: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = int(frac * (len(ordered) - 1))
    return round(ordered[index], 3)


if __name__ == "__main__":
    raise SystemExit(main())
