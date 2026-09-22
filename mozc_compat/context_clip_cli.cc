// Standalone parity CLI for the context contract. This source is copied into
// an upstream Mozc src/rewriter checkout by the Phase 0 build command.
#define MOZC_RERANK_STANDALONE
#include "context_clip.h"

#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  std::string op = "clean";
  int start = 0;
  std::string preceding;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--op" && i + 1 < argc) op = argv[++i];
    if (std::string(argv[i]) == "--start" && i + 1 < argc) start = std::stoi(argv[++i]);
    if (std::string(argv[i]) == "--preceding" && i + 1 < argc) preceding = argv[++i];
  }
  const std::string input((std::istreambuf_iterator<char>(std::cin)),
                          std::istreambuf_iterator<char>());
  if (op == "clean") {
    std::cout << mozc::rerank::CleanContext(input);
  } else if (op == "clip") {
    std::cout << mozc::rerank::ClipContextPrev(input, start);
  } else if (op == "reading") {
    std::cout << mozc::rerank::NormalizeReading(input);
  } else if (op == "runtime") {
    std::vector<std::string> prefix_top1;
    size_t begin = 0;
    while (begin <= input.size()) {
      const size_t end = input.find('\n', begin);
      const std::string surface = input.substr(begin, end - begin);
      if (!surface.empty()) prefix_top1.push_back(surface);
      if (end == std::string::npos) break;
      begin = end + 1;
    }
    std::cout << mozc::rerank::BuildRuntimeContext(preceding, prefix_top1);
  } else {
    return 2;
  }
  return 0;
}
