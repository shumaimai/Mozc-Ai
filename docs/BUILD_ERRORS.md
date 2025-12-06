# ビルドエラー・修正記録

このドキュメントは、AI Mozc IMEプロジェクトで発生したビルドエラーとその修正内容を記録しています。
新しいエラーが発生した場合、このドキュメントを参照してください。

---

## エラー一覧

### エラー #1: std::filesystem が見つからない

**発生日**: 2024年
**症状**:
```
error: 'filesystem' is not a namespace-name
error: '<filesystem>' file not found
```

**原因**: `<filesystem>`ヘッダーはC++17の機能だが、一部のコンパイラ/環境ではサポートが不完全

**修正内容**:
- `ai_config.cc`: `std::filesystem`の使用を削除
- `ai_logger.cc`: `std::filesystem::create_directories()`を独自実装に置き換え
- `CreateDirectoryRecursive()`関数を追加（`stat()`/`mkdir()`を使用）

**修正ファイル**:
- `src/ai/ai_config.cc`
- `src/ai/ai_logger.cc`

---

### エラー #2: インクルードパスエラー

**発生日**: 2024年
**症状**:
```
fatal error: 'ai/ai_config.h' file not found
```

**原因**: インクルードパスが`"ai/ai_config.h"`形式だったが、Bazelのincludes設定と不一致

**修正内容**:
- 同一ディレクトリ内のインクルードを`"ai_config.h"`形式に変更
- 別ディレクトリのインクルードを`"../ai/ai_config.h"`形式に変更
- BUILD filesに`includes = ["."]`を追加

**修正ファイル**:
- `src/ai/ollama_backend.cc`
- `src/ai/mock_backend.cc`
- `src/ai/ai_worker.cc`
- `src/ai/ai_worker.h`
- `src/ai/ai_logger.h`
- `src/rewriter/ai_rewriter.h`
- `src/rewriter/ai_rewriter.cc`
- `src/ai/BUILD`
- `src/rewriter/BUILD`

---

### エラー #3: 日本語パスでBazelが動作しない

**発生日**: 2024年
**症状**:
```
FATAL: changing directory into c:\users\...\ime開発計画\... failed: (error: 3)
```
文字化けしたエラーメッセージが表示される

**原因**: Bazelは日本語（非ASCII文字）を含むパスを処理できない

**修正内容**:
- ドキュメントに警告を追加
- プロジェクトを英語のみのパスに配置することを推奨
  - 悪い例: `C:\Users\name\Documents\IME開発計画\ai_mozc`
  - 良い例: `C:\m\ai_mozc`

**修正ファイル**:
- `docs/GETTING_STARTED.md`（トラブルシューティングセクション追加）

---

### エラー #4: Bazel 8でWORKSPACEが無効

**発生日**: 2024年
**症状**:
```
WARNING: --enable_bzlmod is set, but no MODULE.bazel file was found
ERROR: Unable to find package for @@[unknown repo 'rules_cc' requested from @@]//cc:defs.bzl
The repository '@@[unknown repo 'rules_cc' requested from @@]' could not be resolved
WORKSPACE file is disabled by default in Bazel 8
```

**原因**:
- Bazel 8（2024年後半リリース）からWORKSPACEファイルがデフォルトで無効化
- bzlmod（MODULE.bazel）への移行が必要
- ユーザーのBazelバージョン: 8.4.2

**修正内容**:
1. `MODULE.bazel`ファイルを作成（bzlmod対応）
2. 依存関係をWORKSPACEからMODULE.bazelに移行:
   - `rules_cc` → `bazel_dep(name = "rules_cc", version = "0.0.9")`
   - `googletest` → `bazel_dep(name = "googletest", version = "1.14.0")`
   - `platforms` → `bazel_dep(name = "platforms", version = "0.0.10")`
3. BUILD filesの依存関係参照を更新:
   - `@com_google_googletest//:gtest_main` → `@googletest//:gtest_main`
4. BUILD filesのCOPTSをクロスプラットフォーム対応に変更:
   ```python
   COPTS = select({
       "@platforms//os:windows": ["/std:c++17", "/W3"],
       "//conditions:default": ["-std=c++17", "-Wall", "-Wextra"],
   })
   ```
5. ビルドスクリプトから`--config=windows`/`--config=linux`を削除

**修正ファイル**:
- `MODULE.bazel`（新規作成）
- `src/ai/BUILD`
- `src/rewriter/BUILD`
- `.bazelrc`
- `scripts/build.ps1`
- `scripts/build.sh`

---

## 修正履歴

| 日付 | コミット | 内容 |
|------|----------|------|
| 2024 | initial | 初期実装 |
| 2024 | fix #1 | std::filesystem依存を削除、ログ出力追加 |
| 2024 | fix #2 | インクルードパスエラー修正 |
| 2024 | fix #3 | 日本語パス問題のドキュメント追加 |
| 2024 | fix #4 | Bazel 8 (bzlmod) 対応 |

---

## 今後発生しうる問題

### Visual Studio関連
- `BAZEL_VC`環境変数が設定されていない
- Windows SDKがインストールされていない
- C++ワークロードがインストールされていない

### ネットワーク関連
- 依存関係のダウンロード失敗（プロキシ環境）
- bzlmod registryへの接続失敗

### Bazel関連
- キャッシュの破損 → `bazelisk clean --expunge`で解決
- バージョン不整合 → `.bazelversion`ファイルでバージョン固定

---

## 参考リンク

- [Bazel bzlmod移行ガイド](https://bazel.build/external/migration)
- [Bazel Central Registry](https://registry.bazel.build/)
- [rules_cc](https://github.com/bazelbuild/rules_cc)
- [GoogleTest with Bazel](https://google.github.io/googletest/quickstart-bazel.html)
