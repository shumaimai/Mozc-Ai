# AI Mozc IME - Windows Installer Assets

このディレクトリのファイルは、Mozc の WiX MSI インストーラーに同梱されます。

| ファイル | 役割 |
|---------|------|
| `ai_config.default.json` | デフォルト AI 設定テンプレート |
| `setup_ai_mozc.ps1` | インストール後にユーザー設定を初期化 |

統合は `scripts/integrate_mozc_installer.py` が自動で行います。

詳細: `docs/WINDOWS_INSTALLER.md`
