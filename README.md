# AI統合 Mozc IME

Mozc（Google日本語入力）にローカルAI（Ollama）を統合し、文脈に基づいた変換候補を追加するIME。

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
src/
├── ai/                          # AI関連モジュール
│   ├── ai_config.proto          # 設定定義
│   ├── ai_config.h/cc           # 設定マネージャー
│   ├── ai_candidate_cache.h/cc  # キャッシュ
│   ├── ai_worker.h/cc           # ワーカースレッド
│   ├── ai_backend.h             # バックエンド抽象化
│   ├── ollama_backend.cc        # Ollamaバックエンド
│   ├── mock_backend.cc          # モックバックエンド
│   ├── ai_logger.h/cc           # ロギング
│   └── BUILD
├── rewriter/
│   ├── rewriter_interface.h     # Mozcインターフェース
│   ├── ai_rewriter.h/cc         # AIリライター
│   └── BUILD
└── ...
```

## 前提条件

### Windows

- Windows 10/11
- Visual Studio 2022（C++ワークロード）
- Bazelisk (`choco install bazelisk`)
- Ollama (https://ollama.ai)

### Linux

- GCC 9+ または Clang 10+
- Bazelisk
- Ollama

## ビルド方法

### Windows (PowerShell)

```powershell
# デバッグビルド
.\scripts\build.ps1

# リリースビルド
.\scripts\build.ps1 -Release

# テスト付きビルド
.\scripts\build.ps1 -Test

# クリーンビルド
.\scripts\build.ps1 -Clean
```

### Linux/macOS

```bash
# デバッグビルド
./scripts/build.sh

# リリースビルド
./scripts/build.sh --release

# テスト付きビルド
./scripts/build.sh --test

# クリーンビルド
./scripts/build.sh --clean
```

## Ollama設定

1. Ollamaをインストール
2. モデルをダウンロード:
   ```bash
   ollama pull mistral:7b
   ```
3. Ollamaを起動（自動起動されない場合）:
   ```bash
   ollama serve
   ```

## 設定ファイル

設定ファイルは以下の場所に保存されます:

- Windows: `%LOCALAPPDATA%\Google\Mozc\ai_config.json`
- Linux: `~/.mozc/ai_config.json`

### 設定例

```json
{
  "enabled": true,
  "backend_type": "ollama",
  "ollama_endpoint": "http://localhost:11434",
  "ollama_model": "mistral:7b",
  "connect_timeout_ms": 50,
  "request_timeout_ms": 500,
  "max_wait_ms": 600,
  "cache_ttl_seconds": 60,
  "cache_max_entries": 100,
  "history_size": 5,
  "log_level": "info",
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
| 監視 | ウォッチドッグ | AI処理が長引いたら強制キャンセル |

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
```

## ログ

ログファイルは以下の場所に保存されます:

- Windows: `%LOCALAPPDATA%\Google\Mozc\ai_log.txt`
- Linux: `~/.mozc/ai_log.txt`

## ライセンス

このプロジェクトはMozcと同じライセンス（BSD 3-Clause License）の下で公開されています。

## 貢献

バグ報告や機能リクエストはIssueをお開きください。
プルリクエストも歓迎します。

## 謝辞

- Google Mozc チーム
- Ollama プロジェクト
