// Copyright 2024 AI Mozc IME Project
// AI Candidate Cache Implementation

#include "ai/ai_candidate_cache.h"

#include <algorithm>
#include <limits>

namespace mozc {
namespace ai {

AICandidateCache::AICandidateCache(int max_entries, int ttl_seconds)
    : max_entries_(max_entries),
      ttl_(std::chrono::seconds(ttl_seconds)) {}

std::optional<std::vector<std::string>> AICandidateCache::Get(
    const std::string& key) const {

  std::lock_guard<std::mutex> lock(mutex_);

  auto it = cache_.find(key);
  if (it == cache_.end()) {
    ++misses_;
    return std::nullopt;
  }

  // Check expiration
  if (IsExpired(it->second)) {
    ++misses_;
    return std::nullopt;
  }

  ++hits_;
  return it->second.candidates;
}

void AICandidateCache::Put(const std::string& key,
                           std::vector<std::string> candidates) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check size limit
  if (cache_.size() >= static_cast<size_t>(max_entries_)) {
    // First, try to prune expired entries
    auto now = std::chrono::steady_clock::now();
    for (auto it = cache_.begin(); it != cache_.end();) {
      if (now > it->second.expires_at) {
        it = cache_.erase(it);
      } else {
        ++it;
      }
    }

    // If still full, evict oldest
    if (cache_.size() >= static_cast<size_t>(max_entries_)) {
      EvictOldest();
    }
  }

  // Create new entry
  AICandidateEntry entry;
  entry.candidates = std::move(candidates);
  entry.created_at = std::chrono::steady_clock::now();
  entry.expires_at = entry.created_at + ttl_;

  cache_[key] = std::move(entry);
}

void AICandidateCache::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_.clear();
}

void AICandidateCache::Prune() {
  std::lock_guard<std::mutex> lock(mutex_);

  auto now = std::chrono::steady_clock::now();
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (now > it->second.expires_at) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}

bool AICandidateCache::Contains(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = cache_.find(key);
  if (it == cache_.end()) {
    return false;
  }

  return !IsExpired(it->second);
}

size_t AICandidateCache::Size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return cache_.size();
}

AICandidateCache::Stats AICandidateCache::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);

  Stats stats;
  stats.hits = hits_;
  stats.misses = misses_;
  stats.entries = static_cast<int>(cache_.size());

  int64_t total = hits_ + misses_;
  if (total > 0) {
    stats.hit_rate = static_cast<double>(hits_) / static_cast<double>(total);
  }

  return stats;
}

void AICandidateCache::ResetStats() {
  std::lock_guard<std::mutex> lock(mutex_);
  hits_ = 0;
  misses_ = 0;
}

void AICandidateCache::EvictOldest() {
  // Mutex is already held by caller

  if (cache_.empty()) {
    return;
  }

  // Find oldest entry
  auto oldest_it = cache_.begin();
  auto oldest_time = oldest_it->second.created_at;

  for (auto it = cache_.begin(); it != cache_.end(); ++it) {
    if (it->second.created_at < oldest_time) {
      oldest_time = it->second.created_at;
      oldest_it = it;
    }
  }

  cache_.erase(oldest_it);
}

bool AICandidateCache::IsExpired(const AICandidateEntry& entry) const {
  return std::chrono::steady_clock::now() > entry.expires_at;
}

}  // namespace ai
}  // namespace mozc
