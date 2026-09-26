// Copyright 2026 AI Mozc IME Project
// Anonymous, privacy-safe runtime diagnostics for the AI reranker.
//
// Hard privacy rule: this module records only fixed stage names, fixed reason
// codes, byte/count metadata, latencies, and process-local identifiers.  It
// has no API that accepts user-provided text (readings, candidates, context),
// so input strings cannot leak into diagnostics by construction.

#ifndef MOZC_REWRITER_RERANK_DIAG_H_
#define MOZC_REWRITER_RERANK_DIAG_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace mozc {
namespace rerank {

// Process-local anonymous session id (random 64-bit hex, generated once).
// Correlates JSONL lines and summary lines from one mozc_server process
// without identifying the user.
std::string DiagSessionId();

// Process-local monotonic request id (1, 2, 3, ...).  Together with the
// session id it proves a full C++ -> daemon -> C++ round trip for one
// conversion without carrying any user content.
std::uint64_t NextDiagReqId();

// Resolved guard mode for diagnostics: MOZC_RERANK_GUARD_MODE env value >
// policy override (margin_policy.json "guard_mode") > built-in "safety".
// Returns one of the fixed strings "safety" / "strict" / other fixed mode
// words; never user text.
std::string DiagGuardMode();

// Model identity for diagnostics.  Precedence: daemon-reported sha256 (from
// the daemon's own response, set via SetReportedModelSha256) >
// MOZC_RERANK_DIAG_MODEL_SHA256 (test/smoke harnesses that hash the loaded
// file) > MOZC_RERANK_MODEL_SHA256 (set by the launcher) > "unknown".
// Sanitized to 64 hex characters or "unknown"; never computed from user data.
std::string DiagModelSha256();

// Records the daemon-reported sha256 of the actually loaded model so
// subsequent diag lines report what is really running.  Non-hex input is
// ignored.
void SetReportedModelSha256(std::string_view sha256);

// Emits one stage="summary" line immediately (cumulative counters and
// p50/p95/p99) when MOZC_RERANK_DIAG_LOG is set.  Used by smoke harnesses to
// flush a stable summary line before reading the log.  No-op otherwise.
void FlushDiagSummary();

// One diagnostics event.  Every string field is a compile-time constant or a
// fixed sanitized token from the helpers above.
struct DiagEvent {
  const char* stage = "";       // "rewrite" | "guard_skip" | "summary"
  std::uint64_t req_id = 0;
  // Byte/count metadata only (never content).
  std::uint32_t request_context_bytes = 0;
  std::uint32_t history_bytes = 0;
  std::uint32_t clean_context_bytes = 0;
  std::uint32_t reading_bytes = 0;
  std::uint32_t candidate_count = 0;
  std::uint32_t history_segment_count = 0;
  std::uint32_t conversion_segment_count = 0;
  // Fixed tokens: daemon result "ok" | "fail" | "timeout" | "skip" | "";
  // guard skip reason "reading_too_short" | "context_empty_or_symbol" |
  // "reading_not_eligible" | "".
  const char* daemon_result = "";
  const char* reason = "";
  bool overwrite = false;
  double cpp_ms = 0.0;    // C++ round-trip wall time for this conversion.
  double infer_ms = 0.0;  // daemon-reported inference time (daemon_ms).
  // True when the daemon echoed this event's anonymous req_id back, proving
  // the full C++ -> daemon -> C++ loop for one conversion.
  bool round_trip = false;
};

// Appends one JSONL diagnostics line to MOZC_RERANK_DIAG_LOG (if set),
// updates the in-process counters, and periodically (every
// MOZC_RERANK_DIAG_SUMMARY_EVERY events, default 200) appends a
// stage="summary" line with cumulative counters and p50/p95/p99 for the C++
// round trip and daemon inference times.  Never throws, never blocks on user
// input, records no text.
void AppendDiagEvent(const DiagEvent& event);

struct DiagPercentiles {
  double p50 = 0.0;
  double p95 = 0.0;
  double p99 = 0.0;
};

// Current cumulative counters snapshot (values only; no strings).
struct DiagCounters {
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
  DiagPercentiles cpp_ms;
  DiagPercentiles infer_ms;
};

DiagCounters DiagCountersSnapshot();

}  // namespace rerank
}  // namespace mozc

#endif  // MOZC_REWRITER_RERANK_DIAG_H_
