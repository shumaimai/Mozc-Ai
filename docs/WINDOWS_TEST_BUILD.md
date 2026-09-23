# Mozc AI 1.0.3-test1 — テストビルド手順書

> **これはテストビルドです。** ファイル名 `MozcAI-1.0.3-test1-x64.msi`、
> 「アプリの一覧」での表示名は **「Mozc AI Test」** です。MSI内部の
> ProductVersion は数値の `1.0.3` です。配布は GitHub Actions の
> **Artifact のみ**で、正式リリースではありません。

## 0. このビルドの内容

- 同梱モデル: Phase 2 `format_v2` 凍結クロスエンコーダ
  （ONNX sha256 `00738ecf…`、`ai/model/SHA256SUMS` で全ファイル固定。
  ビルド時とインストール後の両方で照合できます）
- guard モード: **既定 safety**（読みallowlistを無効化、短文脈・記号文脈・
  junk候補の各guardは継続）。優先順位は
  `MOZC_RERANK_GUARD_MODE` 環境変数 > `ai/model/margin_policy.json` の
  `"guard_mode"` > 組込既定（safety）。
- ログ: **既定オフ**。設定しない限り入力内容は一切記録されません（後述）。

## 1. インストール前の確認

### 1-1. 安定版 v1.0.2 MSI の確保（必須）

戻し用に、現在インストールしている版と同じMSIを手元に残します。

```powershell
# GitHub Release から取得（保存しておく）
# https://github.com/shumaimai/Mozc-Ai/releases/tag/v1.0.2
#   MozcAI-1.0.2-x64.msi / MozcAI-1.0.2-x64.msi.sha256
Get-FileHash .\MozcAI-1.0.2-x64.msi -Algorithm SHA256  # .sha256 と照合
```

### 1-2. ユーザー辞書のバックアップ（推奨）

MSIの入れ替えでユーザー辞書が消えることはありませんが、念のため
エクスポートします。

1. タスクトレイの Mozc AI アイコン →「辞書ツール」
2. 「管理」→「テキストファイルへエクスポート」→ 任意の場所へ保存

（辞書の実体は `%APPDATA%\Google\Google Japanese Input\` 配下の
ユーザープロファイル領域です。MSIはここを削除しません。）

### 1-3. 現行バージョンの確認

```powershell
Get-ItemProperty HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\* |
  Where-Object DisplayName -like "Mozc*" |
  Select-Object DisplayName, DisplayVersion
```

## 2. インストール

```powershell
# 管理者 PowerShell
.\scripts\install_msi.ps1 -MsiPath .\MozcAI-1.0.3-test1-x64.msi
# または直接: msiexec /i MozcAI-1.0.3-test1-x64.msi /l*v $env:TEMP\mozc_test1_install.log
```

- verboseログ: `%TEMP%\mozc_test1_install.log`
- **「Mozc AI Test」は v1.0.2 と同じUpgradeCodeのため、インストールすると
  安定版を置き換えます**（ダウングレード抑止はありません）。

### インストール後の確認

```powershell
# 自動検証レポート
powershell -ExecutionPolicy Bypass -File "C:\Program Files\Mozc\ai\..\post_install_verify.ps1" -Quiet:$false
# → %LOCALAPPDATA%\Google\Mozc\install_verify.txt に結果が書かれます

# ペイロード照合（凍結モデルとの一致確認）
powershell -ExecutionPolicy Bypass -File <repo>\scripts\windows_smoke.ps1
```

- `rerank_daemon.exe` はサインイン時に自動起動します（HKLM Run）。
  手動起動: `& "C:\Program Files\Mozc\ai\rerank_daemon.exe"`
- 待ち受け確認: `netstat -ano | findstr 17890`

## 3. 実機テスト（ユーザー担当）

1. **サインオフ → サインイン**（またはPC再起動）。mozc_server と
   rerank_daemon が新しい構成で立ち上がります。
2. `scripts\windows_smoke.ps1` を実行（合成入力のみ。p50/p95/p99も出力）。
3. IMEでの確認（いずれも通常操作のみ）:
   - 複数文節変換: 「えきにきしゃ」→ 一括変換後、最終文節だけ再ランキング
   - 候補保護: 記号・数字・ユーザー登録語が先頭の変換は書き換わらない
   - 復帰: daemonを終了してから変換 → Mozc通常候補のまま（IME停止しない）
   - レイテンシ体感: 変換確定に引っかかりがないか（200ms deadline）
4. daemon起動時の初回変換はモデル読込で数秒かかることがあります。

## 4. ログとプライバシー

| ログ | 既定 | 場所 | 内容 |
|---|---|---|---|
| 変換ログ | **オフ** | `MOZC_RERANK_LOG` で指定したパス | 読み・候補・文脈は**含まれます** |
| 診断ログ | **オフ** | `MOZC_RERANK_DIAG_LOG` で指定したパス | バイト数のみ（テキスト無し） |
| daemon | 常時オフ | なし | リクエストを保存しません |
| MSI | 常時オフ | なし | 変換テキストを記録しません |

変換ログを有効にする場合（デバッグ時のみ）:

```powershell
setx MOZC_RERANK_LOG "%USERPROFILE%\Desktop\mozc_rerank_log.jsonl"
# サインオフ → サインインで反映
```

**注意: 変換ログには個人の入力文が含まれます。GitHubにpushしたり、
画面共有・issue添付をしないでください。** スモークスクリプトの出力は
合成文字列のみで安全です。

## 5. guard モードの切替

| 操作 | 手順 | 反映タイミング |
|---|---|---|
| strictへ戻す（旧allowlist） | 管理者: `setx /M MOZC_RERANK_GUARD_MODE strict` | **サインオフ→サインイン（またはmozc_server再起動）が必要** |
| safetyへ戻す | 管理者: `setx /M MOZC_RERANK_GUARD_MODE safety`（または `REG delete` で値削除） | 同上 |
| AI全体を無効化 | 管理者: `setx /M MOZC_RERANK_ENABLED 0` | 同上（daemonは動き続けますがIMEから呼ばれません） |

- policyファイル側の切替: `C:\Program Files\Mozc\ai\model\margin_policy.json`
  の `"guard_mode"` を編集（管理者権限が必要。env値がある場合はenvが優先）。
- **env値・policyどちらを変更した場合も、反映にはサインオフ→サインイン
  （または mozc_server.exe と rerank_daemon.exe の再起動）が必要です。**
  設定はプロセス起動時に一度だけ読まれます。

## 6. 安定版 v1.0.2 への戻し方

1. 「設定」→「アプリ」→ **「Mozc AI Test」をアンインストール**
   （または管理者PowerShellで `msiexec /x MozcAI-1.0.3-test1-x64.msi /qn`）
2. 再起動（TIP登録のクリーンアップのため）
3. 手元の **`MozcAI-1.0.2-x64.msi`** をインストール
   （`.\scripts\install_msi.ps1 -MsiPath .\MozcAI-1.0.2-x64.msi`）
4. 再起動後、「設定」→「アプリ」で **「Mozc AI」v1.0.2** になっていることを確認
5. テスト中に有効化した環境変数を削除:
   `setx /M MOZC_RERANK_GUARD_MODE ""` の代わりに
   `REG delete "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v MOZC_RERANK_GUARD_MODE /f`

ユーザー辞書は上記操作で失われません（1-2のバックアップは保険）。

## 7. 既知の制限

- このテストビルドはPRブランチからのもので、mainへは未マージです。
- `final_test` データセットはモデル選定に一切使用していません。
- 旧バージョン（v1.0.2以前、strict既定）からの上書きインストールでも
  guardモードはこのMSI同梱のpolicy（safety）に従います。envで明示的に
  strictを設定している場合はそちらが優先されます。
