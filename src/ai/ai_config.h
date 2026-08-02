// Copyright 2024 AI Mozc IME Project
// AI Configuration Manager Header

#ifndef MOZC_AI_AI_CONFIG_H_
#define MOZC_AI_AI_CONFIG_H_

#include <string>
#include <mutex>
#include <memory>

namespace mozc {
namespace ai {

// Forward declaration for protobuf types
// In actual implementation, include "ai/ai_config.pb.h"

// Backend type enumeration
enum class BackendType {
  DISABLED = 0,
  OLLAMA = 1,
  GROQ = 2,
  OPENAI_COMPATIBLE = 3
};

// Log level enumeration
enum class LogLevel {
  TRACE = 0,
  DEBUG = 1,
  INFO = 2,
  WARN = 3,
  ERROR = 4
};

// Ollama configuration
struct OllamaConfig {
  std::string endpoint = "http://localhost:11434";
  std::string model = "gemma3:1b";  // Lightweight model for faster response
};

// Groq configuration
struct GroqConfig {
  std::string api_key_env = "GROQ_API_KEY";
  std::string model = "mixtral-8x7b-32768";
};

// OpenAI-compatible API configuration (DeepSeek, Groq, OpenAI, LM Studio, etc.)
struct OpenAICompatibleConfig {
  std::string endpoint = "https://api.deepseek.com/v1";
  std::string model = "deepseek-chat";
  std::string api_key_env = "DEEPSEEK_API_KEY";
};

// Timeout configuration (Critical for freeze prevention)
struct TimeoutConfig {
  int connect_timeout_ms = 50;     // Very short connection timeout
  int request_timeout_ms = 500;    // Processing timeout
  int max_wait_ms = 600;           // Maximum wait time
  int warmup_timeout_ms = 60000;   // Warmup timeout (60 seconds)
};

// Cache configuration
struct CacheConfig {
  int ttl_seconds = 60;
  int max_entries = 100;
  bool include_context_in_key = true;
};

// Context configuration
struct ContextConfig {
  int history_size = 5;
  int history_expire_min = 5;
};

// Log configuration
struct LogConfig {
  LogLevel level = LogLevel::INFO;
  bool log_ai_communication = false;
};

// Debug configuration
struct DebugConfig {
  bool disable_ai = false;
  bool use_mock = false;
};

// Main AI configuration structure
struct AIConfig {
  bool enabled = true;
  BackendType backend_type = BackendType::OPENAI_COMPATIBLE;
  OllamaConfig ollama;
  GroqConfig groq;
  OpenAICompatibleConfig openai_compatible;
  TimeoutConfig timeout;
  CacheConfig cache;
  ContextConfig context;
  LogConfig log;
  DebugConfig debug;
};

// Singleton configuration manager
class AIConfigManager {
 public:
  // Get singleton instance
  static AIConfigManager& Instance();

  // Get current configuration (thread-safe copy)
  AIConfig GetConfig() const;

  // Load configuration from file
  void Load();

  // Reload configuration
  void Reload();

  // Get default configuration
  static AIConfig GetDefaultConfig();

  // Get configuration file path
  std::string GetConfigPath() const;

  // Check if AI is enabled
  bool IsEnabled() const;

 private:
  AIConfigManager();
  ~AIConfigManager() = default;

  // Non-copyable
  AIConfigManager(const AIConfigManager&) = delete;
  AIConfigManager& operator=(const AIConfigManager&) = delete;

  // Load configuration from JSON file
  bool LoadFromJson(const std::string& path);

  // Save configuration to JSON file
  bool SaveToJson(const std::string& path) const;

  mutable std::mutex mutex_;
  AIConfig config_;
  bool loaded_ = false;
};

}  // namespace ai
}  // namespace mozc

#endif  // MOZC_AI_AI_CONFIG_H_
