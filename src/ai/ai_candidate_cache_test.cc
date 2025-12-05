// Copyright 2024 AI Mozc IME Project
// Unit tests for AI Candidate Cache

#include "ai/ai_candidate_cache.h"

#include <thread>
#include <vector>
#include <chrono>

#include "gtest/gtest.h"

namespace mozc {
namespace ai {
namespace {

// Basic Put/Get test
TEST(AICandidateCacheTest, BasicPutGet) {
  AICandidateCache cache(10, 60);

  // Empty cache should return nullopt
  EXPECT_FALSE(cache.Get("key1").has_value());

  // Put and Get
  cache.Put("key1", {"候補1", "候補2"});
  auto result = cache.Get("key1");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2);
  EXPECT_EQ((*result)[0], "候補1");
  EXPECT_EQ((*result)[1], "候補2");
}

// Multiple keys test
TEST(AICandidateCacheTest, MultipleKeys) {
  AICandidateCache cache(10, 60);

  cache.Put("きょう", {"今日", "教"});
  cache.Put("あした", {"明日", "足下"});

  auto result1 = cache.Get("きょう");
  auto result2 = cache.Get("あした");

  ASSERT_TRUE(result1.has_value());
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ((*result1)[0], "今日");
  EXPECT_EQ((*result2)[0], "明日");
}

// Expiration test
TEST(AICandidateCacheTest, Expiration) {
  // Very short TTL for testing
  AICandidateCache cache(10, 1);  // 1 second TTL

  cache.Put("key1", {"候補"});
  EXPECT_TRUE(cache.Get("key1").has_value());

  // Wait for expiration
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  EXPECT_FALSE(cache.Get("key1").has_value());
}

// Max entries test
TEST(AICandidateCacheTest, MaxEntries) {
  AICandidateCache cache(3, 60);

  cache.Put("key1", {"1"});
  cache.Put("key2", {"2"});
  cache.Put("key3", {"3"});

  EXPECT_EQ(cache.Size(), 3);

  // Adding 4th should evict oldest
  cache.Put("key4", {"4"});

  EXPECT_EQ(cache.Size(), 3);
  EXPECT_FALSE(cache.Get("key1").has_value());  // Evicted
  EXPECT_TRUE(cache.Get("key4").has_value());   // New entry
}

// Statistics test
TEST(AICandidateCacheTest, Statistics) {
  AICandidateCache cache(10, 60);

  cache.Put("key1", {"value"});

  cache.Get("key1");  // Hit
  cache.Get("key1");  // Hit
  cache.Get("key2");  // Miss
  cache.Get("key3");  // Miss

  auto stats = cache.GetStats();

  EXPECT_EQ(stats.hits, 2);
  EXPECT_EQ(stats.misses, 2);
  EXPECT_EQ(stats.entries, 1);
  EXPECT_DOUBLE_EQ(stats.hit_rate, 0.5);
}

// Clear test
TEST(AICandidateCacheTest, Clear) {
  AICandidateCache cache(10, 60);

  cache.Put("key1", {"1"});
  cache.Put("key2", {"2"});
  EXPECT_EQ(cache.Size(), 2);

  cache.Clear();
  EXPECT_EQ(cache.Size(), 0);
  EXPECT_FALSE(cache.Get("key1").has_value());
}

// Contains test
TEST(AICandidateCacheTest, Contains) {
  AICandidateCache cache(10, 60);

  EXPECT_FALSE(cache.Contains("key1"));

  cache.Put("key1", {"value"});
  EXPECT_TRUE(cache.Contains("key1"));
  EXPECT_FALSE(cache.Contains("key2"));
}

// Overwrite test
TEST(AICandidateCacheTest, Overwrite) {
  AICandidateCache cache(10, 60);

  cache.Put("key1", {"old"});
  cache.Put("key1", {"new"});

  auto result = cache.Get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ((*result)[0], "new");
}

// Thread safety test
TEST(AICandidateCacheTest, ThreadSafety) {
  AICandidateCache cache(100, 60);

  // Multiple threads writing
  std::vector<std::thread> writers;
  for (int i = 0; i < 10; ++i) {
    writers.emplace_back([&cache, i]() {
      for (int j = 0; j < 100; ++j) {
        std::string key = "key" + std::to_string(i * 100 + j);
        cache.Put(key, {"value"});
      }
    });
  }

  // Multiple threads reading
  std::vector<std::thread> readers;
  for (int i = 0; i < 10; ++i) {
    readers.emplace_back([&cache, i]() {
      for (int j = 0; j < 100; ++j) {
        std::string key = "key" + std::to_string(i * 100 + j);
        cache.Get(key);  // May or may not find
      }
    });
  }

  for (auto& t : writers) {
    t.join();
  }
  for (auto& t : readers) {
    t.join();
  }

  // Should not crash and cache should be in valid state
  EXPECT_LE(cache.Size(), 100);
}

// Prune test
TEST(AICandidateCacheTest, Prune) {
  AICandidateCache cache(100, 1);  // 1 second TTL

  cache.Put("key1", {"1"});
  cache.Put("key2", {"2"});

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  cache.Put("key3", {"3"});  // This is fresh

  cache.Prune();

  EXPECT_FALSE(cache.Contains("key1"));
  EXPECT_FALSE(cache.Contains("key2"));
  EXPECT_TRUE(cache.Contains("key3"));
}

// Empty candidates test
TEST(AICandidateCacheTest, EmptyCandidates) {
  AICandidateCache cache(10, 60);

  cache.Put("key1", {});

  auto result = cache.Get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

}  // namespace
}  // namespace ai
}  // namespace mozc
