// Copyright 2024 AI Mozc IME Project
// AI Rewriter Header - Mozc Integration Version
// CRITICAL: This rewriter MUST NEVER block or freeze

#ifndef MOZC_REWRITER_AI_REWRITER_H_
#define MOZC_REWRITER_AI_REWRITER_H_

#include <memory>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

// Mozc-style includes (for integration with google/mozc)
#include "rewriter/rewriter_interface.h"
#include "ai/ai_candidate_cache.h"
#include "ai/ai_worker.h"

namespace mozc {

// AIRewriter: Adds AI-generated candidates to Mozc conversion
//
// ╔═══════════════════════════════════════════════════════════════════╗
// ║  DESIGN PRINCIPLE: This class MUST NEVER block or freeze         ║
// ║                                                                   ║
// ║  - Rewrite() returns immediately                                  ║
// ║  - AI candidates come from cache (instant lookup)                 ║
// ║  - If cache miss, request is queued for background processing    ║
// ║  - Results will be available for NEXT conversion                 ║
// ╚═══════════════════════════════════════════════════════════════════╝
class AIRewriter : public RewriterInterface {
 public:
  AIRewriter();
  ~AIRewriter() override;

  // Capability: Only CONVERSION for now
  int capability(const ConversionRequest& request) const override;

  // Rewrite: Add AI candidates (NON-BLOCKING)
  bool Rewrite(const ConversionRequest& request,
               Segments* segments) const override;

  // Clear state
  void Clear() override;

  // Check if AI is enabled and ready
  bool IsEnabled() const;

  // Get statistics
  struct Stats {
    int64_t rewrite_calls = 0;
    int64_t cache_hits = 0;
    int64_t cache_misses = 0;
    int64_t candidates_added = 0;
  };
  Stats GetStats() const;

 private:
  // Lazy initialization (on first Rewrite call)
  void EnsureInitialized() const;

  // Get cache key for input (may include context hash)
  std::string GetCacheKey(const std::string& input) const;

  // Get input key from segments
  std::string GetInputKey(const Segments& segments) const;

  // Get existing candidates from Mozc
  std::vector<std::string> GetExistingCandidates(
      const Segments& segments, int max_count) const;

  // Request AI candidates (non-blocking)
  void RequestAICandidates(
      const std::string& key,
      const Segments& segments) const;

  // Insert AI candidates into segments
  void InsertCandidates(
      const std::vector<std::string>& candidates,
      Segments* segments) const;

  // Check if candidate already exists
  bool IsDuplicate(
      const std::string& candidate,
      const Segments& segments) const;

  // Update context history
  void UpdateContext(const Segments& segments) const;

  // Components (mutable for lazy init in const Rewrite)
  mutable std::unique_ptr<ai::AICandidateCache> cache_;
  mutable std::unique_ptr<ai::AIWorker> worker_;

  // Initialization state
  mutable std::mutex init_mutex_;
  mutable std::atomic<bool> initialized_{false};
  mutable std::atomic<int> init_failures_{0};

  // Context history for better predictions
  mutable std::deque<std::string> context_history_;
  mutable std::mutex context_mutex_;

  // Statistics
  mutable std::mutex stats_mutex_;
  mutable Stats stats_;

  // Configuration
  static constexpr int kMaxInitFailures = 3;
  static constexpr int kMaxContextHistory = 5;
  static constexpr int kMaxCandidatesToAdd = 3;
  static constexpr const char* kCandidateDescription = "AIが生成";
};

}  // namespace mozc

#endif  // MOZC_REWRITER_AI_REWRITER_H_
