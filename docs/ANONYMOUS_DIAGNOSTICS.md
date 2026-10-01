# 匿名診断機能と合成評価セット

> **プライバシー原則**: 診断には読み・文脈・候補・周辺テキストなど
> ユーザー入力文字列を一切記録しません。記録するのは固定トークン、
> バイト/カウント数値、レイテンシ、匿名IDのみです。モジュールのAPIは
> 文字列を受け取る口が存在しないため、ソースレベルで漏洩不可能です。

## 診断項目（要求仕様との対応）

| # | 要求 | 実装 |
|---|------|------|
| 1 | C++ Rewrite呼び出し回数 | `rewrite_calls`（全イベント累算） |
| 2 | guard skip回数と理由 | `guard_skips` + 理由別内訳 `skip_reading_too_short` / `skip_context_empty_or_symbol` / `skip_reading_not_eligible` |
| 3 | daemon成功・失敗・timeout | `daemon_ok` / `daemon_fail` / `daemon_timeout` |
| 4 | overwrite回数 | `overwrites`（junk/保護override後の最終決定） |
| 5 | C++処理時間 p50/p95/p99 | `cpp_ms_p50/p95/p99`（Rewrite開始→完了のラウンドトリップ実測） |
| 6 | daemon推論時間 p50/p95/p99 | `infer_ms_p50/p95/p99`（daemonが報告する純スコアリング時間） |
| 7 | 実行中モデルSHA256とguard mode | `model_sha256`（daemonが実際にロードしたONNXをハッシュ報告、sidecarより優先）+ `guard_mode`（env > policy > 既定 "safety"） |
| 8 | 匿名リクエスト対応ID | `session_id`（プロセスローカル乱数）+ `req_id`（単調増加）+ daemon `req_id` echo → `round_trip:true` で C++→daemon→C++ の完全往復を証明 |

## 使い方

```powershell
# 1. 診断ログ有効化（mozc_server再起動で反映）
[Environment]::SetEnvironmentVariable("MOZC_RERANK_DIAG_LOG", "$env:TEMP\mozc_diag.jsonl", "User")

# 2. テストMSIインストール後、いつもの変換操作を数分行う

# 3. サマリー確認
Get-Content $env:TEMP\mozc_diag.jsonl | Select-String '"stage":"summary"'
```

- イベント行: 変換ごとに1行（`stage: rewrite` または `guard_skip`）
- サマリー行: 既定200イベントごとに自動追記（`MOZC_RERANK_DIAG_SUMMARY_EVERY`で変更可能）
- オフにする: 環境変数を削除してmozc_server再起動。ログは残るので手動削除してください

## 合成評価セット

`evaluation/synthetic_context_set.json` — 固定12ケースの文脈依存変換
（「きしゃ」駅/新聞、「じしん」速報/個人、「きかん」病院/応募 等）。
すべて架空の入力で、実ユーザーデータとは無関係です。

```powershell
# daemon起動状態で実行（実モデルによる本評価）
python evaluation\run_synthetic_eval.py --daemon 127.0.0.1:17890

# レポート保存
python evaluation\run_synthetic_eval.py --daemon 127.0.0.1:17890 --out eval_report.jsonl
```

各ケース行に `mozc_top1 / neural_top1 / margin / final_top1 / overwritten /
infer_ms / round_trip` を出力し、`expect_neural_top1` との一致を `match` で
示します。サマリー行に一致数・overwrite数・推論時間パーセンタイル・
`model_sha256` を記録します。

**旧ログとの関係**: この評価は固定合成入力のみを使用し、8月の実機ログとは
完全に独立しています。実機テスト結果と混同しないでください。

## 実装の所在

- C++: `mozc_compat/rerank_diag.h/.cc`（`MOZC_RERANK_DIAG_LOG` が未設定なら完全無効）
- daemon: `runtime/rerank_daemon.py`（`req_id` echo・`infer_ms`・`model_sha256` を応答）
- 統合: `scripts/integrate_mozc.py` がdiag モジュールをMozcツリーへコピー
- テスト: `mozc_compat/rerank_rewriter_test.cc`（カウンタ差分・サマリー構造・プライバシー回帰ガード）
- スモーク: `scripts/windows_smoke.ps1` とCIのdaemonスモークがreq_id echo・`infer_ms`・`model_sha256` を検証
