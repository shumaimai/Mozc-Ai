# Mozc統合ガイド

このドキュメントでは、AI Mozc IMEモジュールを本家Mozcソースコードに統合する手順を説明します。

---

## 目次

1. [概要](#概要)
2. [前提条件](#前提条件)
3. [ファイル配置](#ファイル配置)
4. [BUILD設定の統合](#build設定の統合)
5. [Rewriter Chainへの追加](#rewriter-chainへの追加)
6. [動作確認](#動作確認)
7. [トラブルシューティング](#トラブルシューティング)

---

## 概要

AI Mozc IMEは以下のコンポーネントで構成されています：

```
┌─────────────────────────────────────────────────────────────┐
│                    AI Mozc モジュール                        │
├─────────────────────────────────────────────────────────────┤
│  src/ai/              │  AIバックエンド、キャッシュ、ワーカー   │
│  src/rewriter/        │  AIRewriter（Mozcインターフェース）    │
└─────────────────────────────────────────────────────────────┘
```

これらを本家Mozcの対応するディレクトリに配置し、Rewriter Chainに追加します。

---

## 前提条件

1. **Mozcソースコード**: https://github.com/google/mozc からクローン済み
2. **ビルド環境**: Mozcのビルドが成功している
3. **Ollama**: ローカルにインストール済み（オプション）

```bash
# Mozcソースのクローン
git clone https://github.com/google/mozc.git
cd mozc/src
```

---

## ファイル配置

### Step 1: AIモジュールのコピー

```bash
# Mozcソースのルートに移動
cd /path/to/mozc/src

# AIディレクトリを作成・コピー
mkdir -p ai
cp -r /path/to/ai_mozc/src/ai/* ai/
```

コピーするファイル:
```
ai/
├── BUILD                    # Bazel BUILD file
├── ai_backend.h            # バックエンドインターフェース
├── ai_backend_test.cc      # テスト
├── ai_candidate_cache.h    # キャッシュヘッダー
├── ai_candidate_cache.cc   # キャッシュ実装
├── ai_candidate_cache_test.cc
├── ai_config.h             # 設定ヘッダー
├── ai_config.cc            # 設定実装
├── ai_config_test.cc
├── ai_logger.h             # ロガーヘッダー
├── ai_logger.cc            # ロガー実装
├── ai_worker.h             # ワーカーヘッダー
├── ai_worker.cc            # ワーカー実装
├── ai_worker_test.cc
├── mock_backend.cc         # モックバックエンド
└── ollama_backend.cc       # Ollamaバックエンド
```

### Step 2: AIRewriterのコピー

```bash
# 既存のrewriterディレクトリにコピー
cp /path/to/ai_mozc/src/rewriter/ai_rewriter.h rewriter/
cp /path/to/ai_mozc/src/rewriter/ai_rewriter.cc rewriter/
cp /path/to/ai_mozc/src/rewriter/ai_rewriter_test.cc rewriter/
```

**注意**: `rewriter_interface.h`はコピー不要（Mozcに既存のインターフェースを使用）

---

## BUILD設定の統合

### Step 3: ai/BUILD の調整

Mozcの既存のBUILDスタイルに合わせて`ai/BUILD`を調整：

```python
# ai/BUILD

load("//bazel:stubs.bzl", "bzl_library", "cc_library_mozc", "cc_test_mozc")

package(default_visibility = ["//visibility:public"])

cc_library_mozc(
    name = "ai_config",
    srcs = ["ai_config.cc"],
    hdrs = ["ai_config.h"],
    deps = [
        "//base:logging",
        "//base:port",
    ],
)

cc_library_mozc(
    name = "ai_logger",
    srcs = ["ai_logger.cc"],
    hdrs = ["ai_logger.h"],
    deps = [
        ":ai_config",
        "//base:port",
    ],
)

cc_library_mozc(
    name = "ai_candidate_cache",
    srcs = ["ai_candidate_cache.cc"],
    hdrs = ["ai_candidate_cache.h"],
    deps = [
        ":ai_config",
    ],
)

cc_library_mozc(
    name = "ai_backend",
    srcs = [
        "ollama_backend.cc",
        "mock_backend.cc",
    ],
    hdrs = ["ai_backend.h"],
    deps = [
        ":ai_config",
        ":ai_logger",
    ],
)

cc_library_mozc(
    name = "ai_worker",
    srcs = ["ai_worker.cc"],
    hdrs = ["ai_worker.h"],
    deps = [
        ":ai_backend",
        ":ai_candidate_cache",
        ":ai_config",
        ":ai_logger",
    ],
)

cc_library_mozc(
    name = "ai",
    deps = [
        ":ai_backend",
        ":ai_candidate_cache",
        ":ai_config",
        ":ai_logger",
        ":ai_worker",
    ],
)

# Tests
cc_test_mozc(
    name = "ai_config_test",
    srcs = ["ai_config_test.cc"],
    deps = [
        ":ai_config",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test_mozc(
    name = "ai_candidate_cache_test",
    srcs = ["ai_candidate_cache_test.cc"],
    deps = [
        ":ai_candidate_cache",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test_mozc(
    name = "ai_worker_test",
    srcs = ["ai_worker_test.cc"],
    deps = [
        ":ai_worker",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test_mozc(
    name = "ai_backend_test",
    srcs = ["ai_backend_test.cc"],
    deps = [
        ":ai_backend",
        "@com_google_googletest//:gtest_main",
    ],
)
```

### Step 4: rewriter/BUILD への追加

`rewriter/BUILD`に以下を追加：

```python
cc_library_mozc(
    name = "ai_rewriter",
    srcs = ["ai_rewriter.cc"],
    hdrs = ["ai_rewriter.h"],
    deps = [
        ":rewriter_interface",
        "//ai:ai_candidate_cache",
        "//ai:ai_worker",
        "//ai:ai_config",
    ],
)

cc_test_mozc(
    name = "ai_rewriter_test",
    srcs = ["ai_rewriter_test.cc"],
    deps = [
        ":ai_rewriter",
        "@com_google_googletest//:gtest_main",
    ],
)
```

---

## Rewriter Chainへの追加

### Step 5: RewriterのFactoryに追加

Mozcの`rewriter/rewriter.cc`（または同等のファイル）を編集：

```cpp
// rewriter/rewriter.cc

#include "rewriter/ai_rewriter.h"

// ... existing code ...

// AddRewriters() または InitRewriters() 関数内で追加
void Rewriter::AddRewriters() {
  // ... existing rewriters ...

  // AI Rewriter を追加（最後に追加することで優先度を低くする）
  AddRewriter(std::make_unique<AIRewriter>());
}
```

### Step 6: インクルードパスの調整

`ai_rewriter.h`のインクルードパスをMozc構造に合わせて修正：

```cpp
// ai_rewriter.h (修正後)

#ifndef MOZC_REWRITER_AI_REWRITER_H_
#define MOZC_REWRITER_AI_REWRITER_H_

#include "rewriter/rewriter_interface.h"  // Mozc本体のインターフェース
#include "ai/ai_candidate_cache.h"
#include "ai/ai_worker.h"

// ... rest of the code ...
```

---

## 動作確認

### Step 7: ビルド

```bash
cd /path/to/mozc/src

# AIモジュールのビルド
bazelisk build //ai:all

# AIRewriterのビルド
bazelisk build //rewriter:ai_rewriter

# 全体ビルド
bazelisk build //server:mozc_server //gui:mozc_tool
```

### Step 8: テスト実行

```bash
# AIモジュールのテスト
bazelisk test //ai:all

# AIRewriterのテスト
bazelisk test //rewriter:ai_rewriter_test
```

### Step 9: 動作確認

1. Ollamaを起動:
   ```bash
   ollama serve
   ```

2. Mozcサーバーを起動:
   ```bash
   ./bazel-bin/server/mozc_server
   ```

3. 入力テスト:
   - 「きょうのてんき」と入力
   - AI候補が表示されるか確認
   - ログファイル (`~/.mozc/ai_log.txt`) を確認

---

## トラブルシューティング

### ビルドエラー: インクルードファイルが見つからない

```
fatal error: 'ai/ai_config.h' file not found
```

**解決方法**: BUILD fileの`deps`に必要な依存関係を追加

### ビルドエラー: シンボルが見つからない

```
undefined reference to `mozc::ai::AIConfigManager::...'
```

**解決方法**: リンク順序を確認、`deps`に不足しているライブラリを追加

### 実行時エラー: AI候補が表示されない

1. ログを確認: `cat ~/.mozc/ai_log.txt`
2. Ollamaの状態確認: `curl http://localhost:11434/api/tags`
3. 設定ファイル確認: `cat ~/.mozc/ai_config.json`

### パフォーマンス問題

AI処理が遅い場合:
1. `connect_timeout_ms`と`request_timeout_ms`を短く設定
2. より軽量なモデルを使用（`mistral:7b` → `phi:2.7b`）
3. キャッシュサイズを増加

---

## 設定オプション

統合後、以下の設定で動作をカスタマイズ可能：

| 設定項目 | 説明 | デフォルト |
|---------|------|-----------|
| `enabled` | AI機能の有効/無効 | `true` |
| `backend_type` | バックエンド種類 | `ollama` |
| `ollama_model` | 使用モデル | `mistral:7b` |
| `connect_timeout_ms` | 接続タイムアウト | `50` |
| `request_timeout_ms` | リクエストタイムアウト | `500` |

設定ファイルの場所:
- **Linux/macOS**: `~/.mozc/ai_config.json`
- **Windows**: `%LOCALAPPDATA%\Google\Mozc\ai_config.json`

---

## 次のステップ

1. **設定UI**: Mozc設定画面にAI設定タブを追加
2. **インストーラー**: AIモジュール込みのインストーラー作成
3. **モデル最適化**: 日本語特化モデルの検討

---

*最終更新: 2024年*
