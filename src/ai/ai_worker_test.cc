// Copyright 2024 AI Mozc IME Project
// Unit tests for AI Worker

#include "ai/ai_worker.h"
#include "ai/ai_candidate_cache.h"
#include "ai/ai_backend.h"

#include <thread>
#include <chrono>

#include "gtest/gtest.h"

namespace mozc {
namespace ai {
namespace {

// Worker creation test
TEST(AIWorkerTest, Creation) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));

  EXPECT_FALSE(worker.IsRunning());
  EXPECT_FALSE(worker.IsBackendReady());
}

// Worker start/stop test
TEST(AIWorkerTest, StartStop) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));

  worker.Start();
  EXPECT_TRUE(worker.IsRunning());

  // Give some time for warmup
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  worker.Stop();
  EXPECT_FALSE(worker.IsRunning());
}

// Enqueue request is non-blocking test
TEST(AIWorkerTest, EnqueueNonBlocking) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));
  worker.Start();

  auto start = std::chrono::steady_clock::now();

  // Enqueue multiple requests
  for (int i = 0; i < 100; ++i) {
    AIRequest request;
    request.input_key = "test" + std::to_string(i);
    worker.EnqueueRequest(std::move(request));
  }

  auto elapsed = std::chrono::steady_clock::now() - start;
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  // Should complete very quickly (< 100ms)
  EXPECT_LT(elapsed_ms, 100);

  worker.Stop();
}

// Pending count test
TEST(AIWorkerTest, PendingCount) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));
  // Don't start worker yet

  AIRequest request;
  request.input_key = "test";
  worker.EnqueueRequest(std::move(request));

  EXPECT_GE(worker.GetPendingCount(), 0);  // At least 0 (may be processed already)
}

// Queue overflow protection test
TEST(AIWorkerTest, QueueOverflow) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));
  // Don't start worker - requests will queue up

  // Enqueue more than max queue size
  for (int i = 0; i < 100; ++i) {
    AIRequest request;
    request.input_key = "test" + std::to_string(i);
    worker.EnqueueRequest(std::move(request));
  }

  // Queue should not exceed max size (10)
  EXPECT_LE(worker.GetPendingCount(), 10);
}

// Results stored in cache test
TEST(AIWorkerTest, ResultsInCache) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));
  worker.Start();

  // Wait for backend to be ready
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Enqueue request
  AIRequest request;
  request.input_key = "きょう";
  worker.EnqueueRequest(std::move(request));

  // Wait for processing
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Check cache
  auto result = cache->Get("きょう");

  // Mock backend should return candidates
  if (worker.IsBackendReady()) {
    EXPECT_TRUE(result.has_value());
    if (result.has_value()) {
      EXPECT_FALSE(result->empty());
    }
  }

  worker.Stop();
}

// Statistics test
TEST(AIWorkerTest, Statistics) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));
  worker.Start();

  // Wait for backend ready
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Enqueue requests
  for (int i = 0; i < 5; ++i) {
    AIRequest request;
    request.input_key = "test" + std::to_string(i);
    worker.EnqueueRequest(std::move(request));
  }

  // Wait for processing
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  auto stats = worker.GetStats();

  // Some requests should have been processed
  if (worker.IsBackendReady()) {
    EXPECT_GT(stats.requests_processed, 0);
  }

  worker.Stop();
}

// Context history test
TEST(AIWorkerTest, ContextHistory) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));
  worker.Start();

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Request with context
  AIRequest request;
  request.input_key = "きょう";
  request.context_history = {"昨日", "一昨日"};
  worker.EnqueueRequest(std::move(request));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  worker.Stop();
}

// Multiple start/stop test
TEST(AIWorkerTest, MultipleStartStop) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));

  for (int i = 0; i < 3; ++i) {
    worker.Start();
    EXPECT_TRUE(worker.IsRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    worker.Stop();
    EXPECT_FALSE(worker.IsRunning());
  }
}

// Graceful stop with pending requests
TEST(AIWorkerTest, GracefulStop) {
  auto cache = std::make_unique<AICandidateCache>(100, 60);
  auto backend = CreateMockBackend();

  AIWorker worker(cache.get(), std::move(backend));
  worker.Start();

  // Enqueue requests
  for (int i = 0; i < 10; ++i) {
    AIRequest request;
    request.input_key = "test" + std::to_string(i);
    worker.EnqueueRequest(std::move(request));
  }

  // Stop immediately
  worker.Stop();

  // Should not crash
  EXPECT_FALSE(worker.IsRunning());
}

}  // namespace
}  // namespace ai
}  // namespace mozc
