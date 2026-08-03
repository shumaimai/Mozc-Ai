// Copyright 2024 AI Mozc IME Project
// Unit tests for AI Configuration Manager

#include "ai_config.h"

#include "gtest/gtest.h"

namespace mozc {
namespace ai {
namespace {

// Default configuration test
TEST(AIConfigTest, DefaultConfig) {
  AIConfig config = AIConfigManager::GetDefaultConfig();

  // Check default values
  EXPECT_TRUE(config.enabled);
  EXPECT_EQ(config.backend_type, BackendType::OPENAI_COMPATIBLE);

  // Ollama defaults
  EXPECT_EQ(config.ollama.endpoint, "http://localhost:11434");
  EXPECT_EQ(config.ollama.model, "gemma3:1b");

  // OpenAI-compatible defaults (DeepSeek)
  EXPECT_EQ(config.openai_compatible.endpoint, "https://api.deepseek.com/v1");
  EXPECT_EQ(config.openai_compatible.model, "deepseek-chat");
  EXPECT_EQ(config.openai_compatible.api_key_env, "DEEPSEEK_API_KEY");

  // Timeout defaults (critical for freeze prevention)
  EXPECT_EQ(config.timeout.connect_timeout_ms, 5000);
  EXPECT_EQ(config.timeout.request_timeout_ms, 15000);
  EXPECT_EQ(config.timeout.max_wait_ms, 16000);

  // Cache defaults
  EXPECT_EQ(config.cache.ttl_seconds, 60);
  EXPECT_EQ(config.cache.max_entries, 100);

  // Context defaults
  EXPECT_EQ(config.context.history_size, 5);

  // Debug defaults
  EXPECT_FALSE(config.debug.disable_ai);
  EXPECT_FALSE(config.debug.use_mock);
}

// Singleton instance test
TEST(AIConfigTest, SingletonInstance) {
  auto& instance1 = AIConfigManager::Instance();
  auto& instance2 = AIConfigManager::Instance();

  EXPECT_EQ(&instance1, &instance2);
}

// GetConfig returns copy test
TEST(AIConfigTest, GetConfigReturnsCopy) {
  auto& manager = AIConfigManager::Instance();

  AIConfig config1 = manager.GetConfig();
  AIConfig config2 = manager.GetConfig();

  // Modifying one shouldn't affect the other
  config1.enabled = false;
  EXPECT_TRUE(config2.enabled);
}

// IsEnabled test
TEST(AIConfigTest, IsEnabled) {
  auto& manager = AIConfigManager::Instance();

  // By default should be enabled
  EXPECT_TRUE(manager.IsEnabled());
}

// Timeout values sanity check
TEST(AIConfigTest, TimeoutSanity) {
  AIConfig config = AIConfigManager::GetDefaultConfig();

  // Connection timeout should allow cloud API handshake
  EXPECT_GE(config.timeout.connect_timeout_ms, 1000);

  // Request timeout should be reasonable for cloud APIs
  EXPECT_GT(config.timeout.request_timeout_ms, 0);
  EXPECT_LE(config.timeout.request_timeout_ms, 30000);

  // Max wait should be >= request timeout
  EXPECT_GE(config.timeout.max_wait_ms, config.timeout.request_timeout_ms);
}

// Cache config sanity check
TEST(AIConfigTest, CacheSanity) {
  AIConfig config = AIConfigManager::GetDefaultConfig();

  EXPECT_GT(config.cache.max_entries, 0);
  EXPECT_GT(config.cache.ttl_seconds, 0);
}

// Config path test
TEST(AIConfigTest, ConfigPath) {
  auto& manager = AIConfigManager::Instance();
  std::string path = manager.GetConfigPath();

  // Path should not be empty
  EXPECT_FALSE(path.empty());

  // Path should end with ai_config.json
  EXPECT_NE(path.find("ai_config.json"), std::string::npos);
}

// Backend type enumeration test
TEST(AIConfigTest, BackendTypes) {
  EXPECT_EQ(static_cast<int>(BackendType::DISABLED), 0);
  EXPECT_EQ(static_cast<int>(BackendType::OLLAMA), 1);
  EXPECT_EQ(static_cast<int>(BackendType::GROQ), 2);
  EXPECT_EQ(static_cast<int>(BackendType::OPENAI_COMPATIBLE), 3);
}

// Log level enumeration test
TEST(AIConfigTest, LogLevels) {
  EXPECT_EQ(static_cast<int>(LogLevel::TRACE), 0);
  EXPECT_EQ(static_cast<int>(LogLevel::DEBUG), 1);
  EXPECT_EQ(static_cast<int>(LogLevel::INFO), 2);
  EXPECT_EQ(static_cast<int>(LogLevel::WARN), 3);
  EXPECT_EQ(static_cast<int>(LogLevel::ERROR), 4);
}

// Groq config defaults
TEST(AIConfigTest, GroqDefaults) {
  AIConfig config = AIConfigManager::GetDefaultConfig();

  EXPECT_EQ(config.groq.api_key_env, "GROQ_API_KEY");
  EXPECT_EQ(config.groq.model, "mixtral-8x7b-32768");
}

// Context config test
TEST(AIConfigTest, ContextConfig) {
  AIConfig config = AIConfigManager::GetDefaultConfig();

  EXPECT_GT(config.context.history_size, 0);
  EXPECT_LE(config.context.history_size, 10);

  EXPECT_GT(config.context.history_expire_min, 0);
}

}  // namespace
}  // namespace ai
}  // namespace mozc
