// Copyright 2024 AI Mozc IME Project
// AI Worker Implementation - Asynchronous AI Processing

#include "ai_worker.h"
#include "ai_config.h"
#include "ai_logger.h"

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
    return;
  }

  worker_thread_ = std::thread([this]() {
    SetLowPriority();
    WarmUp();
    WorkerLoop();
  });
}

void AIWorker::Stop() {
  if (!running_.exchange(false)) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_cv_.notify_all();
  }

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

void AIWorker::EnqueueRequest(AIRequest request) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  while (request_queue_.size() >= kMaxQueueSize) {
    request_queue_.pop();
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    ++stats_.requests_dropped;
  }

  request_queue_.push(std::move(request));
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

  if (stats_.requests_processed > 0) {
    result.avg_latency_ms = static_cast<int>(
        total_latency_ms_ / stats_.requests_processed);
  }

  return result;
}

void AIWorker::WorkerLoop() {
  while (running_.load()) {
    AIRequest request;

    {
      std::unique_lock<std::mutex> lock(queue_mutex_);

      queue_cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
        return !request_queue_.empty() || !running_.load();
      });

      if (!running_.load()) {
        break;
      }

      if (request_queue_.empty()) {
        continue;
      }

      request = std::move(request_queue_.front());
      request_queue_.pop();
    }

    if (!backend_ready_.load()) {
      continue;
    }

    ProcessRequest(request);
  }
}

void AIWorker::WarmUp() {
  if (!backend_) {
    AILogger::Error("AI worker warmup skipped: no backend");
    return;
  }

  if (!backend_->Initialize()) {
    AILogger::Error("AI backend Initialize failed: " + backend_->GetConfigInfo());
    return;
  }

  backend_ready_.store(true);
  AILogger::Info("AI backend ready: " + backend_->GetConfigInfo());

  auto config = AIConfigManager::Instance().GetConfig();
  std::vector<std::string> dummy_existing = {"テスト"};
  std::vector<std::string> dummy_context;

  auto result = backend_->Generate(
      "test",
      dummy_existing,
      dummy_context,
      config.timeout.warmup_timeout_ms);

  if (result.success) {
    AILogger::Info("AI backend warmup succeeded");
  } else {
    AILogger::Warn("AI backend warmup failed (non-fatal): " +
                   result.error_message);
  }
}

void AIWorker::ProcessRequest(const AIRequest& request) {
  if (!backend_ || !cache_) {
    return;
  }

  auto config = AIConfigManager::Instance().GetConfig();
  int timeout_ms = config.timeout.request_timeout_ms;

  auto result = backend_->Generate(
      request.input_key,
      request.existing,
      request.context_history,
      timeout_ms);

  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ++stats_.requests_processed;

    if (result.success) {
      ++stats_.requests_succeeded;
    } else {
      ++stats_.requests_failed;
      AILogger::Warn("AI request failed for '" + request.input_key +
                     "': " + result.error_message);
    }

    total_latency_ms_ += result.elapsed_ms;
  }

  if (result.success && !result.candidates.empty()) {
    const std::string& key = request.cache_key.empty()
                                 ? request.input_key
                                 : request.cache_key;
    const size_t count = result.candidates.size();
    cache_->Put(key, std::move(result.candidates));
    AILogger::Info("AI cached " + std::to_string(count) +
                   " candidates for '" + key + "'");
  }
}

void AIWorker::SetLowPriority() {
#ifdef _WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#else
  pthread_t this_thread = pthread_self();
  struct sched_param param;
  param.sched_priority = 0;

#ifdef SCHED_BATCH
  pthread_setschedparam(this_thread, SCHED_BATCH, &param);
#else
  pthread_setschedparam(this_thread, SCHED_OTHER, &param);
#endif
#endif
}

}  // namespace ai
}  // namespace mozc
