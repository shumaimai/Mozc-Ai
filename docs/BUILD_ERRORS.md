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
   - `rules_cc` → `bazel_dep(name = "rules_cc", version = "0.1.1")`
   - `googletest` → `bazel_dep(name = "googletest", version = "1.14.0.bcr.1")`
   - `platforms` → `bazel_dep(name = "platforms", version = "0.0.11")`
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

**注意**: MODULE.bazelのバージョンはBazel Central Registry (BCR)の実際のバージョンと一致させる必要があります。
BCRで利用可能なバージョンは https://registry.bazel.build/ で確認できます。

**修正ファイル**:
- `MODULE.bazel`（新規作成）
- `src/ai/BUILD`
- `src/rewriter/BUILD`
- `.bazelrc`
- `scripts/build.ps1`
- `scripts/build.sh`

---

### エラー #5: ソースファイルが見つからない (missing input file)

**発生日**: 2024年
**症状**:
```
ERROR: Compiling src/ai/ai_config.cc failed: missing input file '//src/ai:ai_config.h'
ERROR: Compiling src/ai/ai_config.cc failed: missing input file '//src/ai:ai_config.cc'
```
多数の "missing input file" エラーが発生

**原因**:
ローカルのワーキングディレクトリにソースファイルが存在しない。
以下のケースで発生:
1. `git pull`を実行していない（リモートの変更がローカルに反映されていない）
2. ディレクトリを手動コピーした際にファイルが欠落
3. 別のブランチをチェックアウトしている
4. ファイルが正しくクローンされていない

**確認方法**:
```powershell
# ファイルが存在するか確認
dir src\ai\*.cc
dir src\ai\*.h

# Gitの状態確認
git status
git branch -a

# 最新の変更を取得
git fetch origin
git pull origin claude/ai-mozc-ime-integration-01UtNsKb2wmAp6dYJa6c8Hut
```

**修正方法**:
```powershell
# 方法1: 最新をプル
git pull

# 方法2: 強制的に最新に同期
git fetch origin
git reset --hard origin/claude/ai-mozc-ime-integration-01UtNsKb2wmAp6dYJa6c8Hut

# 方法3: 新しくクローン
cd C:\m
git clone <repository-url> ai_mozc
cd ai_mozc
git checkout claude/ai-mozc-ime-integration-01UtNsKb2wmAp6dYJa6c8Hut
```

---

### エラー #6: BazelがVisual C++を検出できない

**発生日**: 2024年
**症状**:
```
ERROR: vc_installation_error_x64.bat failed
The target you are compiling requires Visual C++ build tools.
Bazel couldn't find a valid Visual C++ build tools installation on your machine.
```

**原因**:
BazelがVisual C++のインストールパスを見つけられない。以下のケースで発生:
1. `BAZEL_VC`環境変数が設定されていない
2. Visual Studioの「C++によるデスクトップ開発」ワークロードがインストールされていない
3. Visual Studioのインストールパスが標準と異なる
4. Bazelのキャッシュが古い

**確認方法**:
```powershell
# Visual C++のパスを確認
dir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC"

# vswhere で確認
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64

# 現在のBAZEL_VC設定を確認
echo $env:BAZEL_VC
```

**修正方法**:

方法1: ビルドスクリプトを使用（自動設定）
```powershell
# 最新のスクリプトを取得
git pull

# スクリプトが自動的にBAZEL_VCを設定
.\scripts\build.ps1
```

方法2: 手動で環境変数を設定
```powershell
# 一時的に設定（現在のセッションのみ）
$env:BAZEL_VC = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC"

# 永続的に設定
[Environment]::SetEnvironmentVariable("BAZEL_VC", "C:\Program Files\Microsoft Visual Studio\2022\Community\VC", "User")
```

方法3: C++ワークロードをインストール
```
1. Visual Studio Installerを開く
2. 「変更」をクリック
3. 「C++によるデスクトップ開発」にチェック
4. 「MSVC v143」と「Windows 10/11 SDK」がチェックされていることを確認
5. 「変更」をクリックしてインストール
```

方法4: Bazelキャッシュをクリア
```powershell
bazelisk clean --expunge
```

**修正ファイル**:
- `scripts/build.ps1`（BAZEL_VC自動検出機能を追加）

---

## 修正履歴

| 日付 | コミット | 内容 |
|------|----------|------|
| 2024 | 42d5825 | 初期実装 |
| 2024 | 9d9b3db | fix #1, #2: std::filesystem依存を削除、インクルードパス修正 |
| 2024 | b1d0f27 | fix #3: 日本語パス問題のドキュメント追加、PowerShellスクリプト改善 |
| 2024 | f8b794e | fix #4: Bazel 8 (bzlmod) 対応 |
| 2024 | 368f2b1 | fix #4 update: MODULE.bazelバージョン修正、エラー#5ドキュメント追加 |
| 2024 | (current) | fix #6: BAZEL_VC自動検出機能追加 |

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
