# Mozc Compatibility Layer

このディレクトリには、google/mozc リポジトリに直接統合できるバージョンのファイルが含まれています。

## ファイル一覧

| ファイル | 説明 |
|---------|------|
| `ai_rewriter.h` | Mozc用インクルードパスに修正済みのヘッダー |
| `ai_rewriter.cc` | Mozc APIに対応した実装ファイル |

## 主な変更点

### 1. インクルードパス

```cpp
// スタンドアロン版 (src/rewriter/)
#include "rewriter_interface.h"
#include "../ai/ai_config.h"

// Mozc統合版 (mozc_compat/)
#include "rewriter/rewriter_interface.h"
#include "ai/ai_config.h"
```

### 2. Mozc API対応

```cpp
// スタンドアロン版
segment.key                    // 直接アクセス
segment.candidate(i).value     // 直接アクセス
segment->add_candidate()       // 直接追加

// Mozc統合版
segment.key()                  // メソッド呼び出し
segment.candidate(i).value()   // メソッド呼び出し
segment->push_back_candidate() // Mozc API
```

### 3. ロギング

```cpp
// スタンドアロン版
#define AI_LOG(msg) std::cerr << "[AI-Mozc] " << msg << std::endl

// Mozc統合版
#include "base/logging.h"
LOG(INFO) << "[AI-Mozc] " << msg;
```

## 使用方法

1. これらのファイルを `mozc/src/rewriter/` にコピー
2. AIモジュール (`src/ai/*`) を `mozc/src/ai/` にコピー
3. `scripts/integrate_mozc.sh` または `scripts/integrate_mozc.ps1` を実行

詳細は `docs/MOZC_INTEGRATION.md` を参照してください。
