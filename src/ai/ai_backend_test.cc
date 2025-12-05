// Copyright 2024 AI Mozc IME Project
// Unit tests for AI Backend

#include "ai/ai_backend.h"

#include "gtest/gtest.h"

namespace mozc {
namespace ai {
namespace {

// Mock backend creation test
TEST(AIBackendTest, CreateMockBackend) {
  auto backend = CreateMockBackend();

  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->Name(), "Mock");
}

// Mock backend initialization test
TEST(AIBackendTest, MockBackendInitialize) {
  auto backend = CreateMockBackend();

  ASSERT_NE(backend, nullptr);
  EXPECT_TRUE(backend->Initialize());
  EXPECT_TRUE(backend->IsReady());
}

// Mock backend generate test
TEST(AIBackendTest, MockBackendGenerate) {
  auto backend = CreateMockBackend();
  ASSERT_TRUE(backend->Initialize());

  auto result = backend->Generate(
      "きょう",
      {"今日"},
      {},
      1000);

  EXPECT_TRUE(result.success);
  EXPECT_FALSE(result.candidates.empty());
  EXPECT_TRUE(result.error_message.empty());
}

// Mock backend Japanese candidates
TEST(AIBackendTest, MockBackendJapaneseCandidates) {
  auto backend = CreateMockBackend();
  ASSERT_TRUE(backend->Initialize());

  auto result = backend->Generate("きょう", {}, {}, 1000);

  EXPECT_TRUE(result.success);
  // Mock should return Japanese candidates
  EXPECT_FALSE(result.candidates.empty());
}

// Ollama backend creation test
TEST(AIBackendTest, CreateOllamaBackend) {
  OllamaConfig config;
  config.endpoint = "http://localhost:11434";
  config.model = "mistral:7b";

  auto backend = CreateOllamaBackend(config);

  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->Name(), "Ollama");
}

// Groq backend creation test
TEST(AIBackendTest, CreateGroqBackend) {
  GroqConfig config;
  config.api_key_env = "GROQ_API_KEY";
  config.model = "mixtral-8x7b-32768";

  auto backend = CreateGroqBackend(config);

  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->Name(), "Groq");
}

// Factory function with different backend types
TEST(AIBackendTest, CreateBackendFactory) {
  // Ollama
  {
    AIConfig config;
    config.backend_type = BackendType::OLLAMA;
    auto backend = CreateBackend(config);
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->Name(), "Ollama");
  }

  // Groq
  {
    AIConfig config;
    config.backend_type = BackendType::GROQ;
    auto backend = CreateBackend(config);
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->Name(), "Groq");
  }

  // Disabled
  {
    AIConfig config;
    config.backend_type = BackendType::DISABLED;
    auto backend = CreateBackend(config);
    EXPECT_EQ(backend, nullptr);
  }
}

// Mock backend config info
TEST(AIBackendTest, MockBackendConfigInfo) {
  auto backend = CreateMockBackend();

  std::string info = backend->GetConfigInfo();
  EXPECT_FALSE(info.empty());
  EXPECT_NE(info.find("Mock"), std::string::npos);
}

// Ollama backend config info
TEST(AIBackendTest, OllamaBackendConfigInfo) {
  OllamaConfig config;
  config.endpoint = "http://localhost:11434";
  config.model = "mistral:7b";

  auto backend = CreateOllamaBackend(config);
  ASSERT_TRUE(backend->Initialize());

  std::string info = backend->GetConfigInfo();
  EXPECT_FALSE(info.empty());
  EXPECT_NE(info.find("Ollama"), std::string::npos);
  EXPECT_NE(info.find("localhost"), std::string::npos);
  EXPECT_NE(info.find("mistral"), std::string::npos);
}

// Result structure test
TEST(AIBackendTest, GenerationResult) {
  AIGenerationResult result;

  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.candidates.empty());
  EXPECT_TRUE(result.error_message.empty());
  EXPECT_EQ(result.elapsed_ms, 0);
}

// Backend not initialized test
TEST(AIBackendTest, NotInitializedGenerate) {
  auto backend = CreateMockBackend();
  // Don't call Initialize()

  EXPECT_FALSE(backend->IsReady());

  auto result = backend->Generate("test", {}, {}, 1000);
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
}

}  // namespace
}  // namespace ai
}  // namespace mozc
