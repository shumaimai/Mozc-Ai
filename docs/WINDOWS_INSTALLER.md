# Windows インストーラー構築ガイド

AI 統合済み Mozc の **Windows MSI インストーラー** (`MozcAI64.msi`) を作る手順です。

## 概要

| 項目 | 内容 |
|------|------|
| インストーラー形式 | MSI（Mozc 公式と同じ WiX Toolset） |
| 出力ファイル | `MozcAI64.msi` |
| AI コード | `mozc_server.exe` に静的リンク（別 DLL 不要） |
| 同梱ファイル | `ai_config.default.json`, `setup_ai_mozc.ps1` |
| ユーザー設定 | 初回インストール時に `%LOCALAPPDATA%\Google\Mozc\ai_config.json` を自動生成 |

**Ollama は MSI に含まれません**（サイズ・ライセンスの都合）。別途インストールが必要です。

---

## 前提条件（ビルド用 PC）

1. Windows 10 1809 以降
2. Visual Studio 2022（「C++ によるデスクトップ開発」）
3. [Bazelisk](https://github.com/bazelbuild/bazelisk)
4. Python 3
5. Git
6. .NET SDK（Mozc の WiX ビルド用）

---

## ワンコマンドで MSI を作る

```powershell
cd C:\path\to\ai_mozc
.\scripts\package_windows.ps1
```

成果物: `dist\MozcAI64.msi`

### オプション

```powershell
# 既存の Mozc ソースを使う
.\scripts\package_windows.ps1 -MozcDir C:\mozc\src

# 依存関係ダウンロードをスキップ（2回目以降）
.\scripts\package_windows.ps1 -SkipDeps -SkipQt
```

---

## 手動ビルド（詳細）

### 1. AI モジュール統合

```powershell
python scripts\integrate_mozc.py --mozc-dir C:\mozc\src
python scripts\integrate_mozc_installer.py --mozc-dir C:\mozc\src
```

### 2. Mozc 依存関係

```powershell
cd C:\mozc\src
python build_tools\update_deps.py
python build_tools\build_qt.py --release --confirm_license
```

### 3. MSI ビルド

```powershell
bazelisk build package --config release_build
```

出力: `bazel-bin\win32\installer\MozcAI64.msi`

---

## エンドユーザー向けインストール手順

### 1. Mozc AI をインストール

`MozcAI64.msi` を管理者として実行。

インストール先: `C:\Program Files\Mozc\`

### 2. 日本語 IME を有効化

設定 → 時刻と言語 → 言語 → 日本語 → キーボードに **Mozc** を追加

### 3. Ollama をセットアップ

```powershell
# https://ollama.ai からインストール後
ollama pull gemma3:1b
ollama serve
```

### 4. 動作確認

- 日本語入力で同じ文を2回入力（1回目: Mozc 候補のみ、2回目以降: AI 候補がキャッシュから表示）
- ログ: `%LOCALAPPDATA%\Google\Mozc\ai_log.txt`

---

## MSI に含まれるもの

| ファイル | 説明 |
|---------|------|
| `mozc_server.exe` | AI 統合済み変換サーバー |
| `mozc_tip32.dll` / `mozc_tip64.dll` | IME 本体 |
| `mozc_tool.exe` | 設定ツール |
| `ai_config.default.json` | デフォルト AI 設定テンプレート |
| `setup_ai_mozc.ps1` | ユーザー設定の初期化スクリプト |
| MSVC ランタイム / Qt | Mozc 標準同梱 |

---

## トラブルシューティング

### MSI ビルドが失敗する

- Visual Studio の C++ ワークロードを確認
- `BAZEL_VC` が正しいか確認（`build.ps1 -CheckOnly`）
- Mozc 公式: [build_mozc_in_windows.md](https://github.com/google/mozc/blob/master/docs/build_mozc_in_windows.md)

### AI 候補が出ない

1. Ollama 起動確認: `curl http://localhost:11434/api/tags`
2. 設定確認: `%LOCALAPPDATA%\Google\Mozc\ai_config.json`
3. 手動で設定初期化:

   ```powershell
   powershell -File "C:\Program Files\Mozc\setup_ai_mozc.ps1" -PullModel
   ```

---

## 関連ドキュメント

- [Mozc統合ガイド](MOZC_INTEGRATION.md)
- [ビルドガイド](BUILD_GUIDE.md)
- [開発計画](PLAN.md)
