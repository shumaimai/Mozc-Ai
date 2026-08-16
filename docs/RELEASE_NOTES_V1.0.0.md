# Mozc AI v1.0.0

## 概要

Mozc、公開データで学習した日本語文脈リランカー、ONNX Runtime、モデル、
トークナイザーを `MozcAI-1.0.0-x64.msi` に統合した最初の正式版です。

## 主な変更

- MozcのN-best候補を文脈付きクロスエンコーダーで再順位付け
- AIランタイムを `127.0.0.1:17890` 限定の常駐プロセスとして同梱
- マージンゲート、対象読みガード、段階的degrade、200msタイムアウトを実装
- AI利用不能時はMozc候補順へ安全にフォールバック
- 既存Mozcから専用製品 `Mozc AI 1.0.0` へのMSI移行を実装
- 変換本文を含むログを既定で無効化
- 旧DeepSeek/Ollamaリライターをv1.0.0バイナリから除外
- private usageモデルと個人変換ログを配布対象から除外

## 同梱物

- Mozc Windows x64バイナリ
- `rerank_daemon.exe`
- `cross_encoder_fp32.onnx`
- SentencePieceトークナイザー
- margin policy、モデル由来情報、基盤モデルライセンス
- ONNX Runtimeと必要なWindowsランタイム

## 検証

- upstream Mozc固定コミットからのクリーン統合
- `//rewriter:rerank_rewriter_test` 合格
- MSI administrative extraction成功
- MSI内のAIランタイム・モデル・TIP・Mozcサーバーを確認
- MozcサーバーからDeepSeek、Aliyun、token-plan文字列が0件であることを確認
- モデルランタイムを固定合成入力で100回疎通確認
- Windowsへ実インストールし、製品登録、TIP、Mozcサーバー、loopback AIプロセスの起動を確認

## 既知の制約

- Windows x64のみ
- Windows Installerが要求した場合は再起動が必要
- CPU推論版。GPUは必須ではなく、v1.0.0 MSIでは使用しない
