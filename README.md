# Mozc AI

Mozc AI は、Mozc に日本語文脈リランカーを統合した Windows 向けIMEです。
v1.0.0 は Mozc、ローカル推論ランタイム、ONNXモデル、トークナイザーを1つの
MSIに収録しており、Ollama、Python、クラウドAPIはインストール後に不要です。

## v1.0.0 の要点

- 配布物: `MozcAI-1.0.0-x64.msi`
- 製品名: `Mozc AI`
- 推論先: `127.0.0.1:17890` のローカルプロセスのみ
- モデル: `sbintuitions/modernbert-ja-30m` を基にした公開データ版
  `track30m_ctx` クロスエンコーダー
- 失敗時: AI処理をスキップし、Mozc本来の候補順を維持
- 個人利用ログ・private usageモデル: 配布物、リポジトリ、GitHub Releaseに不収録

旧版に存在したDeepSeek/OpenAI互換バックエンドとOllamaバックエンドのソースは
履歴参照用に残っていますが、v1.0.0のサーバーには登録・リンクされません。

## インストール

1. [Releases](https://github.com/shumaimai/Mozc-Ai/releases) から
   `MozcAI-1.0.0-x64.msi` を取得します。
2. MSIを実行し、Windowsの確認に従います。
3. インストール完了後に再起動を求められた場合は、Windowsを再起動します。

既存のMozcは同じPC上のMozc AIへ移行されます。インストール先の実体は、Mozcの
TIP登録との互換性を保つため `C:\Program Files\Mozc` です。

## アーキテクチャ

```text
キー入力
  -> Mozc変換（N-best候補）
  -> 使用条件・文脈ガード
  -> 127.0.0.1 の常駐ONNXリランカー
  -> マージン条件を満たす場合だけ候補順を変更
  -> 候補表示

デーモン停止・通信失敗・200ms超過
  -> Mozc本来の候補順をそのまま使用
```

ランタイムはloopback以外へのbindを拒否します。既定では入力文・候補・変換結果の
通信ログを保存しません。クラウド推論も行いません。

## ソースからMSIを作る

Windows 11 x64、Visual Studio 2022 Build Tools、Git、Python 3.12、Bazelisk、
Git LFSが必要です。

```powershell
git lfs pull
./scripts/package_windows.ps1
```

再現性のため、Mozcの基準コミットは
`3f235b4eb6fcff7d14ef5f0fb8ee56de7ee4c732` に固定しています。
成果物は `dist/MozcAI-1.0.0-x64.msi` に生成されます。

既に依存関係・Qt・ランタイムを構築済みの場合:

```powershell
./scripts/package_windows.ps1 -SkipDeps -SkipQt -SkipRuntime
```

## テスト

統合スクリプトが生成したMozcツリーでリランカーのC++テストを実行できます。

```powershell
cd .mozc-build/mozc/src
bazelisk test //rewriter:rerank_rewriter_test --config release_build
```

ランタイムは固定の合成入力だけを使って疎通確認できます。

```powershell
bazelisk build //client:runtime_smoke_client --config release_build
./bazel-bin/client/runtime_smoke_client.exe
```

## 設定とプライバシー

通常は環境変数を設定する必要はありません。管理者向けの主な切替は次の通りです。

- `MOZC_RERANK_ENABLED=0`: AIリランクを停止
- `MOZC_RERANK_DAEMON_ADDR=127.0.0.1:17890`: 既定のローカル接続先
- `MOZC_RERANK_TIMEOUT_MS=200`: 既定タイムアウト
- `MOZC_RERANK_LOG=<path>`: 明示的に指定した場合だけ変換内容をローカル保存

`MOZC_RERANK_LOG` には入力や候補が含まれるため、通常利用・共有PC・バグ報告では
設定しないでください。診断が必要な場合は、本文を記録しない
`MOZC_RERANK_DIAG_LOG` を使用できます。

ビルド・公開時には以下を必ず除外します。

- 個人の変換ログ、チャット、APIキー、トークン
- `usage30m_v1` を含むprivate usageモデル
- `data/` や `artifacts/private/` の個人利用データ

## モデルとライセンス

同梱モデルの由来は
[`runtime/model/PROVENANCE.md`](runtime/model/PROVENANCE.md)、基盤モデルの
ライセンス全文は [`runtime/model/MODEL_LICENSE.txt`](runtime/model/MODEL_LICENSE.txt)
に収録しています。v1.0.0では公開データ版モデルのみを配布します。

Mozc由来コードにはMozcのBSDライセンス、基盤モデルにはMITライセンス、その他の
依存関係には各同梱ライセンスが適用されます。

## リポジトリ構成

- `mozc_compat/`: upstream Mozcへ注入するローカルリランカーとテスト
- `runtime/`: loopback限定ONNX推論デーモン、モデル、由来情報
- `scripts/integrate_mozc.py`: Mozc変換パイプラインへの統合
- `scripts/integrate_mozc_installer.py`: 単一MSIへのランタイム統合
- `scripts/build_runtime_bundle.ps1`: Windowsランタイムの固定バンドル作成
- `scripts/package_windows.ps1`: v1.0.0 MSIの再現ビルド
- [Mozc-Ai-Training](https://github.com/shumaimai/Mozc-Ai-Training): データ生成、学習、評価、ONNX出力

## リリース

変更点と検証結果は
[`docs/RELEASE_NOTES_V1.0.0.md`](docs/RELEASE_NOTES_V1.0.0.md) を参照してください。
