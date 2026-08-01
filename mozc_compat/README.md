# Mozc Compatibility Layer

このディレクトリには、google/mozc リポジトリに直接統合するためのファイルが含まれています。

## ファイル一覧

| ファイル | 説明 |
|---------|------|
| `ai_rewriter.h` / `ai_rewriter.cc` | 本家 Mozc API 向け AIRewriter 実装 |
| `ai_rewriter_test.cc` | Mozc 環境向けユニットテスト |
| `ai/BUILD.bazel` | `mozc/src/ai/BUILD.bazel` 用ビルド定義 |
| `rewriter_build.bazel.patch` | `rewriter/BUILD.bazel` 追記用スニペット |

## 統合方法（推奨）

```bash
git clone https://github.com/google/mozc.git
python3 scripts/integrate_mozc.py --mozc-dir /path/to/mozc/src

cd /path/to/mozc/src
bazelisk build //ai:all //rewriter:ai_rewriter //server:mozc_server
bazelisk test //ai:all //rewriter:ai_rewriter_test
```

Windows:

```powershell
.\scripts\integrate_mozc.ps1 -MozcDir C:\mozc\src
```

## スタンドアロン版との違い

| 項目 | スタンドアロン (`src/rewriter/`) | 統合版 (`mozc_compat/`) |
|------|----------------------------------|-------------------------|
| インターフェース | モック `rewriter_interface.h` | 本家 `rewriter/rewriter_interface.h` |
| 候補 API | `set_value()` 等 | `candidate.value` フィールド直接代入 |
| リクエスト API | `request.request_type` | `request.request_type()` |
| capability 戻り値 | `NONE` | `NOT_AVAILABLE` |
| ビルド | `cc_library` | `mozc_cc_library` |

詳細は `docs/MOZC_INTEGRATION.md` を参照してください。
