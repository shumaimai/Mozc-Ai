// Copyright 2024 AI Mozc IME Project
// AI Worker Header - Asynchronous AI Processing

#ifndef MOZC_AI_AI_WORKER_H_
#define MOZC_AI_AI_WORKER_H_

#include <thread>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>
#include <vector>

#include "ai_candidate_cache.h"
#include "ai_backend.h"

namespace mozc {
namespace ai {

// AI processing request
struct AIRequest {
  std::string input_key;                     // Hiragana input
  std::vector<std::string> existing;         // Existing Mozc candidates
  std::vector<std::string> context_history;  // Recent conversion history
};

// AI Worker class for asynchronous processing
// Key design principles:
// 1. EnqueueRequest() MUST be non-blocking and return immediately
// 2. Worker thread runs at low priority to not impact system
// 3. Results are stored in cache for next conversion
class AIWorker {
 public:
  // Constructor
  // @param cache Pointer to shared cache (not owned)
  // @param backend Unique pointer to AI backend (takes ownership)
  AIWorker(AICandidateCache* cache,
           std::unique_ptr<AIBackendInterface> backend);

  ~AIWorker();

  // Start worker thread
  void Start();

  // Stop worker thread gracefully
  void Stop();

  // Enqueue request for processing (NON-BLOCKING - returns immediately)
  // This is the critical method for freeze prevention
  void EnqueueRequest(AIRequest request);

  // Check if backend is ready to process requests
  bool IsBackendReady() const;

  // Get pending request count
  size_t GetPendingCount() const;

  // Check if worker is running
  bool IsRunning() const;

  // Get worker statistics
  struct Stats {
    int64_t requests_processed = 0;
    int64_t requests_succeeded = 0;
    int64_t requests_failed = 0;
    int64_t requests_dropped = 0;  // Dropped due to queue overflow
    int avg_latency_ms = 0;
  };
  Stats GetStats() const;

 private:
  // Worker thread main loop
  void WorkerLoop();

  // Warmup backend (low priority)
  void WarmUp();

  // Process single request
  void ProcessRequest(const AIRequest& request);

  // Set thread to low priority
  void SetLowPriority();

  // Cache reference (not owned)
  AICandidateCache* cache_;

  // AI backend (owned)
  std::unique_ptr<AIBackendInterface> backend_;

  // Worker thread
  std::thread worker_thread_;

  // Running state
  std::atomic<bool> running_{false};

  // Backend ready state
  std::atomic<bool> backend_ready_{false};

  // Request queue
  mutable std::mutex queue_mutex_;
  std::queue<AIRequest> request_queue_;
  std::condition_variable queue_cv_;

  // Statistics
  mutable std::mutex stats_mutex_;
  Stats stats_;
  int64_t total_latency_ms_ = 0;

  // Maximum queue size (overflow protection)
  static constexpr size_t kMaxQueueSize = 10;
};

}  // namespace ai
}  // namespace mozc

#endif  // MOZC_AI_AI_WORKER_H_
