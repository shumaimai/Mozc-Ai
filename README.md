# AI統合 Mozc IME

Mozc（Google日本語入力）にローカルAI（Ollama）を統合し、文脈に基づいた変換候補を追加するIME。

**[📖 詳細なセットアップガイド（Getting Started）](docs/GETTING_STARTED.md)**

## 最重要設計原則

```
╔══════════════════════════════════════════════════════════════════╗
║  IMEは絶対にフリーズしない                                        ║
║  ────────────────────────────────────────────────────────────────║
║  • AI処理が遅くても、エラーでも、IMEは即座に応答する              ║
║  • 全てのAI処理は「あれば嬉しい」程度の位置づけ                   ║
║  • 1msでも長く待たせるくらいなら、AI候補は諦める                  ║
╚══════════════════════════════════════════════════════════════════╝
```

## クイックスタート

### Windows（推奨: Release からインストール）

1. [GitHub Releases](https://github.com/shumaimai/Mozc-Ai/releases) から `MozcAI64.msi` をダウンロード
2. MSI を実行してインストール
3. [Ollama](https://ollama.ai) をインストールし `ollama pull gemma3:1b`

### 開発者向け（ソースからビルド）

### 1. 前提条件をインストール

**Windows:**
```powershell
# Chocolateyを使用
choco install bazelisk git visualstudio2022community
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt install -y build-essential git
curl -Lo /usr/local/bin/bazel https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64
chmod +x /usr/local/bin/bazel
```

### 2. Ollamaをセットアップ

```bash
# インストール (Linux/macOS)
curl -fsSL https://ollama.ai/install.sh | sh

# Windowsの場合は https://ollama.ai からダウンロード

# モデルをダウンロード
ollama pull gemma3:1b
```

### 3. ビルドと実行

```bash
# クローン
git clone <repository-url> ai_mozc
cd ai_mozc

# ビルド
./scripts/build.sh        # Linux/macOS
.\scripts\build.ps1       # Windows

# テスト
./scripts/build.sh --test # Linux/macOS
.\scripts\build.ps1 -Test # Windows
```

## アーキテクチャ

```
┌─────────────────────────────────────────────────────────────────┐
│                        IME メインスレッド                        │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐  │
│  │ 入力受付     │───▶│ Mozc変換    │───▶│ 候補表示            │  │
│  └─────────────┘    └─────────────┘    └─────────────────────┘  │
│         │                                        ▲               │
│         │ 入力通知（非ブロッキング）               │ キャッシュ参照 │
│         ▼                                        │               │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                    AI候補キャッシュ                          ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ 結果書き込み（非同期）
┌─────────────────────────────────────────────────────────────────┐
│                        AI ワーカースレッド                       │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐  │
│  │ 入力キュー   │───▶│ AI処理      │───▶│ 結果キャッシュ      │  │
│  └─────────────┘    └─────────────┘    └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## ディレクトリ構成

```
ai_mozc/
├── src/
│   ├── ai/                          # AI関連モジュール
│   │   ├── ai_config.h/cc           # 設定マネージャー
│   │   ├── ai_candidate_cache.h/cc  # キャッシュ
│   │   ├── ai_worker.h/cc           # ワーカースレッド
│   │   ├── ai_backend.h             # バックエンド抽象化
│   │   ├── ollama_backend.cc        # Ollamaバックエンド
│   │   ├── mock_backend.cc          # モックバックエンド
│   │   ├── ai_logger.h/cc           # ロギング
│   │   └── BUILD
│   └── rewriter/
│       ├── rewriter_interface.h     # Mozcインターフェース
│       ├── ai_rewriter.h/cc         # AIリライター
│       └── BUILD
├── docs/                             # ドキュメント
│   ├── GETTING_STARTED.md           # 入門ガイド
│   ├── BUILD_GUIDE.md               # ★ビルド詳細・成果物の場所
│   ├── TESTING_GUIDE.md             # ★テスト・動作確認ガイド
│   ├── MOZC_INTEGRATION.md          # Mozc統合ガイド
│   └── BUILD_ERRORS.md              # エラー・修正記録
├── scripts/                          # スクリプト
│   ├── build.ps1                    # Windows用ビルドスクリプト
│   ├── build.sh                     # Linux用ビルドスクリプト
│   ├── install.ps1                  # Windowsインストーラー
│   └── install.sh                   # Linuxインストーラー
├── MODULE.bazel                      # Bazel 8+ 依存関係
├── WORKSPACE                         # Bazel 7以前用
├── .bazelrc                          # Bazelオプション
│
└── bazel-bin/                        # ★ビルド成果物（自動生成）
    └── src/
        ├── ai/                       # libai.a, テスト実行ファイル
        └── rewriter/                 # libai_rewriter.a
```

## 設定ファイル

設定ファイルの場所:
- **Windows**: `%LOCALAPPDATA%\Google\Mozc\ai_config.json`
- **Linux/macOS**: `~/.mozc/ai_config.json`

### 設定例

```json
{
  "enabled": true,
  "backend_type": "ollama",
  "ollama_endpoint": "http://localhost:11434",
  "ollama_model": "gemma3:1b",
  "connect_timeout_ms": 50,
  "request_timeout_ms": 500,
  "max_wait_ms": 600,
  "cache_ttl_seconds": 60,
  "cache_max_entries": 100,
  "history_size": 5,
  "log_level": "info",
  "log_ai_communication": false,
  "disable_ai": false,
  "use_mock": false
}
```

## フリーズ防止機能

| レイヤー | 手法 | 詳細 |
|---------|------|------|
| 設計 | 完全非同期 | AI処理は別スレッド、結果は次回変換で使用 |
| タイムアウト | 二重タイムアウト | 接続50ms + 処理500ms = 最大550ms |
| フォールバック | 即時降格 | 少しでも異常があればAI機能を即座にスキップ |
| オーバーフロー | キュー制限 | 最大10リクエスト、超過分は破棄 |

## デバッグ・ログ

### ログファイルの場所
- **Windows**: `%LOCALAPPDATA%\Google\Mozc\ai_log.txt`
- **Linux/macOS**: `~/.mozc/ai_log.txt`

### ログの確認

```bash
# Linux/macOS
tail -f ~/.mozc/ai_log.txt

# Windows (PowerShell)
Get-Content -Wait $env:LOCALAPPDATA\Google\Mozc\ai_log.txt
```

### デバッグモードの有効化

設定ファイルで以下を変更:
```json
{
  "log_level": "debug",
  "log_ai_communication": true
}
```

### ビルド時のログ出力

ビルド中のエラーは標準エラー出力（stderr）に `[AI-Mozc]` プレフィックス付きで出力されます:
```
[AI-Mozc] Initializing AIConfigManager...
[AI-Mozc] Loading config from: /home/user/.mozc/ai_config.json
[AI-Mozc] Config loaded successfully
```

## トラブルシューティング

### Ollamaに接続できない

```bash
# サービスの状態確認
curl http://localhost:11434/api/tags

# サービスを起動
ollama serve
```

### AI候補が表示されない

1. Ollamaが起動しているか確認: `curl http://localhost:11434/api/tags`
2. モデルがダウンロード済みか確認: `ollama list`
3. 設定ファイルで `enabled: true` になっているか確認
4. ログファイルを確認

### ビルドエラー

詳細は [Getting Started Guide](docs/GETTING_STARTED.md#トラブルシューティング) を参照してください。

## テスト

```bash
# 全テスト実行
bazelisk test //src/ai:all //src/rewriter:all

# 個別テスト
bazelisk test //src/ai:ai_candidate_cache_test
bazelisk test //src/ai:ai_config_test
bazelisk test //src/ai:ai_backend_test
bazelisk test //src/ai:ai_worker_test
bazelisk test //src/rewriter:ai_rewriter_test

# 詳細出力
bazelisk test --test_output=all //src/ai:all
```

## ライセンス

このプロジェクトはMozcと同じライセンス（BSD 3-Clause License）の下で公開されています。

## 貢献

バグ報告や機能リクエストはIssueをお開きください。
プルリクエストも歓迎します。

## 関連ドキュメント

### プロジェクトドキュメント

| ドキュメント | 内容 |
|-------------|------|
| [入門ガイド](docs/GETTING_STARTED.md) | 環境構築、セットアップ手順 |
| [ビルドガイド](docs/BUILD_GUIDE.md) | ★ビルド詳細、成果物の場所、ターゲット一覧 |
| [テストガイド](docs/TESTING_GUIDE.md) | ★テスト実行、動作確認手順 |
| [Mozc統合ガイド](docs/MOZC_INTEGRATION.md) | Mozcソースコードへの統合方法 |
| [Windowsインストーラー](docs/WINDOWS_INSTALLER.md) | MSI (`MozcAI64.msi`) のビルド・配布手順 |
| [エラー・修正記録](docs/BUILD_ERRORS.md) | 既知のエラーと解決策 |

### 外部リンク

- [Mozc公式リポジトリ](https://github.com/google/mozc)
- [Ollama公式サイト](https://ollama.ai)
- [Bazel公式ドキュメント](https://bazel.build/)

## 謝辞

- Google Mozc チーム
- Ollama プロジェクト
