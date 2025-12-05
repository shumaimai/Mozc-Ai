// Copyright 2024 AI Mozc IME Project
// AI Worker Implementation - Asynchronous AI Processing

#include "ai/ai_worker.h"
#include "ai/ai_config.h"

#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

namespace mozc {
namespace ai {

AIWorker::AIWorker(AICandidateCache* cache,
                   std::unique_ptr<AIBackendInterface> backend)
    : cache_(cache), backend_(std::move(backend)) {}

AIWorker::~AIWorker() {
  Stop();
}

void AIWorker::Start() {
  if (running_.exchange(true)) {
    return;  // Already running
  }

  worker_thread_ = std::thread([this]() {
    // Set thread to low priority immediately
    SetLowPriority();

    // Warmup backend (may take time, but at low priority)
    WarmUp();

    // Main processing loop
    WorkerLoop();
  });
}

void AIWorker::Stop() {
  if (!running_.exchange(false)) {
    return;  // Already stopped
  }

  // Wake up worker thread
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_cv_.notify_all();
  }

  // Wait for thread to finish
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

void AIWorker::EnqueueRequest(AIRequest request) {
  // ╔════════════════════════════════════════════════════════════╗
  // ║ CRITICAL: This method MUST be non-blocking                 ║
  // ║ It should return immediately, never wait for anything      ║
  // ╚════════════════════════════════════════════════════════════╝

  std::lock_guard<std::mutex> lock(queue_mutex_);

  // If queue is full, drop oldest requests (prevent memory growth)
  while (request_queue_.size() >= kMaxQueueSize) {
    request_queue_.pop();

    // Update statistics
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    ++stats_.requests_dropped;
  }

  // Add new request
  request_queue_.push(std::move(request));

  // Notify worker thread
  queue_cv_.notify_one();
}

bool AIWorker::IsBackendReady() const {
  return backend_ready_.load();
}

size_t AIWorker::GetPendingCount() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return request_queue_.size();
}

bool AIWorker::IsRunning() const {
  return running_.load();
}

AIWorker::Stats AIWorker::GetStats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  Stats result = stats_;

  // Calculate average latency
  if (stats_.requests_processed > 0) {
    result.avg_latency_ms = static_cast<int>(
        total_latency_ms_ / stats_.requests_processed);
  }

  return result;
}

void AIWorker::WorkerLoop() {
  auto config = AIConfigManager::Instance().GetConfig();
  int timeout_ms = config.timeout.request_timeout_ms;

  while (running_.load()) {
    AIRequest request;

    // Wait for request
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);

      // Wait with timeout for new requests
      queue_cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
        return !request_queue_.empty() || !running_.load();
      });

      // Check if we should exit
      if (!running_.load()) {
        break;
      }

      // Check if queue is empty (spurious wakeup)
      if (request_queue_.empty()) {
        continue;
      }

      // Get request from queue
      request = std::move(request_queue_.front());
      request_queue_.pop();
    }

    // Skip processing if backend not ready
    if (!backend_ready_.load()) {
      continue;
    }

    // Process request
    ProcessRequest(request);
  }
}

void AIWorker::WarmUp() {
  if (!backend_) {
    return;
  }

  // Initialize backend
  if (!backend_->Initialize()) {
    // Initialization failed - continue without AI
    return;
  }

  auto config = AIConfigManager::Instance().GetConfig();
  int warmup_timeout = config.timeout.warmup_timeout_ms;

  // Perform warmup request to load model into memory
  std::vector<std::string> dummy_existing = {"テスト"};
  std::vector<std::string> dummy_context;

  auto result = backend_->Generate(
      "test",
      dummy_existing,
      dummy_context,
      warmup_timeout);

  if (result.success) {
    backend_ready_.store(true);
  }
}

void AIWorker::ProcessRequest(const AIRequest& request) {
  if (!backend_ || !cache_) {
    return;
  }

  auto config = AIConfigManager::Instance().GetConfig();
  int timeout_ms = config.timeout.request_timeout_ms;

  // Call AI backend
  auto result = backend_->Generate(
      request.input_key,
      request.existing,
      request.context_history,
      timeout_ms);

  // Update statistics
  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ++stats_.requests_processed;

    if (result.success) {
      ++stats_.requests_succeeded;
    } else {
      ++stats_.requests_failed;
    }

    total_latency_ms_ += result.elapsed_ms;
  }

  // Store successful results in cache
  if (result.success && !result.candidates.empty()) {
    cache_->Put(request.input_key, std::move(result.candidates));
  }
}

void AIWorker::SetLowPriority() {
#ifdef _WIN32
  // Windows: Set thread priority to below normal
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#else
  // Linux/macOS: Set nice value
  pthread_t this_thread = pthread_self();

  // Try to set scheduling policy to SCHED_BATCH (Linux)
  struct sched_param param;
  param.sched_priority = 0;

#ifdef SCHED_BATCH
  pthread_setschedparam(this_thread, SCHED_BATCH, &param);
#else
  // Fallback: just use SCHED_OTHER with lowest priority
  pthread_setschedparam(this_thread, SCHED_OTHER, &param);
#endif

  // Also set nice value if possible
  // Note: This may require root privileges
  // nice(10);  // Increase niceness (lower priority)
#endif
}

}  // namespace ai
}  // namespace mozc
