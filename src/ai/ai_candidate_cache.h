// Copyright 2024 AI Mozc IME Project
// AI Candidate Cache Header
// Thread-safe cache for AI-generated candidates

#ifndef MOZC_AI_AI_CANDIDATE_CACHE_H_
#define MOZC_AI_AI_CANDIDATE_CACHE_H_

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace mozc {
namespace ai {

// AI generated candidate entry with expiration
struct AICandidateEntry {
  std::vector<std::string> candidates;
  std::chrono::steady_clock::time_point created_at;
  std::chrono::steady_clock::time_point expires_at;
};

// Thread-safe cache for AI candidates
// Key design principles:
// 1. Get() MUST be non-blocking and return immediately
// 2. Put() can be called from worker thread
// 3. All operations are thread-safe
class AICandidateCache {
 public:
  // Constructor with cache configuration
  // @param max_entries Maximum number of entries to store
  // @param ttl_seconds Time-to-live for each entry in seconds
  explicit AICandidateCache(int max_entries = 100, int ttl_seconds = 60);

  ~AICandidateCache() = default;

  // Get candidates for a key (non-blocking, returns immediately)
  // Returns nullopt if key not found or expired
  std::optional<std::vector<std::string>> Get(const std::string& key) const;

  // Store candidates for a key (called from background worker)
  void Put(const std::string& key, std::vector<std::string> candidates);

  // Clear all entries
  void Clear();

  // Remove expired entries
  void Prune();

  // Check if key exists and is not expired
  bool Contains(const std::string& key) const;

  // Get current cache size
  size_t Size() const;

  // Cache statistics
  struct Stats {
    int64_t hits = 0;
    int64_t misses = 0;
    int entries = 0;
    double hit_rate = 0.0;
  };

  // Get cache statistics
  Stats GetStats() const;

  // Reset statistics
  void ResetStats();

 private:
  // Remove oldest entry when cache is full
  void EvictOldest();

  // Check if entry is expired
  bool IsExpired(const AICandidateEntry& entry) const;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, AICandidateEntry> cache_;

  int max_entries_;
  std::chrono::seconds ttl_;

  // Statistics (mutable for const Get() method)
  mutable int64_t hits_ = 0;
  mutable int64_t misses_ = 0;
};

}  // namespace ai
}  // namespace mozc

#endif  // MOZC_AI_AI_CANDIDATE_CACHE_H_
