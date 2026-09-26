// Copyright 2026 AI Mozc IME Project
// Anonymous, privacy-safe runtime diagnostics for the AI reranker.
// See rerank_diag.h for the privacy contract.

#include "rewriter/rerank_diag.h"

#include "rewriter/rerank_guard.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace mozc {
namespace rerank {
namespace {

std::string GetEnvOrEmpty(const char* name) {
  const char* v = std::getenv(name);
  return v == nullptr ? std::string() : std::string(v);
}

bool IsHexChar(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

// Sanitizes a model hash to exactly 64 hex characters, else "unknown".
std::string SanitizeSha256(const std::string& v) {
  if (v.size() != 64) {
    return "unknown";
  }
  for (char c : v) {
    if (!IsHexChar(c)) {
      return "unknown";
    }
  }
  return v;
}

std::mutex g_reported_model_sha_mutex;
std::string g_reported_model_sha;  // sanitized 64-hex when set, else empty

std::string ResolveModelSha256() {
  // Prefer the sha256 reported by the daemon that actually loaded the model;
  // fall back to environment-provided hashes, else "unknown".
  {
    std::lock_guard<std::mutex> lock(g_reported_model_sha_mutex);
    if (!g_reported_model_sha.empty()) {
      return g_reported_model_sha;
    }
  }
  const std::string from_test =
      SanitizeSha256(GetEnvOrEmpty("MOZC_RERANK_DIAG_MODEL_SHA256"));
  if (from_test != "unknown") {
    return from_test;
  }
  const std::string from_env =
      SanitizeSha256(GetEnvOrEmpty("MOZC_RERANK_MODEL_SHA256"));
  if (from_env != "unknown") {
    return from_env;
  }
  return "unknown";
}

// Guard mode mirrors rerank_guard.cc precedence (env > policy > default) and
// reports only fixed tokens.
std::string ResolveGuardMode() {
  std::string mode = GetEnvOrEmpty("MOZC_RERANK_GUARD_MODE");
  if (mode.empty()) {
    mode = PolicyGuardModeOverride();
  }
  if (mode.empty()) {
    mode = "safety";
  }
  for (char& c : mode) {
    if (c >= 'A' && c <= 'Z') {
      c += 'a' - 'A';
    }
  }
  if (mode == "safety" || mode == "strict") {
    return mode;
  }
  return "custom";
}

std::uint64_t SummaryEveryValue() {
  const std::string v = GetEnvOrEmpty("MOZC_RERANK_DIAG_SUMMARY_EVERY");
  if (v.empty()) {
    return 200;
  }
  const unsigned long long parsed = std::strtoull(v.c_str(), nullptr, 10);
  return parsed == 0 ? 200 : static_cast<std::uint64_t>(parsed);
}

struct DiagState {
  // Counters.
  std::uint64_t rewrite_calls = 0;
  std::uint64_t guard_skips = 0;
  std::uint64_t skip_reading_too_short = 0;
  std::uint64_t skip_context_empty_or_symbol = 0;
  std::uint64_t skip_reading_not_eligible = 0;
  std::uint64_t daemon_ok = 0;
  std::uint64_t daemon_fail = 0;
  std::uint64_t daemon_timeout = 0;
  std::uint64_t overwrites = 0;
  std::uint64_t round_trips = 0;
  std::uint64_t events_since_summary = 0;
  // Latency samples (capped; oldest half dropped when full).
  std::vector<double> cpp_ms;
  std::vector<double> infer_ms;
  static constexpr size_t kMaxSamples = 4096;
};

DiagState& State() {
  static DiagState state;
  return state;
}

std::mutex& StateMutex() {
  static std::mutex mutex;
  return mutex;
}

void AddSample(std::vector<double>* samples, double value) {
  constexpr double kCap = 100000.0;
  if (value < 0.0) {
    value = 0.0;
  }
  if (value > kCap) {
    value = kCap;
  }
  if (samples->size() >= DiagState::kMaxSamples) {
    samples->erase(samples->begin(),
                   samples->begin() + static_cast<std::ptrdiff_t>(
                                          samples->size() / 2));
  }
  samples->push_back(value);
}

DiagPercentiles PercentilesOf(const std::vector<double>& samples) {
  DiagPercentiles p;
  if (samples.empty()) {
    return p;
  }
  std::vector<double> sorted(samples);
  std::sort(sorted.begin(), sorted.end());
  auto at = [&sorted](double frac) {
    const size_t n = sorted.size();
    const size_t idx = static_cast<size_t>(frac * static_cast<double>(n - 1));
    return sorted[idx];
  };
  p.p50 = at(0.50);
  p.p95 = at(0.95);
  p.p99 = at(0.99);
  return p;
}

std::string JsonEscapeToken(const char* s) {
  std::string out = "\"";
  for (const char* p = s; *p != '\0'; ++p) {
    if (*p == '"' || *p == '\\') {
      out.push_back('\\');
    }
    out.push_back(*p);
  }
  out.push_back('"');
  return out;
}

void AppendEventLocked(std::ofstream* out, const DiagEvent& e,
                       const std::string& session_id,
                       const std::string& model_sha, const std::string& mode) {
  *out << "{\"stage\":" << JsonEscapeToken(e.stage)
       << ",\"session_id\":\"" << session_id << "\""
       << ",\"req_id\":" << e.req_id << ",\"request_context_bytes\":"
       << e.request_context_bytes << ",\"history_bytes\":" << e.history_bytes
       << ",\"clean_context_bytes\":" << e.clean_context_bytes
       << ",\"reading_bytes\":" << e.reading_bytes
       << ",\"candidate_count\":" << e.candidate_count
       << ",\"history_segment_count\":" << e.history_segment_count
       << ",\"conversion_segment_count\":" << e.conversion_segment_count
       << ",\"daemon_result\":" << JsonEscapeToken(e.daemon_result)
       << ",\"reason\":" << JsonEscapeToken(e.reason)
       << ",\"overwrite\":" << (e.overwrite ? "true" : "false")
       << ",\"round_trip\":" << (e.round_trip ? "true" : "false")
       << ",\"cpp_ms\":" << e.cpp_ms << ",\"infer_ms\":" << e.infer_ms
       << ",\"model_sha256\":\"" << model_sha << "\""
       << ",\"guard_mode\":" << JsonEscapeToken(mode.c_str()) << "}\n";
}

void AppendSummaryLocked(std::ofstream* out, const DiagState& s,
                         const std::string& session_id,
                         const std::string& model_sha,
                         const std::string& mode) {
  const DiagPercentiles cpp = PercentilesOf(s.cpp_ms);
  const DiagPercentiles inf = PercentilesOf(s.infer_ms);
  *out << "{\"stage\":\"summary\""
       << ",\"session_id\":\"" << session_id << "\""
       << ",\"rewrite_calls\":" << s.rewrite_calls
       << ",\"guard_skips\":" << s.guard_skips
       << ",\"skip_reading_too_short\":" << s.skip_reading_too_short
       << ",\"skip_context_empty_or_symbol\":" << s.skip_context_empty_or_symbol
       << ",\"skip_reading_not_eligible\":" << s.skip_reading_not_eligible
       << ",\"daemon_ok\":" << s.daemon_ok
       << ",\"daemon_fail\":" << s.daemon_fail
       << ",\"daemon_timeout\":" << s.daemon_timeout
       << ",\"overwrites\":" << s.overwrites
       << ",\"round_trips\":" << s.round_trips
       << ",\"cpp_ms_p50\":" << cpp.p50
       << ",\"cpp_ms_p95\":" << cpp.p95 << ",\"cpp_ms_p99\":" << cpp.p99
       << ",\"infer_ms_p50\":" << inf.p50 << ",\"infer_ms_p95\":" << inf.p95
       << ",\"infer_ms_p99\":" << inf.p99 << ",\"model_sha256\":\""
       << model_sha << "\",\"guard_mode\":" << JsonEscapeToken(mode.c_str())
       << "}\n";
}

}  // namespace

std::string DiagSessionId() {
  static const std::string session_id = [] {
    std::random_device rd;
    const std::uint64_t value =
        (static_cast<std::uint64_t>(rd()) << 32) |
        static_cast<std::uint64_t>(rd());
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(value));
    return std::string(buf);
  }();
  return session_id;
}

std::uint64_t NextDiagReqId() {
  static std::atomic<std::uint64_t> counter{0};
  return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

std::string DiagGuardMode() { return ResolveGuardMode(); }

std::string DiagModelSha256() { return ResolveModelSha256(); }

void SetReportedModelSha256(std::string_view sha256) {
  const std::string sanitized = SanitizeSha256(std::string(sha256));
  if (sanitized == "unknown") {
    return;
  }
  std::lock_guard<std::mutex> lock(g_reported_model_sha_mutex);
  g_reported_model_sha = sanitized;
}

void FlushDiagSummary() {
  const std::string path = GetEnvOrEmpty("MOZC_RERANK_DIAG_LOG");
  if (path.empty()) {
    return;
  }
  const std::string session_id = DiagSessionId();
  const std::string model_sha = DiagModelSha256();
  const std::string mode = DiagGuardMode();
  std::ofstream out(path, std::ios::app | std::ios::binary);
  if (!out) {
    return;
  }
  std::lock_guard<std::mutex> lock(StateMutex());
  AppendSummaryLocked(&out, State(), session_id, model_sha, mode);
}

DiagCounters DiagCountersSnapshot() {
  std::lock_guard<std::mutex> lock(StateMutex());
  DiagState& s = State();
  DiagCounters c;
  c.rewrite_calls = s.rewrite_calls;
  c.guard_skips = s.guard_skips;
  c.skip_reading_too_short = s.skip_reading_too_short;
  c.skip_context_empty_or_symbol = s.skip_context_empty_or_symbol;
  c.skip_reading_not_eligible = s.skip_reading_not_eligible;
  c.daemon_ok = s.daemon_ok;
  c.daemon_fail = s.daemon_fail;
  c.daemon_timeout = s.daemon_timeout;
  c.overwrites = s.overwrites;
  c.round_trips = s.round_trips;
  c.cpp_ms = PercentilesOf(s.cpp_ms);
  c.infer_ms = PercentilesOf(s.infer_ms);
  return c;
}

void AppendDiagEvent(const DiagEvent& event) {
  const std::string session_id = DiagSessionId();
  const std::string model_sha = DiagModelSha256();
  const std::string mode = DiagGuardMode();
  const bool is_guard_skip = event.stage != nullptr &&
                             std::string(event.stage) == "guard_skip";
  const bool is_rewrite =
      event.stage != nullptr && std::string(event.stage) == "rewrite";
  bool emit_summary = false;
  if (is_guard_skip || is_rewrite) {
    std::lock_guard<std::mutex> lock(StateMutex());
    DiagState& s = State();
    ++s.rewrite_calls;
    ++s.events_since_summary;
    if (is_guard_skip) {
      ++s.guard_skips;
      if (event.reason != nullptr) {
        const std::string reason(event.reason);
        if (reason == "reading_too_short") {
          ++s.skip_reading_too_short;
        } else if (reason == "context_empty_or_symbol") {
          ++s.skip_context_empty_or_symbol;
        } else if (reason == "reading_not_eligible") {
          ++s.skip_reading_not_eligible;
        }
      }
    } else {
      if (event.daemon_result != nullptr) {
        const std::string result(event.daemon_result);
        if (result == "ok") {
          ++s.daemon_ok;
        } else if (result == "fail") {
          ++s.daemon_fail;
        } else if (result == "timeout") {
          ++s.daemon_timeout;
        }
      }
      if (event.overwrite) {
        ++s.overwrites;
      }
      if (event.round_trip) {
        ++s.round_trips;
      }
    }
    if (event.cpp_ms > 0.0) {
      AddSample(&s.cpp_ms, event.cpp_ms);
    }
    if (event.infer_ms > 0.0) {
      AddSample(&s.infer_ms, event.infer_ms);
    }
    const std::uint64_t every = SummaryEveryValue();
    if (s.events_since_summary >= every) {
      s.events_since_summary = 0;
      emit_summary = true;
    }
  }

  const std::string path = GetEnvOrEmpty("MOZC_RERANK_DIAG_LOG");
  if (path.empty()) {
    return;
  }
  std::ofstream out(path, std::ios::app | std::ios::binary);
  if (!out) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(StateMutex());
    AppendEventLocked(&out, event, session_id, model_sha, mode);
    if (emit_summary) {
      AppendSummaryLocked(&out, State(), session_id, model_sha, mode);
    }
  }
}

}  // namespace rerank
}  // namespace mozc
