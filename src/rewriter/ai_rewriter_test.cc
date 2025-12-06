// Copyright 2024 AI Mozc IME Project
// Unit tests for AI Rewriter
// NOTE: These tests use ASCII keys to avoid isctype warnings on Windows

#include "ai_rewriter.h"
#include "rewriter_interface.h"

#include <thread>
#include <chrono>

#include "gtest/gtest.h"

namespace mozc {
namespace {

// Test fixture to set up mock mode for faster testing
class AIRewriterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Tests run without AI backend connection (uses cache only)
  }
};

// Basic creation test
TEST_F(AIRewriterTest, Creation) {
  AIRewriter rewriter;

  // Should not crash on creation
  EXPECT_EQ(rewriter.GetName(), "AIRewriter");
}

// Rewrite returns true test
TEST_F(AIRewriterTest, RewriteReturnsTrue) {
  AIRewriter rewriter;

  ConversionRequest request;
  Segments segments;

  // Empty segments should still return true
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

// Preserves existing candidates test
TEST_F(AIRewriterTest, PreservesExistingCandidates) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("today");

  Candidate* c = segment->add_candidate();
  c->value = "Today";
  c->content_value = "Today";

  ConversionRequest request;
  request.request_type = ConversionRequest::CONVERSION;

  EXPECT_TRUE(rewriter.Rewrite(request, &segments));

  // Existing candidates should be preserved
  EXPECT_GE(segment->candidates_size(), 1);
  EXPECT_EQ(segment->candidate(0).value, "Today");
}

// Capability test
TEST_F(AIRewriterTest, Capability) {
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
TEST_F(AIRewriterTest, NullSegments) {
  AIRewriter rewriter;

  ConversionRequest request;

  // Should not crash with null segments
  EXPECT_TRUE(rewriter.Rewrite(request, nullptr));
}

// Empty key test
TEST_F(AIRewriterTest, EmptyKey) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("");

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

// Clear test
TEST_F(AIRewriterTest, Clear) {
  AIRewriter rewriter;

  // Add some context first
  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("hello");
  Candidate* c = segment->add_candidate();
  c->value = "Hello";

  ConversionRequest request;
  rewriter.Rewrite(request, &segments);

  // Clear should not crash
  rewriter.Clear();
}

// Non-blocking rewrite test
TEST_F(AIRewriterTest, NonBlockingRewrite) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("test");

  ConversionRequest request;

  auto start = std::chrono::steady_clock::now();

  // Rewrite multiple times
  for (int i = 0; i < 5; ++i) {
    rewriter.Rewrite(request, &segments);
  }

  auto elapsed = std::chrono::steady_clock::now() - start;
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  // Should complete quickly (< 30000ms for 5 iterations)
  // Allow more time for initialization overhead
  EXPECT_LT(elapsed_ms, 30000);
}

// Statistics test
TEST_F(AIRewriterTest, Statistics) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("test");

  ConversionRequest request;

  // Make some rewrite calls
  for (int i = 0; i < 3; ++i) {
    rewriter.Rewrite(request, &segments);
  }

  auto stats = rewriter.GetStats();
  EXPECT_EQ(stats.rewrite_calls, 3);
}

// Multiple segments test
TEST_F(AIRewriterTest, MultipleSegments) {
  AIRewriter rewriter;

  Segments segments;

  Segment* seg1 = segments.add_segment();
  seg1->set_key("hello");
  Candidate* c1 = seg1->add_candidate();
  c1->value = "Hello";

  Segment* seg2 = segments.add_segment();
  seg2->set_key("world");
  Candidate* c2 = seg2->add_candidate();
  c2->value = "World";

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));

  // First segment should be processed (candidate preserved)
  EXPECT_GE(seg1->candidates_size(), 1);
}

// IsEnabled test
TEST_F(AIRewriterTest, IsEnabled) {
  AIRewriter rewriter;

  // Default should be enabled
  EXPECT_TRUE(rewriter.IsEnabled());
}

// Thread safety test (reduced iterations for speed)
TEST_F(AIRewriterTest, ThreadSafety) {
  AIRewriter rewriter;

  std::vector<std::thread> threads;

  // Use minimal iterations to avoid timeout
  for (int i = 0; i < 2; ++i) {
    threads.emplace_back([&rewriter]() {
      for (int j = 0; j < 3; ++j) {
        Segments segments;
        Segment* segment = segments.add_segment();
        segment->set_key("test");

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
  EXPECT_EQ(stats.rewrite_calls, 6);
}

// Long input test (ASCII version)
TEST_F(AIRewriterTest, LongInput) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("this_is_a_very_long_input_string_to_test_handling_of_long_keys");

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

// Special characters test (ASCII version)
TEST_F(AIRewriterTest, SpecialCharacters) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("test\n\t\r");

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

// Candidate duplicate check test
TEST_F(AIRewriterTest, NoDuplicates) {
  AIRewriter rewriter;

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("hello");

  // Add existing candidate
  Candidate* c = segment->add_candidate();
  c->value = "Hello";

  ConversionRequest request;
  rewriter.Rewrite(request, &segments);

  // Count occurrences of "Hello"
  int count = 0;
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    if (segment->candidate(i).value == "Hello") {
      ++count;
    }
  }

  // Should only have one "Hello"
  EXPECT_EQ(count, 1);
}

}  // namespace
}  // namespace mozc
