// Copyright 2024 AI Mozc IME Project
// AI Rewriter Implementation - Mozc Integration Version
// CRITICAL: All methods in this file MUST be non-blocking

#include "rewriter/ai_rewriter.h"

#include "ai/ai_backend.h"
#include "ai/ai_config.h"
#include "ai/ai_logger.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

#include "absl/log/log.h"
#include "absl/strings/string_view.h"

#include <algorithm>
#include <mutex>

namespace mozc {

namespace {
void LogRewriterOnce() {
  static std::once_flag once;
  std::call_once(once, []() {
    ai::AILogger::Info("AIRewriter active");
  });
}
}  // namespace

AIRewriter::AIRewriter() {
  ai::AILogger::EnsureOpen();
  ai::AILogger::Info("AIRewriter constructed");
}

AIRewriter::~AIRewriter() {
  // Stop worker thread gracefully
  if (worker_) {
    worker_->Stop();
  }
}

int AIRewriter::capability(const ConversionRequest& request) const {
  if (request.request_type() == ConversionRequest::CONVERSION) {
    return RewriterInterface::CONVERSION;
  }
  return RewriterInterface::NOT_AVAILABLE;
}

bool AIRewriter::Rewrite(const ConversionRequest& request,
                         Segments* segments) const {
  // ╔════════════════════════════════════════════════════════════════════╗
  // ║ CRITICAL: This function MUST return immediately                    ║
  // ║ - No blocking operations                                           ║
  // ║ - No network calls                                                 ║
  // ║ - No waiting for AI response                                       ║
  // ╚════════════════════════════════════════════════════════════════════╝

  LogRewriterOnce();

  // Update statistics
  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ++stats_.rewrite_calls;
  }

  // Check configuration
  auto config = ai::AIConfigManager::Instance().GetConfig();
  if (!config.enabled || config.debug.disable_ai) {
    return true;  // AI disabled, return immediately
  }

  // Check segments
  if (!segments || segments->conversion_segments_size() == 0) {
    return true;  // Nothing to process
  }

  // Get input key
  std::string key = GetInputKey(*segments);
  if (key.empty()) {
    return true;  // No input
  }

  // Step 1: Lazy initialization (if needed)
  try {
    EnsureInitialized();
  } catch (...) {
    // Initialization failed, continue without AI
    ai::AILogger::Error("AIRewriter initialization failed");
    return true;
  }

  // Step 2: Check cache for AI candidates (instant lookup)
  std::vector<std::string> ai_candidates;
  if (cache_) {
    std::string cache_key = GetCacheKey(key);
    auto cached = cache_->Get(cache_key);
    if (cached.has_value()) {
      ai_candidates = std::move(*cached);

      std::lock_guard<std::mutex> lock(stats_mutex_);
      ++stats_.cache_hits;
    } else {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      ++stats_.cache_misses;
    }
  }

  // Step 3: If cache miss, queue request for background processing
  // The result will be available for the NEXT conversion
  if (ai_candidates.empty() && worker_ && worker_->IsBackendReady()) {
    RequestAICandidates(key, *segments);
    // DO NOT WAIT! Continue with Mozc candidates only
  }

  // Step 4: If we have cached candidates, add them to segments
  if (!ai_candidates.empty()) {
    InsertCandidates(ai_candidates, segments);
  }

  // Step 5: Update context for future predictions
  UpdateContext(*segments);

  // Always return true (we never fail the rewriting)
  return true;
}

void AIRewriter::Clear() {
  // Clear context history
  {
    std::lock_guard<std::mutex> lock(context_mutex_);
    context_history_.clear();
  }

  // Note: We don't clear cache as it may contain useful predictions
}

bool AIRewriter::IsEnabled() const {
  auto config = ai::AIConfigManager::Instance().GetConfig();
  return config.enabled && !config.debug.disable_ai;
}

AIRewriter::Stats AIRewriter::GetStats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}

void AIRewriter::EnsureInitialized() const {
  // Fast path: already initialized
  if (initialized_.load()) {
    return;
  }

  // Check failure count
  if (init_failures_.load() >= kMaxInitFailures) {
    return;  // Too many failures, give up
  }

  try {
    auto config = ai::AIConfigManager::Instance().GetConfig();

    // Create cache
    cache_ = std::make_unique<ai::AICandidateCache>(
        config.cache.max_entries,
        config.cache.ttl_seconds);

    // Create backend
    auto backend = ai::CreateBackend(config);
    if (!backend) {
      ai::AILogger::Warn("Failed to create AI backend");
      ++init_failures_;
      return;
    }

    // Create and start worker
    worker_ = std::make_unique<ai::AIWorker>(
        cache_.get(), std::move(backend));
    worker_->Start();

    initialized_.store(true);
    ai::AILogger::Info("AIRewriter initialized successfully");

  } catch (const std::exception& e) {
    ai::AILogger::Error(std::string("AIRewriter init exception: ") + e.what());
    ++init_failures_;
  } catch (...) {
    ai::AILogger::Error("AIRewriter init unknown exception");
    ++init_failures_;
  }
}

std::string AIRewriter::GetCacheKey(const std::string& input) const {
  auto config = ai::AIConfigManager::Instance().GetConfig();

  // Simple key by default
  if (!config.cache.include_context_in_key) {
    return input;
  }

  // Include context in key
  std::string key = input;

  std::lock_guard<std::mutex> lock(context_mutex_);
  if (!context_history_.empty()) {
    // Add last context item to key
    key += "|" + context_history_.back();
  }

  return key;
}

std::string AIRewriter::GetInputKey(const Segments& segments) const {
  if (segments.conversion_segments_size() == 0) {
    return "";
  }
  return std::string(segments.conversion_segment(0).key());
}

std::vector<std::string> AIRewriter::GetExistingCandidates(
    const Segments& segments, int max_count) const {

  std::vector<std::string> result;

  if (segments.conversion_segments_size() == 0) {
    return result;
  }

  const auto& segment = segments.conversion_segment(0);
  int count = std::min(static_cast<int>(segment.candidates_size()),
                       max_count);

  for (int i = 0; i < count; ++i) {
    result.push_back(segment.candidate(i).value);
  }

  return result;
}

void AIRewriter::RequestAICandidates(
    const std::string& key,
    const Segments& segments) const {

  if (!worker_) {
    return;
  }

  // Build request
  ai::AIRequest request;
  request.input_key = key;
  request.existing = GetExistingCandidates(segments, 5);

  // Add context history
  {
    std::lock_guard<std::mutex> lock(context_mutex_);
    for (const auto& h : context_history_) {
      request.context_history.push_back(h);
    }
  }

  // Enqueue request (NON-BLOCKING - returns immediately)
  worker_->EnqueueRequest(std::move(request));
}

void AIRewriter::InsertCandidates(
    const std::vector<std::string>& candidates,
    Segments* segments) const {

  if (candidates.empty() || segments->conversion_segments_size() == 0) {
    return;
  }

  Segment* segment = segments->mutable_conversion_segment(0);
  int added = 0;

  for (const auto& value : candidates) {
    // Skip duplicates
    if (IsDuplicate(value, *segments)) {
      continue;
    }

    // Validate candidate
    if (value.empty() || value.size() > 60) {
      continue;
    }

    // Add candidate
    converter::Candidate* c = segment->push_back_candidate();
    c->value = value;
    c->content_value = value;
    c->description = kCandidateDescription;
    c->category = converter::Candidate::OTHER;

    ++added;

    // Limit candidates added
    if (added >= kMaxCandidatesToAdd) {
      break;
    }
  }

  // Update statistics
  if (added > 0) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.candidates_added += added;
  }
}

bool AIRewriter::IsDuplicate(
    const std::string& candidate,
    const Segments& segments) const {

  if (segments.conversion_segments_size() == 0) {
    return false;
  }

  const auto& segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == candidate) {
      return true;
    }
  }

  return false;
}

void AIRewriter::UpdateContext(const Segments& segments) const {
  if (segments.conversion_segments_size() == 0) {
    return;
  }

  const auto& segment = segments.conversion_segment(0);
  if (segment.candidates_size() == 0) {
    return;
  }

  // Add top candidate to context history
  std::string top_candidate = segment.candidate(0).value;

  std::lock_guard<std::mutex> lock(context_mutex_);
  context_history_.push_back(top_candidate);

  // Limit history size
  while (context_history_.size() > kMaxContextHistory) {
    context_history_.pop_front();
  }
}

}  // namespace mozc
