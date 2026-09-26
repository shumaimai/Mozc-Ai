#pragma once
#include "converter/segments.h"
#include "converter/candidate.h"
namespace mozc {
namespace converter { struct Candidate; }
class Segment;

class ConversionRequest;
class RewriterInterface {
 public:
  enum Capability { NOT_AVAILABLE, CONVERSION };
  virtual ~RewriterInterface() = default;
  virtual int capability(const ConversionRequest&) const { return NOT_AVAILABLE; }
  virtual bool Rewrite(const ConversionRequest&, Segments*) const { return false; }
  virtual void Finish(const ConversionRequest&, const Segments&) {}
  virtual void Clear() {}
};
}  // namespace mozc
// stub: match base signature used by RerankRewriter
