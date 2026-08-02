# AI バックエンド戦略メモ

**更新**: 2026年8月2日

## 現状

| 項目 | 内容 |
|------|------|
| デフォルト | Ollama + `gemma3:1b` |
| 実装済み | `ollama_backend`, `mock_backend` |
| 予定のみ | Groq（`ai_config.proto` に定義） |

## 「自前 AI を作る」とは何を指すか

IME 向け AI は **汎用 LLM を丸ごと自作する** のではなく、次のどれかを選ぶのが現実的です。

### A. ローカル小モデル（現行路線の強化）— 推奨

Ollama 上でモデルを差し替えるだけ。コード変更は設定のみ。

| モデル | 特徴 | IME 向き |
|--------|------|----------|
| `gemma3:1b` | 軽量・高速（現デフォルト） | 応答速度 ◎、品質 △ |
| `deepseek-r1:1.5b` | 推論寄り・日本語可 | 品質 ◎、やや重い |
| `qwen2.5:0.5b` | 超軽量 | 速度 ◎、品質 △ |
| `phi4-mini` | MS 製小型 | バランス型 |

```json
{
  "ollama_model": "deepseek-r1:1.5b"
}
```

**メリット**: プライバシー、オフライン、MSI に同梱不要  
**デメリット**: ユーザーが Ollama + モデル DL が必要

### B. DeepSeek API（クラウド）— 次の実装候補

DeepSeek は OpenAI 互換 API を提供。新バックエンド `openai_compatible_backend` 1本で以下をまとめて対応可能:

- DeepSeek (`https://api.deepseek.com`)
- Groq
- OpenAI
- ローカル LM Studio

```json
{
  "backend_type": "openai_compatible",
  "api_endpoint": "https://api.deepseek.com/v1",
  "api_model": "deepseek-chat",
  "api_key_env": "DEEPSEEK_API_KEY"
}
```

**メリット**: 高品質、セットアップ簡単（API キーのみ）  
**デメリット**: ネット必須、課金、プライバシー、レイテンシ（タイムアウト設計が重要）

### C. IME 特化の小型モデル自作 — 中長期

汎用 LLM ではなく **「ひらがな + 文脈 → 候補 top-3」** に特化したモデル。

- 学習データ: Mozc 辞書 + ユーザー履歴（匿名化）+ 合成コーパス
- 手法: 小さな transformer / ランキングモデル
- 推論: ONNX Runtime でローカル実行（Ollama 不要）

**メリット**: 速度・品質を IME 向けに最適化可能  
**デメリット**: 学習パイプライン・評価基準の構築が大規模

### D. プロンプト＋ルールのみ — すぐ試せる

モデルはそのまま、プロンプトを日本語 IME 特化にする。

```
入力: {key}
既存候補: {candidates}
文脈: {history}
→ 既存にない自然な変換を最大3つ、JSON配列で返せ
```

**メリット**: 実装コスト最小  
**デメリット**: 根本的な品質上限はモデル依存

---

## 推奨ロードマップ

```
短期  DeepSeek を Ollama 経由で試す（設定変更のみ）
  ↓
中期  OpenAI 互換 API バックエンド追加（DeepSeek API / Groq）
  ↓
長期  IME 特化プロンプト改善 + ベンチマーク
  ↓
任意  小型特化モデルの検討（ ONNX ）
```

## 設計上の制約（変わらない）

- IME は絶対にブロックしない
- クラウド API はタイムアウト 500ms 以下が必須
- ローカルモデルは 1〜3B パラメータが現実的上限
- MSI にモデル本体は同梱しない（サイズ・更新頻度の問題）

## DeepSeek を試す最短手順（Ollama 経由）

```bash
ollama pull deepseek-r1:1.5b
```

`%LOCALAPPDATA%\Google\Mozc\ai_config.json`:

```json
{
  "ollama_model": "deepseek-r1:1.5b"
}
```

同じ入力を2回打って、2回目以降に AI 候補が出るか確認。
