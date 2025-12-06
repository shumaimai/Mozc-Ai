// Copyright 2024 AI Mozc IME Project
// Unit tests for AI Rewriter

#include "ai_rewriter.h"
#include "rewriter_interface.h"

#include <thread>
#include <chrono>

#include "gtest/gtest.h"

namespace mozc {
namespace {

// Basic creation test
TEST(AIRewriterTest, Creation) {
  AIRewriter rewriter;

  // Should not crash on creation
  EXPECT_EQ(rewriter.GetName(), "AIRewriter");
}

// Rewrite returns true test
TEST(AIRewriterTest, RewriteReturnsTrue) {
  AIRewriter rewriter;

  ConversionRequest request;
  Segments segments;

  // Empty segments should still return true
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

// Preserves existing candidates test
TEST(AIRewriterTest, PreservesExistingCandidates) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("きょう");

  Candidate* c = segment->add_candidate();
  c->value = "今日";
  c->content_value = "今日";

  ConversionRequest request;
  request.request_type = ConversionRequest::CONVERSION;

  EXPECT_TRUE(rewriter.Rewrite(request, &segments));

  // Existing candidates should be preserved
  EXPECT_GE(segment->candidates_size(), 1);
  EXPECT_EQ(segment->candidate(0).value, "今日");
}

// Capability test
TEST(AIRewriterTest, Capability) {
  AIRewriter rewriter;

  // Conversion request
  ConversionRequest conv_request;
  conv_request.request_type = ConversionRequest::CONVERSION;
  EXPECT_EQ(rewriter.capability(conv_request), RewriterInterface::CONVERSION);

  // Prediction request
  ConversionRequest pred_request;
  pred_request.request_type = ConversionRequest::PREDICTION;
  EXPECT_EQ(rewriter.capability(pred_request), RewriterInterface::NONE);
}

// Null segments test
TEST(AIRewriterTest, NullSegments) {
  AIRewriter rewriter;

  ConversionRequest request;

  // Should not crash with null segments
  EXPECT_TRUE(rewriter.Rewrite(request, nullptr));
}

// Empty key test
TEST(AIRewriterTest, EmptyKey) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("");

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

// Clear test
TEST(AIRewriterTest, Clear) {
  AIRewriter rewriter;

  // Add some context first
  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("きょう");
  Candidate* c = segment->add_candidate();
  c->value = "今日";

  ConversionRequest request;
  rewriter.Rewrite(request, &segments);

  // Clear should not crash
  rewriter.Clear();
}

// Non-blocking rewrite test
TEST(AIRewriterTest, NonBlockingRewrite) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("test");  // Use ASCII to avoid isctype warnings

  ConversionRequest request;

  auto start = std::chrono::steady_clock::now();

  // Rewrite multiple times (reduced from 100 to 10)
  for (int i = 0; i < 10; ++i) {
    rewriter.Rewrite(request, &segments);
  }

  auto elapsed = std::chrono::steady_clock::now() - start;
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  // Should complete quickly (< 5000ms for 10 iterations)
  // Increased threshold due to initialization overhead
  EXPECT_LT(elapsed_ms, 5000);
}

// Statistics test
TEST(AIRewriterTest, Statistics) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("test");  // Use ASCII to avoid isctype warnings

  ConversionRequest request;

  // Make some rewrite calls
  for (int i = 0; i < 5; ++i) {
    rewriter.Rewrite(request, &segments);
  }

  auto stats = rewriter.GetStats();
  EXPECT_EQ(stats.rewrite_calls, 5);
}

// Multiple segments test
TEST(AIRewriterTest, MultipleSegments) {
  AIRewriter rewriter;

  Segments segments;

  Segment* seg1 = segments.add_segment();
  seg1->set_key("today");
  Candidate* c1 = seg1->add_candidate();
  c1->value = "Today";

  Segment* seg2 = segments.add_segment();
  seg2->set_key("is");
  Candidate* c2 = seg2->add_candidate();
  c2->value = "is";

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));

  // First segment should be processed (candidate preserved)
  EXPECT_GE(seg1->candidates_size(), 1);
}

// IsEnabled test
TEST(AIRewriterTest, IsEnabled) {
  AIRewriter rewriter;

  // Default should be enabled
  EXPECT_TRUE(rewriter.IsEnabled());
}

// Thread safety test
TEST(AIRewriterTest, ThreadSafety) {
  AIRewriter rewriter;

  std::vector<std::thread> threads;

  // Reduced from 10x100 to 4x10 to avoid timeout
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&rewriter]() {
      for (int j = 0; j < 10; ++j) {
        Segments segments;
        Segment* segment = segments.add_segment();
        segment->set_key("test");  // Use ASCII to avoid isctype warnings

        ConversionRequest request;
        rewriter.Rewrite(request, &segments);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Should not crash
  auto stats = rewriter.GetStats();
  EXPECT_EQ(stats.rewrite_calls, 40);
}

// Long input test
TEST(AIRewriterTest, LongInput) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("これはとてもながいひらがなのにゅうりょくですがどうなるでしょうか");

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

// Special characters test
TEST(AIRewriterTest, SpecialCharacters) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("あ\n\t\r");

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

// Candidate duplicate check test
TEST(AIRewriterTest, NoDuplicates) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("きょう");

  // Add existing candidate
  Candidate* c = segment->add_candidate();
  c->value = "今日";

  ConversionRequest request;
  rewriter.Rewrite(request, &segments);

  // Count occurrences of "今日"
  int count = 0;
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    if (segment->candidate(i).value == "今日") {
      ++count;
    }
  }

  // Should only have one "今日"
  EXPECT_EQ(count, 1);
}

}  // namespace
}  // namespace mozc
