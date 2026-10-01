調査日：2026年9月30日（日本時間）。対象は「MozcAI改良計画」の過去の会話、shumaimai/Mozc-Ai-Training、配布側のshumaimai/Mozc-Ai、および判断モデルの一次資料です。今回は読み取りと分析を行い、学習・モデル変更・PR作成・マージ・公開は行っていません。

結論は、MozcAIに最も取り込みやすいのは「日本語を理解する小型モデルに、動的な候補集合を比較する学習と、上書きを控える判断を追加する」方法です。Jeffの学習手順、Kevのpointer head、Nimbleの反実仮想データ、NanoJevの確率学習と評価の分離が参考になります。既存LLMの層数を減らすだけで、Jevの学習方法や速度を再現できるとはいえません。

以下では、公開コード・結果で確認した事実と、MozcAIへの提案を区別しています。

**MozcAIの現状を確認すると、出荷構成と二つの研究経路が存在します。**

| 経路 | 読んだ資料・実装 | 確認できた内容 |
| --- | --- | --- |
| main／配布版 | Training README、Mozc-Ai README | ModernBERT-ja-30Mの文脈クロスエンコーダー、ONNX FP32、読み・文脈・候補を入力し、マージン条件でMozcの候補順を変更 |
| experiment/sarashina-jev-poc | model.py、data.py、train_distill.py、結果README、summary.json | Sarashina2.2-0.5BのLM headを使わず、層削減＋スカラーのscore head。5候補のlistwise学習、8L→6L蒸留、QAT、語彙削減を実施 |
| v2/contextual-ranking-reset | restart計画、train_cross_encoder.py、Phase 2の学習・パリティ報告 | 実Mozc候補とメタデータ、文書分離、学習・評価・ランタイムの入力一致を整備し、ModernBERT-ja-30Mを再学習。mainへのマージは報告上保留 |

mainの出荷版READMEにはWikipedia由来のseen/unseen/freshで89.36%／89.98%／92.38%とあります。一方、旧DECISION_RERANKには実使用186件で、ガードなしがMozcより−8.1pt、ガード後が＋0.5ptという別の評価があります。これは「公開コーパスで良い」ことと「実際に入力する文章で良い」ことが一致しなかった例です。旧資料の既定OFFなどを、そのまま現在の配布版の設定だとは扱いません。

Sarashinaの最新保存結果は次のとおりです。両モデルは64k語彙、Dynamic INT8＋FP16 embeddingの組み合わせです。

| モデル | 従来204ページ | 新規800ページ | サイズ | CPU p50 |
| --- | ---: | ---: | ---: | ---: |
| 8L教師 | 81.3725% | 61.2500% | 326.06 MiB | 157.82 ms |
| 6L QAT v2 | 80.8824% | 58.1250% | 283.65 MiB | 120.28 ms |

この評価は実Mozc N-bestではありません。Wikipediaから作った5候補のproxyで、正解が必ず候補に含まれ、候補順はコーパス頻度順です。candidate 0は実Mozcのベースラインではありません。204件では1問が約0.49ptなので、既存評価の1問差から同等性能を断定できません。800件の別記事holdoutでは6Lが8Lより3.125pt下がり、結果資料も6Lを条件付き候補として扱っています。30Mモデルの別データの数字と直接比較しません。

INT8の大幅な精度崩壊についても、後の記録を優先しました。同じONNXが別の新しいコンテナで正常に近い精度に戻り、graphやembedding比較からCPU／ORTの量子化kernelの問題が疑われています。原因が完全確定したという扱いはしません。古い崩壊値だけを根拠に「QATモデルが壊れた」「INT8化は不可能」と結論づけるのは適切ではありません。

新しいv2では47,589学習グループ、284,957ペアで30Mを学習し、5,974 validationグループで評価しています。教師なしの通常のペア分類学習で、実装の損失はBCEWithLogitsLossです。純粋なニューラル順位、マージン適用後、出荷ポリシー適用後は別の結果です。

| v2の評価 | 結果 |
| --- | --- |
| NEURAL_ELIGIBLE、4,021件、ニューラルのみ | Mozc 82.9396% → 86.4710% |
| ALL、5,974件、マージンのみの最良点 | Mozc 74.5899% → 77.2514%（tau=3.0） |
| 実際のポリシー再生、safety mode・tau=2.5 | 74.590% → 76.364%、helped 153／hurt 47 |
| strict allowlistのポリシー再生 | 74.757%。5,974件中90件しか推論に到達しない |
| 5800X3Dで常駐daemonへのTCP往復 | p50 18.6／p95 78.0／p99 114.0 ms、5,974件の200ms超過なし |

最後の速度はORT 1.30.0、intra_op=4、ウォームアップ後の記録です。GPUのバッチthroughputやSarashinaのModal CPU速度とは比較条件が違います。validationの結果であり、final_testは未使用です。ポリシー込みの76.364%を、マージンだけの77.2514%と取り違えないことが重要です。safety modeも現行mainの既定だとは扱いません。

参照：[main README](https://github.com/shumaimai/Mozc-Ai-Training/blob/3fc8339e67dc014651dc9862564e6efc55bde470/README.md)、[旧実使用評価](https://github.com/shumaimai/Mozc-Ai-Training/blob/3fc8339e67dc014651dc9862564e6efc55bde470/docs/reranker/plans/DECISION_RERANK.md)、[Sarashina結果](https://github.com/shumaimai/Mozc-Ai-Training/blob/d6489bc93ed2cdc4ddf4c2e3fe76bf67a7934076/docs/sarashina_jev/results/README.md)、[proxyデータの説明](https://github.com/shumaimai/Mozc-Ai-Training/blob/d6489bc93ed2cdc4ddf4c2e3fe76bf67a7934076/data/public/sarashina_jev_proxy/README.md)、[v2 baseline](https://github.com/shumaimai/Mozc-Ai-Training/blob/678870e6ec2e94f1c85e468616f104f1ea457063/docs/contextual_ranking_v2/PHASE2_BASELINE_REPORT.md)、[v2 runtime検証](https://github.com/shumaimai/Mozc-Ai-Training/blob/678870e6ec2e94f1c85e468616f104f1ea457063/docs/contextual_ranking_v2/PHASE2_PARITY_REPORT.md)。

**Jev本体の公開説明と、公開されている代替モデルの実装は分けて理解する必要があります。**

TypeSafe公式は、新しいarchitecture、parallel sampler、Reinforcement Learning for Calibrated Decisions（RLCD）を説明し、生成文章ではなく型付き判断と確率を返すとしています。しかし確認した公式資料には、学習を再現できるreward式・optimizer・データ一式・実装が示されていません。JeffやKevの公開コードは、Jevの内部を回収したものではなく、似た入出力を独立に実装したものです。API互換から内部構造の同一性は推定できません。

また、「文章を生成しない」ことと「判断を間違えない」ことは別です。型が正しい答えでも誤判定は起きます。確率校正も、90%とした予測群がおよそ90%当たることを目標とするもので、個々の判断の保証ではありません。

参照：[TypeSafe公式発表](https://typesafe.ai/blog/introducing-system-one-models-and-jev)、[公式AI primer](https://docs.typesafe.ai/introduction/machine-learning-primer)。

**主要なオープンウェイトの作り方は、回答トークンを読む方式と、判断ヘッドを学習する方式に分かれます。**

| プロジェクト | 基盤・公開物 | 判断の取り出し方 | 学習方法 | MozcAIに取り込める点 |
| --- | --- | --- | --- | --- |
| Jeff 1 | Qwen3-4B-Instruct-2507＋公開LoRA adapter。推論には4B本体も必要 | LMのラベルtoken logitsを読む。独立した分類headはない | 回答ラベルtokenだけを教師あり学習。教師Jevのラベルと人手ラベルを使用 | 簡潔な判断専用学習、教師一致率と真の正答率の分離、学習・推論promptの一致 |
| Kev | Qwen3.5／3.8の0.8B〜27B＋LoRA＋pointer head | 質問のdecide表現と各候補の表現を照合し、候補集合にsoftmax | 主に候補分布上のCE、公開データ＋生成policy／rule、候補順変更・distractor・none。温度校正 | 動的候補を意味から選ぶhead、候補集合単位の損失、判断の留保、候補順耐性 |
| Bespoke Nimble | Qwen3.5-9B、重み・データ・レシピ | 各選択肢を1 tokenのコードに対応させ、allowed token logitsを読む | LoRA＋許可候補間CE、1,338組・2,676件の反実仮想対を使用した公開レシピ | わずかな文脈差で正解が変わる対の作成・検証、対を同じsplitに保つ |
| NanoJev | Qwen3-0.6B＋structured decision heads、checkpoint・データ・学習コード | 候補pathをバッチで符号化し、Choiceはset attention＋softmaxなどで判断 | programmaticな正解／分布、教師分布、直接CE・Brier・RLCD-inspired実験 | 分布そのものの学習、有限候補では単純な適正損失を基準にする、バッチ化の実態を測る |

各モデルの汎用的な英語評価は、日本語IMEの性能を直接証明しません。サイズも30Mより大きく、公開モデルをそのまま差し替えれば速くなるという根拠はありません。設計・データ・評価の工夫を借りるのが現実的です。

**Jeffを詳しく見ると、既存LLMの知識を残し、出力の読み方を判断専用にしています。**

公開版はLoRA rank 16、alpha 32、dropout 0.05で、attentionのq/k/v/o projectionを更新します。学習するadapterは約11.8Mパラメータですが、推論モデルが11.8Mになるわけではありません。4.03Bの基盤も動きます。

学習例は「state・質問・ラベル定義 → 正解ラベル」だけです。promptとpaddingのlossをマスクし、回答ラベル部分だけを教師にします。公開版はChoice 9,119件、Score 1,800件、Noul 1,200件の計12,119行を2 epoch。Choice用SFTの作成コードではJevの教師分布のargmaxを主にラベルにします。教師の分布を保存していることと、公開adapterがその分布に対するKL蒸留で学習されていることは同義ではありません。

推論時は、各ラベルの最初のtokenが異なればそのlogitをsoftmaxし、同じtokenから始まるラベルがあれば全ラベルをsequence scoringへ切り替えます。例えば空白tokenが先に出る数値ラベルでは、最初のtokenだけを比べると全選択肢が同じ確率になる問題を避けています。これは、日本語の候補文字列をそのまま先頭tokenで比較する方式にも注意を促します。同じ最初の漢字やsubwordが複数候補にあれば区別できません。

公開model cardの9,730人手ラベル評価ではJeff 81.83%、live Jev 82.83%。最大クラス確率を用いた同じECE定義ではJeff 0.0807、Jev 0.0932ですが、BrierではJevの方が良い結果です。開発中のエラー分析にも使われた評価なので、完全未見の独立holdoutとは区別されています。語彙確率や「confidence」という名前だけを正解率とみなさず、同じ指標で比較する姿勢が参考になります。

参照：[model card](https://github.com/Gestalt-Lab/jeff/blob/14ee67e2814a19ebd0a67f508ac1b0f424ba1ebc/MODEL_CARD_JEFF1.md)、[学習手順](https://github.com/Gestalt-Lab/jeff/blob/14ee67e2814a19ebd0a67f508ac1b0f424ba1ebc/docs/TRAIN_YOUR_OWN.md)、[readout.py](https://github.com/Gestalt-Lab/jeff/blob/14ee67e2814a19ebd0a67f508ac1b0f424ba1ebc/jev_clf/readout.py)、[SFTデータ作成](https://github.com/Gestalt-Lab/jeff/blob/14ee67e2814a19ebd0a67f508ac1b0f424ba1ebc/scripts/jev_clf_sft_data.py)、[公開adapter](https://huggingface.co/GestaltLabs/Jeff-1)。

**Kevのpointer headは、Mozcの変換候補のように毎回選択肢が変わる問題に適しています。**

候補を固定のクラス番号として覚えさせず、候補の文字列を入力して意味を読ませます。コードでは質問末尾のhidden stateをq、各候補末尾のhidden stateをkに射影し、内積で各候補を採点します。候補数Kに応じてsoftmaxするので、候補表面形が変わっても同じheadが働きます。

学習はLoRAとheadを共同更新し、他の基盤重みは固定します。0.8B／4B／9Bの基礎レシピには10,000公開例、896 policy例、1,680生成rule例があり、教師Jev出力を使っていません。candidate permutationやnone-of-the-aboveなどを加え、後続学習では決め手となる証拠を除いた例も扱います。情報がない場合のuniform targetは、そのプロジェクトの条件での設計です。Mozcには順位・頻度という事前情報があるため、そのまま全候補を一様にするべきではありません。

並列性にも実装上の条件があります。attention-onlyのモデルではbranch maskでstateを共有し質問を分離できますが、Qwen3.5のDeltaNetはattention maskだけで分離できないので独立rowを使います。serving側はstate cacheを再利用します。単に「1回のforward」と書かれていても、文脈を1回しか計算していないとは限りません。

公開checkpointはheld-outで温度Tをfitします。温度は順位を変えず、確率の尖り具合を変えます。校正しただけで誤判定を治せるわけではありません。候補順不変性も通常のcausal入力なら自動ではなく、順序変更と、意味・メタデータを保ったcandidate ID変更の検証が必要です。

参照：[READMEのarchitecture・training](https://github.com/jaredpalmer/kev/blob/main/README.md)、[model.py](https://github.com/jaredpalmer/kev/blob/main/kev/model.py)、[0.8B model card](https://github.com/jaredpalmer/kev/blob/main/docs/model-cards/kev-0.8b.md)、[公開モデル一覧](https://huggingface.co/collections/jaredpalmer/kev-6aad9d0ea49f2589665e07cd)。

**Nimbleの価値は、単なる文章量より「何を変えると判断が変わるか」をデータに入れることです。**

公開データは1,338組のbase／counterfactualです。文脈の決め手となる事実を変えて正解を反転させます。合成ラベルをモデルで検証し、source familyをholdoutと分け、対になる例を同じsplitに置きます。人手確認済みデータという扱いではありません。公開学習レシピはBF16 LoRA、許可候補logits間のCEで、生成推論文や教師確率を訓練対象にしません。

IMEへの提案は、例えば同じ読み「きしゃ」と同じ候補集合を使って、直前の文脈を「鉄道の話を続けている場面」と「新聞の取材を話している場面」で対にすることです。前者で汽車、後者で記者を優先する対を作り、実際のMozc候補・対象segment・入力できる左文脈を使って成立を確認します。対象語そのものを文脈へ紛れ込ませたり、実際には見えない右文脈から正解を決めたりしません。これは提案であり、今のモデルがその例に正答するという測定結果ではありません。

参照：[Nimble README](https://github.com/bespokelabsai/nimble/blob/main/README.md)、[公開データ](https://github.com/bespokelabsai/nimble/blob/main/docs/DATASET.md)、[学習レシピ](https://github.com/bespokelabsai/nimble/blob/main/docs/NIMBLE_TRAINING.md)、[公開モデル](https://huggingface.co/bespokelabs/Bespoke-Nimble-9B)。

**NanoJevのRLCD-inspired実験は、確率学習と「行動を選ぶこと」を区別する参考になります。**

候補を一括符号化してheadで採点し、直接CE、直接Brier、sampledなproper rewardのpolicy gradientを比較しています。後者はTypeSafeの未公開手法を再現したという主張ではなく、独立した研究候補です。正解したら報酬、だけのREINFORCEは最も有利なクラスへ確率を集中させやすく、校正された分布の学習とは目的が違います。

そのsampled rewardの期待値は 2 p・q − ||p||² = ||q||² − ||p−q||² で、Brierに対応する目的です。小さい有限候補集合では、直接Brierを計算すればsampling varianceを追加せずに済みます。Qwen実験には有効な結果がありますが、1 seed・限定した環境なのでRLがCEや直接Brierより一般的に優れるとは結論していません。

Choiceのsoftmaxが「どの行動を選ぶか」の分布であっても、その行動が成功する確率とは限りません。IMEでも候補選好のsoftmaxを、その候補がユーザーの意図に合う確率や、上書きの安全性と直結させない設計が必要です。

参照：[NanoJev README](https://github.com/TianyuCodings/NanoJev/blob/76fdfc9ecdca45a9bcef17991a07d3041a87685a/README.md)、[pipeline](https://github.com/TianyuCodings/NanoJev/blob/76fdfc9ecdca45a9bcef17991a07d3041a87685a/research/pipeline_runbook.md)、[RLCD実験と測定](https://github.com/TianyuCodings/NanoJev/blob/76fdfc9ecdca45a9bcef17991a07d3041a87685a/docs/RLCD_EXPERIMENT.md)、[loss実装](https://github.com/TianyuCodings/NanoJev/blob/76fdfc9ecdca45a9bcef17991a07d3041a87685a/scripts/calibrated_objectives.py)、[公開checkpoint](https://huggingface.co/C-Tianyu/NanoJev)。

**MozcAIには、既にある機能を土台に、次の順番で応用することを提案します。**

| 優先度 | 施策 | 今回の調査から追加できるもの | 既存施策との違い |
| --- | --- | --- | --- |
| 1 | v2のデータ契約と30M baselineを維持し、対になる難例を追加 | 同じ読み・候補で文脈だけ変える、情報削除で曖昧にする、短い読み・日常語・似た意味の誤候補 | v2の入力一致・文書分離は既に整備済み。やり直さず難例のcoverageを広げる |
| 2 | 候補集合単位の学習を30Mで比較 | ペアBCEをbaselineに、実top-Kを使ったlistwise CE／分布蒸留をablation | Sarashinaにはlistwiseが既にある。新提案はv2の実候補データと30Mへ適用すること |
| 3 | 上書きの留保と校正 | 候補が曖昧・正解が候補外・Mozc top1が妥当な例を学習し、変更の有益性を別に評価 | 既存のマージン・ガード・Mozc維持anchorを残し、固定閾値だけに頼る判断を改善 |
| 4 | 小型page-wise／set-aware headの研究 | Kevのように候補文字列とstateを照合するhead、必要に応じた軽い候補間比較 | 30Mに大きいQwenをそのまま置換しない。二重符号化とjoint入力の速度・精度を比較 |
| 5 | 有効な教師から小さいstudentへ蒸留 | 真の日本語IME validationで強い教師を確認し、必要な難例・候補分布を渡す | 既存の8L→6L CE＋KL＋score MSE＋margin蒸留を「未実装」とは扱わない |
| 6 | 配布CPUで量子化 | FP32→対応するINT8／混合精度を同じCPU・ORT・thread条件で比較 | 語彙削減は速度改善と同義でない。既知のORT anomalyを確認し、校正とポリシーも再評価 |

「候補集合を学習する」損失の単純な比較案は、p_i = softmax(s_i/T)、L_rank = −log p_gold です。蒸留を足す場合はL = L_rank + λ T² KL(q_teacher || p_student)を基本controlにし、教師・studentとも同じ実候補集合を使います。これは提案であり、最良のλやTが確定しているわけではありません。曖昧な例で複数の表記が正当なら、公開コーパスの1表面形だけを唯一の正解と決めつけず、複数許容ラベルや妥当性確認を扱います。

温度校正は候補順を変えません。「confidence 0.9で上書き」のような値を先に決めず、held-out calibrationで上書き後のhurt率とcoverageを測定して動作点を決めます。候補数Kが変わるとsoftmaxの尺度も変わるので、候補数・文脈長・読み長ごとの校正を確認します。候補内の選好分布と「上書きが有益か」の確率は別物として扱います。

Mozcのrank・cost・ユーザー履歴等はv2の候補metadataとして存在しますが、確認した30M formatterは読み・文脈・候補文字列を入力しています。提案としては、モデルのスコアにMozcの事前情報を加えた軽いfusionを比較できます。raw costを適当な係数で足すより、cost_delta等を正規化し、validationだけで係数を決め、元のguardやhard-protectを含めて再生します。

速度については、Sarashinaの現行inputが「候補→読み→文脈→判定」なので、candidateごとに文脈を再計算します。batch×候補を1 forwardで処理していても、この重複は残ります。文脈を先頭に置いたcache共有やstate／candidateを分けたheadは研究候補ですが、入力順変更には再学習と新しいパリティ検証が必要です。双方向ModernBERTのjoint cross-encoderには、decoderのprefix KV cacheをそのまま適用できません。1件の変換で文脈が短いMozcでは、複雑なcacheの管理費が利益を上回る可能性もあり、実測で採否を決めます。

**コードへ対応させるなら、次の変更単位で切り分けると比較しやすくなります。**

| 既存コード・資料 | 現在の役割 | 提案する実験 |
| --- | --- | --- |
| v2 tools/rerank/train_cross_encoder.py | 読み・文脈・候補のペアBCE | 同じ初期model・splitでgroup/listwise版と比較 |
| v2 tools/rerank/contextual_ranking_v2_contract.py | canonical format_v2 | 全経路で継続使用。新入力を使う実験は別契約にして混在を防ぐ |
| v2 Dataset／phase1生成・監査 | 実候補、対象segment、文書分離、runtime context | contrast pair、情報削除例、候補外正解をgroup単位で保持 |
| v2 tools/rerank/policy_replay.py | guard→score→margin→revertの再生 | calibration・変更gate・metadata融合を同じchainで比較 |
| Sarashina tools/sarashina_jev/data.py | 5候補ページ、Mozc維持anchor、gold page shuffle | 実N-best・可変候補数への対応は研究用の別経路で比較 |
| Sarashina tools/sarashina_jev/train_distill.py | CE・KL・中心化MSE・margin蒸留 | proxy教師での追加圧縮より、真のvalidationで教師の有効性確認を先行 |
| model.py／ONNX export・parity | backbone→score head、候補batch | 新headなら動的候補・mask・tokenization・policyの一致を検証 |

参照：[v2 trainer](https://github.com/shumaimai/Mozc-Ai-Training/blob/678870e6ec2e94f1c85e468616f104f1ea457063/tools/rerank/train_cross_encoder.py)、[Sarashina model](https://github.com/shumaimai/Mozc-Ai-Training/blob/d6489bc93ed2cdc4ddf4c2e3fe76bf67a7934076/tools/sarashina_jev/model.py)、[Sarashina data](https://github.com/shumaimai/Mozc-Ai-Training/blob/d6489bc93ed2cdc4ddf4c2e3fe76bf67a7934076/tools/sarashina_jev/data.py)、[蒸留の実装](https://github.com/shumaimai/Mozc-Ai-Training/blob/d6489bc93ed2cdc4ddf4c2e3fe76bf67a7934076/tools/sarashina_jev/train_distill.py)、[v2の再出発方針](https://github.com/shumaimai/Mozc-Ai-Training/blob/678870e6ec2e94f1c85e468616f104f1ea457063/docs/CONTEXTUAL_RANKING_V2_RESTART.md)。

**評価は、モデルが当たるかと、IME全体が改善するかの両方を測ります。**

- 同一の文書分離validationでMozc top1、top-K oracle、ニューラルraw、policy後のHit@1を並べる。候補にない正解をモデルの順位付けで救えるとは扱わない。
- helped／hurt／overwrite数と、hurt制約内のcoverageを報告する。正解率だけ高くても誤った上書きが多いmodelは採用しない。
- 未見source・未見reading・会話的短文を分けて確認する。人の意図が特定できない例は単一goldの意味も点検する。
- 対の両方で正答する率を測る。片側だけ高い頻度語を当てても、文脈依存の能力とみなさない。
- 文脈shuffleと情報削除をcontrolにする。文脈shuffleで性能がほとんど変わらないなら、候補頻度などのshortcutを疑う。全例で機械的にchanceへ下がることを要求するわけではない。
- candidate表示順を変え、rank／cost等を候補と一緒に動かしたときに意味が保たれるか確認する。数値rankまで消す実験とは区別する。
- NLL・Brier・ECEを同じ定義で報告し、teacher agreementは真のgold accuracyと別に出す。
- CPU常駐・batch=1 conversion・実top-Kでp50／p95／p99、200ms超過、RSS、サイズを測る。GPUの大量batchのms/groupをIME latencyと表現しない。
- 学習framework、ONNX、tokenizer、runtime input、最終policyの一致を確認する。量子化後は確率と上書き閾値も再点検する。
- final_testはmodel選択や今回の資料調査に使わない。個人の入力履歴は既存方針どおりローカルに保持する。

最も筋のよい次の実験は「v2の30M baselineに、文脈反転の対と実候補集合単位の学習を加え、同じvalidationとpolicy再生でhelped／hurtを比較する」ものです。そこで一般化の改善が得られた後、必要な場合にpointer／set headや小型studentへ進むのが、既存の成果を活かしながら判断能力を改善する順序だと考えます。
