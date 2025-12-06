// Copyright 2024 AI Mozc IME Project
// AI Backend Interface Header

#ifndef MOZC_AI_AI_BACKEND_H_
#define MOZC_AI_AI_BACKEND_H_

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "ai_config.h"

namespace mozc {
namespace ai {

// Result of AI generation
struct AIGenerationResult {
  bool success = false;
  std::vector<std::string> candidates;
  std::string error_message;
  int elapsed_ms = 0;
};

// Abstract base class for AI backends
class AIBackendInterface {
 public:
  virtual ~AIBackendInterface() = default;

  // Initialize the backend
  // Returns true if initialization was successful
  virtual bool Initialize() = 0;

  // Check if backend is ready to process requests
  virtual bool IsReady() const = 0;

  // Generate candidates for given input
  // @param input The hiragana input string
  // @param existing_candidates Current Mozc candidates (to avoid duplicates)
  // @param context Recent conversion history for context
  // @param timeout_ms Timeout in milliseconds (CRITICAL for freeze prevention)
  // @return Generation result with candidates or error
  virtual AIGenerationResult Generate(
      const std::string& input,
      const std::vector<std::string>& existing_candidates,
      const std::vector<std::string>& context,
      int timeout_ms) = 0;

  // Get backend name for logging
  virtual std::string Name() const = 0;

  // Get current configuration
  virtual std::string GetConfigInfo() const = 0;
};

// Factory function to create appropriate backend based on configuration
std::unique_ptr<AIBackendInterface> CreateBackend(const AIConfig& config);

// Create Ollama backend
std::unique_ptr<AIBackendInterface> CreateOllamaBackend(const OllamaConfig& config);

// Create Groq backend (placeholder for future implementation)
std::unique_ptr<AIBackendInterface> CreateGroqBackend(const GroqConfig& config);

// Create Mock backend for testing
std::unique_ptr<AIBackendInterface> CreateMockBackend();

}  // namespace ai
}  // namespace mozc

#endif  // MOZC_AI_AI_BACKEND_H_
