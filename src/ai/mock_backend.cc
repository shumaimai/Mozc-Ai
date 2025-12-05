// Copyright 2024 AI Mozc IME Project
// Mock Backend Implementation for Testing

#include "ai_backend.h"
#include "ai_config.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <cstdlib>

#define AI_LOG(msg) std::cerr << "[AI-Mozc Mock] " << msg << std::endl

namespace mozc {
namespace ai {

// Mock backend for testing purposes
class MockBackend : public AIBackendInterface {
 public:
  MockBackend() = default;

  bool Initialize() override {
    initialized_ = true;
    return true;
  }

  bool IsReady() const override {
    return initialized_;
  }

  AIGenerationResult Generate(
      const std::string& input,
      const std::vector<std::string>& /* existing_candidates */,
      const std::vector<std::string>& /* context */,
      int timeout_ms) override {

    AIGenerationResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (!initialized_) {
      result.error_message = "Mock backend not initialized";
      return result;
    }

    // Simulate delay if configured
    if (simulate_delay_ms_ > 0) {
      // Don't exceed timeout
      int actual_delay = std::min(simulate_delay_ms_, timeout_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(actual_delay));

      // Check if we exceeded timeout
      auto elapsed = std::chrono::steady_clock::now() - start_time;
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
      if (elapsed_ms >= timeout_ms) {
        result.error_message = "Simulated timeout";
        return result;
      }
    }

    // Simulate error if configured
    if (simulate_error_) {
      result.error_message = "Simulated error";
      return result;
    }

    // Generate mock candidates based on input
    result.candidates = GenerateMockCandidates(input);
    result.success = true;

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
  }

  std::string Name() const override {
    return "Mock";
  }

  std::string GetConfigInfo() const override {
    return "Mock[delay=" + std::to_string(simulate_delay_ms_) +
           "ms, error=" + (simulate_error_ ? "true" : "false") + "]";
  }

  // Test configuration methods
  void SetSimulateDelay(int ms) { simulate_delay_ms_ = ms; }
  void SetSimulateError(bool error) { simulate_error_ = error; }
  void SetCustomCandidates(const std::vector<std::string>& candidates) {
    custom_candidates_ = candidates;
  }

 private:
  std::vector<std::string> GenerateMockCandidates(const std::string& input) const {
    // If custom candidates are set, use them
    if (!custom_candidates_.empty()) {
      return custom_candidates_;
    }

    // Default mock candidates
    std::vector<std::string> candidates;

    // Japanese-aware mock responses
    if (input == "きょう" || input == "kyou") {
      candidates = {"今日", "教", "強"};
    } else if (input == "あした" || input == "ashita") {
      candidates = {"明日", "足下", "遊した"};
    } else if (input == "かいぎ" || input == "kaigi") {
      candidates = {"会議", "開議", "海技"};
    } else if (input == "しごと" || input == "shigoto") {
      candidates = {"仕事", "仕事場", "死語と"};
    } else if (input == "でんわ" || input == "denwa") {
      candidates = {"電話", "伝話", "田圃"};
    } else {
      // Generic candidates
      candidates = {
        "[モック候補1: " + input + "]",
        "[モック候補2: " + input + "]",
        "[モック候補3: " + input + "]"
      };
    }

    return candidates;
  }

  bool initialized_ = false;
  int simulate_delay_ms_ = 0;
  bool simulate_error_ = false;
  std::vector<std::string> custom_candidates_;
};

// Factory function
std::unique_ptr<AIBackendInterface> CreateMockBackend() {
  return std::make_unique<MockBackend>();
}

// Groq backend placeholder
class GroqBackend : public AIBackendInterface {
 public:
  explicit GroqBackend(const GroqConfig& config) : config_(config) {}

  bool Initialize() override {
    // TODO: Implement Groq API initialization
    // Get API key from environment variable
    const char* api_key = std::getenv(config_.api_key_env.c_str());
    if (!api_key) {
      return false;
    }
    api_key_ = api_key;
    initialized_ = true;
    return true;
  }

  bool IsReady() const override {
    return initialized_;
  }

  AIGenerationResult Generate(
      const std::string& /* input */,
      const std::vector<std::string>& /* existing_candidates */,
      const std::vector<std::string>& /* context */,
      int /* timeout_ms */) override {

    AIGenerationResult result;
    result.error_message = "Groq backend not yet implemented";
    // TODO: Implement Groq API calls
    return result;
  }

  std::string Name() const override {
    return "Groq";
  }

  std::string GetConfigInfo() const override {
    return "Groq[model=" + config_.model + "]";
  }

 private:
  GroqConfig config_;
  std::string api_key_;
  bool initialized_ = false;
};

std::unique_ptr<AIBackendInterface> CreateGroqBackend(const GroqConfig& config) {
  return std::make_unique<GroqBackend>(config);
}

}  // namespace ai
}  // namespace mozc
