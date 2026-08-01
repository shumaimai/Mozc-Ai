# Mozc統合ガイド

AI Mozc IME モジュールを本家 [google/mozc](https://github.com/google/mozc) に統合する手順です。

**最終更新**: 2026年8月1日（Bzlmod / `BUILD.bazel` / `mozc_cc_*` 対応）

---

## 概要

統合後の構成:

```
mozc/src/
├── ai/                    # AIモジュール（Ollama, キャッシュ, ワーカー）
└── rewriter/
    ├── ai_rewriter.*      # Mozc互換 AIRewriter
    ├── BUILD.bazel        # ai_rewriter ターゲット追記
    └── rewriter.cc        # Rewriter Chain 登録
```

---

## 前提条件

1. Mozc ソースを clone 済み（`git clone` のみで可。submodule 不要）
2. Bazelisk がインストール済み
3. Mozc のビルドが単体で成功すること

```bash
git clone https://github.com/google/mozc.git
cd mozc/src
bazelisk build //server:mozc_server
```

---

## 自動統合（推奨）

### Linux / macOS

```bash
cd /path/to/ai_mozc
python3 scripts/integrate_mozc.py --mozc-dir /path/to/mozc/src
```

### Windows

```powershell
cd C:\path\to\ai_mozc
.\scripts\integrate_mozc.ps1 -MozcDir C:\path\to\mozc\src
```

ドライラン:

```bash
python3 scripts/integrate_mozc.py --mozc-dir /path/to/mozc/src --dry-run
```

### スクリプトが行うこと

1. `src/ai/*` → `mozc/src/ai/` にコピー
2. `mozc_compat/ai/BUILD.bazel` → `mozc/src/ai/BUILD.bazel`
3. `mozc_compat/ai_rewriter.*` → `mozc/src/rewriter/`
4. `rewriter/BUILD.bazel` に `ai_rewriter` ターゲットを追記
5. `rewriter/rewriter.cc` に `MOZC_AI_REWRITER` と `AIRewriter` 登録を追記

---

## ビルドとテスト

```bash
cd /path/to/mozc/src

# AIモジュール
bazelisk build //ai:all

# AIRewriter
bazelisk build //rewriter:ai_rewriter

# 統合後の Mozc サーバー
bazelisk build //server:mozc_server

# テスト（5件すべて PASS すること）
bazelisk test //ai:all //rewriter:ai_rewriter_test
```

**2026年8月検証結果**（Bazel 9.0.2, Linux）:

```
//ai:ai_backend_test              PASSED
//ai:ai_candidate_cache_test      PASSED
//ai:ai_config_test                PASSED
//ai:ai_worker_test                PASSED
//rewriter:ai_rewriter_test        PASSED
```

---

## 手動統合

自動スクリプトを使わない場合:

### 1. AIモジュールの配置

```bash
cp -r ai_mozc/src/ai/* mozc/src/ai/
cp ai_mozc/mozc_compat/ai/BUILD.bazel mozc/src/ai/BUILD.bazel
```

### 2. AIRewriter の配置

```bash
cp ai_mozc/mozc_compat/ai_rewriter.{h,cc} mozc/src/rewriter/
cp ai_mozc/mozc_compat/ai_rewriter_test.cc mozc/src/rewriter/
```

### 3. `rewriter/BUILD.bazel` への追記

`mozc_compat/rewriter_build.bazel.patch` の内容を `dice_rewriter` ターゲットの直前に追加し、`rewriter` ターゲットの `deps` に `":ai_rewriter"` を追加します。

### 4. `rewriter/rewriter.cc` への追記

```cpp
#define MOZC_AI_REWRITER

#ifdef MOZC_AI_REWRITER
#include "rewriter/ai_rewriter.h"
#endif

// Rewriter::Rewriter() 内、CorrectionRewriter の後:
#ifdef MOZC_AI_REWRITER
  AddRewriter(std::make_unique<AIRewriter>());
#endif
```

---

## 動作確認

1. Ollama を起動:

   ```bash
   ollama serve
   ollama pull gemma3:1b
   ```

2. 設定ファイルを作成（`~/.mozc/ai_config.json`）:

   ```json
   {
     "enabled": true,
     "backend_type": "ollama",
     "ollama_endpoint": "http://localhost:11434",
     "ollama_model": "gemma3:1b"
   }
   ```

3. Mozc サーバーを起動して入力テスト
4. ログ確認: `tail -f ~/.mozc/ai_log.txt`

---

## トラブルシューティング

### `RewriterInterface::NONE` コンパイルエラー

本家 Mozc では `NOT_AVAILABLE` を使用します。`mozc_compat/` 版を使ってください。

### Bazel 9 の依存警告

Mozc 推奨の Bazel バージョンを使用してください。問題が続く場合は Mozc の `MODULE.bazel` に合わせて Bazelisk のバージョンを固定します。

### AI候補が表示されない

1. 初回変換はキャッシュミスのため Mozc 候補のみ（設計通り）
2. 同じ入力を2回目以降で試す
3. Ollama の起動状態とログを確認

---

## ロールバック

```bash
rm -rf mozc/src/ai
rm mozc/src/rewriter/ai_rewriter.*
# rewriter/BUILD.bazel と rewriter/rewriter.cc の変更を git checkout で戻す
```

---

## 関連ドキュメント

- [開発計画書](PLAN.md)
- [テストガイド](TESTING_GUIDE.md)
- [mozc_compat/README.md](../mozc_compat/README.md)
