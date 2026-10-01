# MozcAI 改善結果（2026-09-30）

このIntel i7-1060NG7で、常駐daemonへのTCP往復を含むp50は **20.4ms** でした。
70ms以下の目標を達成しました。推論した成功応答だけでもp50は **30.5ms** です。
モデルとランタイムはこの作業ツリーに反映済みです。既存のWindows MSIの再ビルド・再インストールは行っていません。

## 同じ5,974変換での比較

公開Dataset v2の文書分離validationを使用。毎回新しいTCP接続を作り、候補を一括推論し、
20件のウォームアップを除外しました。200msの期限、同じ文脈ガード、同じ候補cap=30です。
通信失敗時はMozcの選択を正答数に含め、遅い応答も全体の遅延から除外しません。
C++のHARD_PROTECTも評価側で再生します。CPU governorはpowersaveのままです。

| 指標 | 旧配布モデル・旧daemon | 新モデル・新daemon |
|---|---:|---:|
| 全変換TCP往復 p50 | 33.5ms | **20.4ms** |
| 全変換TCP往復 p95 | 187.4ms | **125.9ms** |
| 全変換TCP往復 p99 | 202.7ms | 201.1ms |
| 推論した成功応答の p50 | 47.4ms | **30.5ms** |
| 200ms期限超過 | 283/5,974 (4.74%) | **145/5,974 (2.43%)** |
| 期限・フォールバック込み正答率 | 76.31% | **76.92%** |
| 正しい上書き helped | 145 | **168** |
| 誤った上書き hurt | 42 | **29** |

全体p50はガードによるskipを含みます。推論した成功応答の分位点は期限超過を含まず、
その裾は打ち切られています。別の**期限なしCPU全件評価**では、推論対象4,353件
（先頭20件のウォームアップ相当を除外）でp50 30.5ms、
p95 123.8ms、p99 289.0msでした。
78件が200msを超え、最大約908msです。低頻度の長い推論は残っており、通信期限でMozcへ戻します。
成功応答だけのp99を期限なし推論のp99と混同しません。

測定環境：Intel Core i7-1060NG7、4コア/8スレッド、Linux、Python 3.14、ORT 1.30.0、
SentencePiece 0.2.2。旧daemonは既定の8スレッド、新daemonは4スレッド。
CPUの性能測定は順番に実行し、GPU学習はModalのL4を使いました。
資料のRyzen 5800X3DやSarashinaモデルの速度とは測定条件が異なります。

## 精度と文脈判断

期限なし・全件CPU評価で、ガード後正答率は **77.12%**、helped 180 / hurt 29でした。
凍結Phase 2の保存済みポリシー評価は76.36%、helped 153 / hurt 47です。
ニューラル単独のNEURAL_ELIGIBLE 4,021件は、保存済みbaselineの86.47%から
CPU評価で**88.06%**へ改善しました。ALLのニューラル単独は78.27%です。
速度表の76.92%は200msの期限を含む実際の選択結果で、77.12%は期限なしの値です。

学習に使用していない短い左文脈の診断54件（27対）も同じ候補順で両モデルを評価しました。

| 診断指標 | 旧モデル | 採用モデル |
|---|---:|---:|
| ニューラル正答 | 41/54 | **47/54** |
| ポリシー後正答 | 38/54 | **44/54** |
| 文脈を反転した対の両方に正答 | 14/27 | **20/27** |
| ポリシー後に対の両方に正答 | 11/27 | **17/27** |

この診断は手作成の候補集合で、実Mozc N-bestや現場の入力ログではありません。
元の12件の合成セットには、読みと候補が合わない例や正解が曖昧な例がありました。
正しい同音候補と決め手になる左文脈へ修正し、**同じ修正版**で両モデルを再評価すると
旧9/12、新12/12でした。旧版セットの点数とは比較しません。

validationはモデル・閾値の選択に使用しました。独立したfinal_testは開いておらず、
個人の入力履歴も学習・クラウド送信していません。**新しい現場ログでの改善は未検証です。**

## 実装した変更

1. Mozcが同じ表記を辞書経路・品詞違いで何度も返す場合、同じ入力文字列のforwardを
   変換内で共有し、元の位置と重複数へスコアを戻します。候補の削除や文脈短縮はしません。
   同じ160件の試測では平均バッチ15.72件から11.27件へ減り、旧8スレッドp50 49.74msから
   重複除去+4スレッド33.07msになりました。変換間の回答キャッシュは使いません。
2. スレッド数をポリシーの`intra_op: 4`で指定可能にし、CLIの`--intra-op`を優先します。
3. 実候補集合単位のlistwise CEを実装し、0.15倍の候補BCEを補助損失に加えました。
   正解が候補外なら学習対象から除外し、goldを注入しません。重複表記は同じラベルとして扱います。
4. 30Mの事前学習モデルから新規学習した1 epoch目を教師にし、lr=2e-6、1 epoch、
   温度2・KL係数2で判断分布を保ちながら追加学習しました。10層は維持します。
5. 切り出し後の先頭空白がdaemonでだけ消える差がvalidationの21件にありました。
   最終の学習・評価でも同じ`clean_context`を適用し、全件CPU照合で文脈不一致は0件です。
6. C++のTCP処理を接続・送信・受信を通じた一つの期限へ変更しました。
   `std::async`で毎回スレッドを作るdaemon経路を直接の期限付きTCP呼び出しへ変更し、
   future破棄時に待ち続ける問題を避けました。部分応答を送り続ける実サーバーで試験しています。
7. 評価器の期限超過時のMozcフォールバック、正答数、遅延の集計を修正しました。
   公開fixtureの品質も修正し、hash付きのモデルmanifestを同梱しました。

候補上限を15 negativesから実cap=30へ変え、学習をgroup単位で正規化し、動的paddingも
使っているため、この結果を損失関数だけの効果とは断定しません。

## 比較して採用しなかった構成

| 構成 | validationでの代表的なガード後結果 | 判断 |
|---|---|---|
| 旧BCEからlistwise追加学習 | 76.58%、hurt 32（epoch1、tau4） | 新規学習に劣る |
| 7層蒸留 | 76.67%、hurt 34（epoch1、tau2.5） | 10層を採用 |
| 新規listwiseの2 epoch目 | 77.02%、hurt 48（tau2.5） | 過学習・誤上書きが増える |
| 新規学習から正規化も統一 | 76.87%、hurt 37（epoch1、tau1.5） | 分布を保つ追加学習版を採用 |
| 新規listwise教師のDynamic INT8 | CPU76.77%、hurt 28（tau1.5） | p50約19msだがFP32教師77.13%より精度低下 |
| FP32グラフ融合 | 160件でp50約36ms | 安定した速度利益がなく通常exportを採用 |

INT8はMatMulのper-channel QInt8で比較しました。ONNX Runtimeの
[量子化手順](https://onnxruntime.ai/docs/performance/model-optimizations/quantization.html)を参考に、
元のgraphと最適化graphを別artifactとして保存し、このCPUで精度を測りました。
INT8や小さいモデルを、速度だけを理由に出荷モデルへ置き換えていません。

## 成果物と検証

採用モデル：`runtime/model/cross_encoder_fp32.onnx`、140.25 MiB、opset17。
SHA256: `b2971435a9934c71d8fecbdfd50611c355c4dd12cba32b3244073e4fb8702a6c`。
checkpoint: `ea935215b2c0148bae56277ddd49b3735c04f678b9c996f5d2f273ac281bbd87`。
設定はtau1.5 / intra_op4 / cap30 / max_len128 / context50 / deadline200ms / safetyです。

検証：rerank関連のunit test **53件**、dataset test **14件**がすべて成功。
C++ TCPの実コードをコンパイルし、通常応答・部分応答・無応答の期限を検証しました。
モデルassetのSHA256も全9件一致しています。

CPU PyTorchとONNXのパリティ：800変換、9,158入力でトークンID・argmax・tau1.5選択が全件一致。
最大スコア差2.13e-5。全件GPU validationとの比較ではGPU TF32数値差が残り、
argmaxは5,971/5,974、tau1.5選択は5,973/5,974一致でした。文脈不一致は0件です。
出荷構成のCPU評価値を最終値として記録しています。

学習元は47,589行、NEURAL_ELIGIBLE 31,665行、このうち正解がcap内にあり複数の
異なる表記がある30,411 groupを使いました。NEURAL_ELIGIBLEだけを学習し、
PROTECTED_EVAL_ONLY / COVERAGE_LIMITEDは評価に残します。

生の評価artifactは `/home/hashiguchishuhei/Mozc-Ai-Training-v2/artifacts/optimization_20260930/`：
`daemon_original.json`、`daemon_selected.json`、`selected_fp32/report.json`、
`selected_fp32/scores.jsonl.gz`、`refine_parity_cpu.json`、`everyday_selected.json`、
各Modal runのmanifestとlogに保存しています。旧配布モデルと設定は同ディレクトリの
`shipping_before/`、旧daemonソースは`original_runtime.py`にバックアップしました。

採用モデルの学習runは[Modal L4の完了run](https://modal.com/apps/syuhei2009/main/ap-mGLb6AkBkLSKrhpGGg2XjS)。
コードは `Mozc-Ai-Training-v2/tools/rerank/listwise.py` と
`scripts/modal_listwise_optimization.py`、出荷側はこのリポジトリのdaemon・C++ rewriterです。

## このマシンでの起動と再測定

```bash
/home/hashiguchishuhei/Mozc-Ai-Training-v2/.venv-parity/bin/python \
  /home/hashiguchishuhei/Mozc-Ai/runtime/rerank_daemon.py
```

別ターミナルで：

```bash
cd /home/hashiguchishuhei/Mozc-Ai-Training-v2
.venv-parity/bin/python -m tools.rerank.daemon_latency_bench \
  --data data/public/contextual_ranking_v2_production_runtime_context/dataset/validation.jsonl.gz \
  --port 17890 --out artifacts/optimization_20260930/recheck.json
```

Windowsは `scripts/package_windows.ps1` でMSIを再ビルドし、
`windows_smoke.ps1`で新しいmodel hashを確認する必要があります。
今回のLinuxでの測定は、WindowsのIME組み込み・インストーラー試験を代替しません。
試験用daemonは停止し、学習ジョブも完了しています。

## PRレビューへの対応（2026-10-01）

[公開PR #18のレビュー](https://github.com/shumaimai/Mozc-Ai/pull/18#issuecomment-5927714756)
の3件を修正しました。

- 匿名診断は入口でreason・stage・daemon_resultを既知の固定コードへ制限し、
  未知の応答文字列を`unknown`へ置換します。JSON文字列の制御文字もエスケープします。
- C++ policy読込は既存の数値トークン抽出処理を使い、同梱policyのtau=1.5を
  正しく読み込みます。候補cap・期限・入力長・文脈長も同じ処理で取得します。
- Windows smokeはインストール済みONNXの実SHAとping・scored responseのSHAを照合し、
  別モデルが稼働している場合に失敗します。MSI抽出後のCI smokeにも同じ照合を追加しました。
  また、標準Windows PowerShell 5.1が日本語を誤読しないようsmokeをUTF-8 BOM付きにしました。

修正後のCPUテストはreranker **62件**、dataset **14件**が成功しました。
追加9件は実C++診断モジュールと実Abseilパーサーで検証します。
同じ回帰テストが修正前commitのtau不一致と応答本文の混入を検出することも確認しました。
Windowsでは実smoke内のSHA照合関数を8ケースで検証するCIを追加し、
Windows PowerShellとPowerShell 7の両方で実行します。
これはモデル識別処理の検証であり、MSIインストールやIME組み込み試験は引き続き未実施です。
