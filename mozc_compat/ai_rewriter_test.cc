// Copyright 2024 AI Mozc IME Project
// Unit tests for AI Rewriter (Mozc integration version)

#include "rewriter/ai_rewriter.h"

#include <string>

#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

void AddCandidate(const absl::string_view key, const absl::string_view value,
                  Segment* segment) {
  converter::Candidate* candidate = segment->add_candidate();
  candidate->value = std::string(value);
  candidate->content_value = std::string(value);
  candidate->content_key = std::string(key);
}

TEST(AIRewriterTest, RewriteReturnsTrue) {
  AIRewriter rewriter;
  ConversionRequest request;
  Segments segments;

  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
}

TEST(AIRewriterTest, PreservesExistingCandidates) {
  AIRewriter rewriter;
  Segments segments;
  Segment* segment = segments.push_back_segment();
  segment->set_key("today");
  AddCandidate("today", "Today", segment);

  ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));

  ASSERT_GE(segment->candidates_size(), 1u);
  EXPECT_EQ(segment->candidate(0).value, "Today");
}

TEST(AIRewriterTest, Capability) {
  AIRewriter rewriter;
  ConversionRequest request;

  EXPECT_EQ(rewriter.capability(request), RewriterInterface::CONVERSION);
}

TEST(AIRewriterTest, NullSegments) {
  AIRewriter rewriter;
  ConversionRequest request;

  EXPECT_TRUE(rewriter.Rewrite(request, nullptr));
}

}  // namespace
}  // namespace mozc
