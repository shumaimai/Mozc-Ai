// Copyright 2024 AI Mozc IME Project
// Rewriter Interface Header
// This mirrors the Mozc rewriter interface for integration

#ifndef MOZC_REWRITER_REWRITER_INTERFACE_H_
#define MOZC_REWRITER_REWRITER_INTERFACE_H_

#include <string>
#include <vector>
#include <memory>

namespace mozc {

// Forward declarations to match Mozc structures

// Conversion request from user
struct ConversionRequest {
  // Request type
  enum RequestType {
    CONVERSION = 0,
    PREDICTION = 1,
    SUGGESTION = 2,
  };

  RequestType request_type = CONVERSION;
  bool use_dictionary = true;
};

// Single candidate
struct Candidate {
  std::string value;          // Converted text (e.g., "今日")
  std::string content_value;  // Content value
  std::string content_key;    // Hiragana key
  std::string description;    // Description (e.g., "[AI]")
  int cost = 0;               // Candidate cost/priority
};

// Segment containing candidates
struct Segment {
  std::string key;                    // Hiragana key (e.g., "きょう")
  std::vector<Candidate> candidates;  // Candidate list

  // Convenience methods
  size_t candidates_size() const { return candidates.size(); }

  const Candidate& candidate(size_t i) const { return candidates[i]; }

  Candidate* add_candidate() {
    candidates.emplace_back();
    return &candidates.back();
  }

  Candidate* mutable_candidate(size_t i) {
    return &candidates[i];
  }

  void set_key(const std::string& k) { key = k; }
};

// Segments container
class Segments {
 public:
  // Get number of conversion segments
  size_t conversion_segments_size() const {
    return segments_.size();
  }

  // Get segment by index
  const Segment& conversion_segment(size_t i) const {
    return segments_[i];
  }

  // Get mutable segment by index
  Segment* mutable_conversion_segment(size_t i) {
    return &segments_[i];
  }

  // Add new segment
  Segment* add_segment() {
    segments_.emplace_back();
    return &segments_.back();
  }

 private:
  std::vector<Segment> segments_;
};

// Rewriter interface
// Each rewriter can modify conversion candidates
class RewriterInterface {
 public:
  virtual ~RewriterInterface() = default;

  // Capability flags
  enum Capability {
    NONE = 0,
    CONVERSION = 1 << 0,
    PREDICTION = 1 << 1,
    SUGGESTION = 1 << 2,
    ALL = CONVERSION | PREDICTION | SUGGESTION,
  };

  // Return supported capabilities
  virtual int capability(const ConversionRequest& request) const = 0;

  // Rewrite candidates
  // Returns true if rewriting was successful
  // Note: Returning false does not indicate an error, just no changes made
  virtual bool Rewrite(const ConversionRequest& request,
                       Segments* segments) const = 0;

  // Optional: Focus on specific segment
  virtual bool Focus(Segments* /*segments*/,
                     size_t /*segment_index*/,
                     int /*candidate_index*/) const {
    return false;
  }

  // Optional: Finish rewriting
  virtual void Finish(const ConversionRequest& /*request*/,
                      Segments* /*segments*/) {}

  // Optional: Clear state
  virtual void Clear() {}

  // Get rewriter name for debugging
  virtual std::string GetName() const { return "RewriterInterface"; }
};

}  // namespace mozc

#endif  // MOZC_REWRITER_REWRITER_INTERFACE_H_
