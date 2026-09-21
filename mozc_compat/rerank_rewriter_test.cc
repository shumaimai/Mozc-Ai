// Copyright 2026 AI Mozc IME Project
// Basic RerankRewriter tests for the all-in-one local runtime path.

#include "rewriter/rerank_rewriter.h"
#include "rewriter/rerank_guard.h"

#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace mozc {
namespace {

ConversionRequest MakeConversionRequest() {
  ConversionRequest::Options options = {
      .request_type = ConversionRequest::CONVERSION,
  };
  return ConversionRequestBuilder().SetOptions(std::move(options)).Build();
}

TEST(RerankRewriterTest, EnabledByDefault) {
  // The v1 MSI installs a loopback-only daemon, so an unset kill switch
  // enables the rewriter.  Empty context still fails safe before any I/O.
#ifdef _WIN32
  _putenv_s("MOZC_RERANK_ENABLED", "");
  _putenv_s("MOZC_RERANK_HOOK_CMD", "");
  _putenv_s("MOZC_RERANK_DAEMON_ADDR", "");
#else
  unsetenv("MOZC_RERANK_ENABLED");
  unsetenv("MOZC_RERANK_HOOK_CMD");
  unsetenv("MOZC_RERANK_DAEMON_ADDR");
#endif
  RerankRewriter rewriter;
  EXPECT_TRUE(rewriter.IsEnabled());
  const ConversionRequest req = MakeConversionRequest();
  EXPECT_EQ(rewriter.capability(req), RewriterInterface::CONVERSION);

  Segments segments;
  Segment* seg = segments.add_segment();
  seg->set_key("とうきょう");
  seg->set_segment_type(Segment::FREE);
  converter::Candidate* c0 = seg->add_candidate();
  c0->value = "東京";
  converter::Candidate* c1 = seg->add_candidate();
  c1->value = "東響";

  EXPECT_FALSE(rewriter.Rewrite(req, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "東京");
}

TEST(RerankRewriterTest, EmptyContextKeepsMozcWhenDisabled) {
#ifdef _WIN32
  _putenv_s("MOZC_RERANK_ENABLED", "0");
  _putenv_s("MOZC_RERANK_HOOK_CMD", "");
  _putenv_s("MOZC_RERANK_DAEMON_ADDR", "");
#else
  setenv("MOZC_RERANK_ENABLED", "0", 1);
  unsetenv("MOZC_RERANK_HOOK_CMD");
  unsetenv("MOZC_RERANK_DAEMON_ADDR");
#endif
  RerankRewriter rewriter;
  const ConversionRequest req = MakeConversionRequest();
  Segments segments;
  Segment* hist = segments.add_segment();
  hist->set_segment_type(Segment::HISTORY);
  hist->set_key("きしゃ");
  converter::Candidate* h0 = hist->add_candidate();
  h0->value = "記者";
  Segment* seg = segments.add_segment();
  seg->set_key("が");
  seg->set_segment_type(Segment::FREE);
  converter::Candidate* c0 = seg->add_candidate();
  c0->value = "が";
  EXPECT_FALSE(rewriter.Rewrite(req, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "が");
}

TEST(RerankRewriterTest, TimeoutKeepsMozcOrder) {
#ifdef _WIN32
  _putenv_s("MOZC_RERANK_ENABLED", "1");
  _putenv_s("MOZC_RERANK_TIMEOUT_MS", "80");
  _putenv_s("MOZC_RERANK_HOOK_CMD", "python -c \"import time; time.sleep(5)\"");
#else
  setenv("MOZC_RERANK_ENABLED", "1", 1);
  setenv("MOZC_RERANK_TIMEOUT_MS", "80", 1);
  setenv("MOZC_RERANK_HOOK_CMD", "python3 -c \"import time; time.sleep(5)\"", 1);
#endif
  RerankRewriter rewriter;
  EXPECT_TRUE(rewriter.IsEnabled());
  const ConversionRequest req = MakeConversionRequest();
  Segments segments;
  // Linguistic context so usage-guard does not skip before the hook timeout.
  Segment* hist = segments.add_segment();
  hist->set_segment_type(Segment::HISTORY);
  hist->set_key("えきに");
  converter::Candidate* h0 = hist->add_candidate();
  h0->value = "駅に";
  Segment* seg = segments.add_segment();
  seg->set_key("きしゃ");
  seg->set_segment_type(Segment::FREE);
  converter::Candidate* c0 = seg->add_candidate();
  c0->value = "記者";
  converter::Candidate* c1 = seg->add_candidate();
  c1->value = "汽車";
  EXPECT_FALSE(rewriter.Rewrite(req, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "記者");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).value, "汽車");
}

TEST(RerankRewriterTest, KishyaContextHistoryIsClippedNotScoredWhenDisabled) {
  // History is present (記者) but rewriter is off → Mozc order for きしゃ.
#ifdef _WIN32
  _putenv_s("MOZC_RERANK_ENABLED", "0");
  _putenv_s("MOZC_RERANK_HOOK_CMD", "");
  _putenv_s("MOZC_RERANK_DAEMON_ADDR", "");
#else
  setenv("MOZC_RERANK_ENABLED", "0", 1);
  unsetenv("MOZC_RERANK_HOOK_CMD");
  unsetenv("MOZC_RERANK_DAEMON_ADDR");
#endif
  RerankRewriter rewriter;
  const ConversionRequest req = MakeConversionRequest();
  Segments segments;
  Segment* hist = segments.add_segment();
  hist->set_segment_type(Segment::HISTORY);
  hist->set_key("しんぶんの");
  converter::Candidate* h0 = hist->add_candidate();
  h0->value = "新聞の";
  Segment* seg = segments.add_segment();
  seg->set_key("きしゃ");
  seg->set_segment_type(Segment::FREE);
  converter::Candidate* c0 = seg->add_candidate();
  c0->value = "汽車";
  converter::Candidate* c1 = seg->add_candidate();
  c1->value = "記者";
  EXPECT_FALSE(rewriter.Rewrite(req, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "汽車");
}

TEST(RerankRewriterTest, DaemonUnreachableKeepsMozcOrder) {
#ifdef _WIN32
  _putenv_s("MOZC_RERANK_ENABLED", "1");
  _putenv_s("MOZC_RERANK_HOOK_CMD", "");
  _putenv_s("MOZC_RERANK_DAEMON_ADDR", "127.0.0.1:59999");
  _putenv_s("MOZC_RERANK_TIMEOUT_MS", "80");
#else
  setenv("MOZC_RERANK_ENABLED", "1", 1);
  unsetenv("MOZC_RERANK_HOOK_CMD");
  setenv("MOZC_RERANK_DAEMON_ADDR", "127.0.0.1:59999", 1);
  setenv("MOZC_RERANK_TIMEOUT_MS", "80", 1);
#endif
  RerankRewriter rewriter;
  EXPECT_TRUE(rewriter.IsEnabled());
  const ConversionRequest req = MakeConversionRequest();
  Segments segments;
  Segment* hist = segments.add_segment();
  hist->set_segment_type(Segment::HISTORY);
  hist->set_key("えきに");
  converter::Candidate* h0 = hist->add_candidate();
  h0->value = "駅に";
  Segment* seg = segments.add_segment();
  seg->set_key("きしゃ");
  seg->set_segment_type(Segment::FREE);
  converter::Candidate* c0 = seg->add_candidate();
  c0->value = "記者";
  converter::Candidate* c1 = seg->add_candidate();
  c1->value = "汽車";
  EXPECT_FALSE(rewriter.Rewrite(req, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "記者");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).value, "汽車");
}

TEST(RerankRewriterTest, GuardSkipsShortReadingWithoutHook) {
#ifdef _WIN32
  _putenv_s("MOZC_RERANK_ENABLED", "1");
  _putenv_s("MOZC_RERANK_TIMEOUT_MS", "80");
  _putenv_s("MOZC_RERANK_HOOK_CMD", "python -c \"import time; time.sleep(5)\"");
#else
  setenv("MOZC_RERANK_ENABLED", "1", 1);
  setenv("MOZC_RERANK_TIMEOUT_MS", "80", 1);
  setenv("MOZC_RERANK_HOOK_CMD", "python3 -c \"import time; time.sleep(5)\"", 1);
#endif
  RerankRewriter rewriter;
  const ConversionRequest req = MakeConversionRequest();
  Segments segments;
  Segment* hist = segments.add_segment();
  hist->set_segment_type(Segment::HISTORY);
  hist->set_key("に");
  converter::Candidate* h0 = hist->add_candidate();
  h0->value = "2";
  Segment* seg = segments.add_segment();
  seg->set_key("い");
  seg->set_segment_type(Segment::FREE);
  converter::Candidate* c0 = seg->add_candidate();
  c0->value = "位";
  converter::Candidate* c1 = seg->add_candidate();
  c1->value = "李";
  // Guard skip is immediate; a 5s hook would trip the 80ms timeout otherwise.
  EXPECT_FALSE(rewriter.Rewrite(req, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "位");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).value, "李");
}

TEST(RerankRewriterTest, GuardSkipReasons) {
  EXPECT_EQ(rerank::RerankSkipReason("い", "文化"), "reading_too_short");
  EXPECT_EQ(rerank::RerankSkipReason("ねん", "5"), "reading_too_short");
  EXPECT_EQ(rerank::RerankSkipReason("きしゃ", ""), "context_empty_or_symbol");
  EXPECT_EQ(rerank::RerankSkipReason("きしゃ", "1"), "context_empty_or_symbol");
  EXPECT_EQ(rerank::RerankSkipReason("きしゃ", "、"), "context_empty_or_symbol");
  EXPECT_EQ(rerank::RerankSkipReason("いいんちょう", "文化"),
            "reading_not_eligible");
  EXPECT_EQ(rerank::RerankSkipReason("きょうかい", "全国商業高等学校"),
            "reading_not_eligible");
  EXPECT_EQ(rerank::RerankSkipReason("きしゃ", "駅に"), "");
  EXPECT_TRUE(rerank::IsJunkSurface("ヨセン"));
  EXPECT_TRUE(rerank::IsJunkSurface("實際に"));
  EXPECT_FALSE(rerank::IsJunkSurface("予選"));
}

TEST(RerankRewriterTest, SafetyGuardModeRelaxesOnlyReadingAllowlist) {
#ifdef _WIN32
  _putenv_s("MOZC_RERANK_GUARD_MODE", "safety");
#else
  setenv("MOZC_RERANK_GUARD_MODE", "safety", 1);
#endif
  EXPECT_EQ(rerank::RerankSkipReason("いいんちょう", "文化"), "");
  EXPECT_EQ(rerank::RerankSkipReason("い", "文化"), "reading_too_short");
  EXPECT_EQ(rerank::RerankSkipReason("いいんちょう", "1"),
            "context_empty_or_symbol");
#ifdef _WIN32
  _putenv_s("MOZC_RERANK_GUARD_MODE", "");
#else
  unsetenv("MOZC_RERANK_GUARD_MODE");
#endif
}

#ifndef _WIN32
TEST(RerankRewriterTest, MultiSegmentLogUsesScoredTargetSegment) {
  const char *log_path = "/tmp/mozc_phase0_segment_log.jsonl";
  const char *hook_path = "/tmp/mozc_phase0_segment_hook.sh";
  std::remove(log_path);
  {
    std::ofstream hook(hook_path);
    hook << "#!/bin/sh\n"
         << "printf '%s' '{\"ranked_surfaces\":[\"汽車\",\"記者\"],"
            "\"rerank_top1\":\"汽車\",\"final_top1\":\"汽車\","
            "\"overwritten\":true}' > \"$2\"\n";
  }
  chmod(hook_path, 0700);
  setenv("MOZC_RERANK_ENABLED", "1", 1);
  setenv("MOZC_RERANK_LOG", log_path, 1);
  setenv("MOZC_RERANK_HOOK_CMD",
         "sh -c 'printf \\\"{\\\\\\\"ranked_surfaces\\\\\\\":[\\\\\\\"汽車\\\\\\\",\\\\\\\"記者\\\\\\\"],\\\\\\\"rerank_top1\\\\\\\":\\\\\\\"汽車\\\\\\\",\\\\\\\"final_top1\\\\\\\":\\\\\\\"汽車\\\\\\\",\\\\\\\"overwritten\\\\\\\":true}\\\" > \\\"$1\\\"' _",
         1);
  setenv("MOZC_RERANK_HOOK_CMD", hook_path, 1);
  unsetenv("MOZC_RERANK_DAEMON_ADDR");

  RerankRewriter rewriter;
  const ConversionRequest req = MakeConversionRequest();
  Segments segments;
  Segment *first = segments.add_segment();
  first->set_key("えき");
  first->add_candidate()->value = "駅";
  Segment *target = segments.add_segment();
  target->set_key("きしゃ");
  target->add_candidate()->value = "記者";
  target->add_candidate()->value = "汽車";

  EXPECT_TRUE(rewriter.Rewrite(req, &segments));
  EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "汽車");
  rewriter.Finish(req, segments);

  std::ifstream in(log_path);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"target_segment_index\":1"), std::string::npos);
  EXPECT_NE(line.find("\"mozc_top1\":\"記者\""), std::string::npos);
  EXPECT_NE(line.find("\"model_top1\":\"汽車\""), std::string::npos);
  EXPECT_NE(line.find("\"committed_candidate\":\"汽車\""), std::string::npos);
  std::remove(log_path);
  std::remove(hook_path);
  unsetenv("MOZC_RERANK_LOG");
  unsetenv("MOZC_RERANK_HOOK_CMD");
}
#endif

}  // namespace
}  // namespace mozc
