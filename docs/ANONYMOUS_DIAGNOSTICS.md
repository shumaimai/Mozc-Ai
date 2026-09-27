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
# 1. 既存のMozc Low Integrity用ログディレクトリを使用。
#    TEMPや相対パスは使わず、絶対パスを設定する。
$diagDir = Join-Path (Split-Path $env:LOCALAPPDATA -Parent) 'LocalLow\Mozc'
$diagPath = Join-Path $diagDir 'ime_diag.jsonl'
Test-Path $diagDir   # Trueを確認
[Environment]::SetEnvironmentVariable('MOZC_RERANK_DIAG_LOG', $diagPath, 'User')
[Environment]::SetEnvironmentVariable('MOZC_RERANK_DIAG_SUMMARY_EVERY', '1', 'User')

# 2. Windowsからサインアウト→サインインしてから固定の合成入力を変換。
#    mozc_server.exe自身に環境変数が継承される必要がある。

# 3. 起動時点でstage=startup、その後capability→rewrite_enter→
#    guard_skip/rewrite、summaryの順に確認。
Get-Content $diagPath | Select-String '"stage":"summary"'
```

- 起動マーカー: `stage: startup`。呼び出し経路の確認用に`capability`と`rewrite_enter`も記録（入力文字列なし）。これらのマーカーは`rewrite_calls`には含まれません。
- イベント行: 完了した変換ごとに1行（`stage: rewrite` または `guard_skip`）
- サマリー行: 既定200件ごとに自動追記（`MOZC_RERANK_DIAG_SUMMARY_EVERY`で変更可能）
- Windowsで相対パスを指定した場合は、作業ディレクトリに依存しないようファイル名のみを`%LOCALAPPDATA%\..\LocalLow\Mozc\`配下に保存します。推奨は上記の絶対パス指定です。
- 出力先ディレクトリ作成・ファイルopen・書き込みの失敗は、Mozc標準ログに`RerankDiag file_error=<固定コード> os_code=<数値>`を一度だけ記録。ユーザー入力やファイルパスは記録しません。
- オフにする: 診断用2つのユーザー環境変数を削除してサインアウト→サインイン（またはサーバー再起動）。既存ログは必要に応じ手動削除。

## ログが出ない場合の切り分け

1. 標準の`mozc_server.exe.log`で`RerankDiag startup configured=...`を探す。存在しなければ、別のサーバーEXEやRerankRewriter未登録を疑い、インストール済みEXEのSHA256をMSIと照合する。
2. `configured=0`なら診断変数がサーバープロセスに届いていない。単にPowerShellやExplorerに存在するだけでは不十分。
3. `configured=1`なのにJSONLが無ければ`RerankDiag file_error=`を確認する。相対パスはLocalLowへリダイレクトされるので、元の`outputs`を探さない。
4. `startup`のみなら`capability`の有無を確認。`capability`のみなら変換要求タイプやRewriterの呼出しを確認。`rewrite_enter`はあるが`rewrite/guard_skip`が無い場合は早期return経路を確認。
5. 変換ログ`MOZC_RERANK_LOG`とは別物です。個人の変換文字列を含むログはGitHubへpushしない。

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
