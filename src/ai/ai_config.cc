// Copyright 2024 AI Mozc IME Project
// AI Configuration Manager Implementation

#include "ai/ai_config.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace mozc {
namespace ai {

namespace {

// Get user profile directory
std::string GetUserProfileDirectory() {
#ifdef _WIN32
  char path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
    return std::string(path) + "\\Google\\Mozc";
  }
  // Fallback to LOCALAPPDATA environment variable
  const char* localappdata = std::getenv("LOCALAPPDATA");
  if (localappdata) {
    return std::string(localappdata) + "\\Google\\Mozc";
  }
  return ".";
#else
  // Linux/Unix
  const char* home = std::getenv("HOME");
  if (!home) {
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
      home = pw->pw_dir;
    }
  }
  if (home) {
    return std::string(home) + "/.mozc";
  }
  return ".";
#endif
}

// Simple JSON parser for configuration
// Note: In production, use a proper JSON library like nlohmann/json
class SimpleJsonParser {
 public:
  static bool Parse(const std::string& json, AIConfig& config) {
    // Parse enabled
    config.enabled = GetBool(json, "enabled", true);

    // Parse backend_type
    std::string backend = GetString(json, "backend_type", "ollama");
    if (backend == "disabled") {
      config.backend_type = BackendType::DISABLED;
    } else if (backend == "groq") {
      config.backend_type = BackendType::GROQ;
    } else {
      config.backend_type = BackendType::OLLAMA;
    }

    // Parse ollama config
    config.ollama.endpoint = GetString(json, "ollama_endpoint",
                                        "http://localhost:11434");
    config.ollama.model = GetString(json, "ollama_model", "mistral:7b");

    // Parse groq config
    config.groq.api_key_env = GetString(json, "groq_api_key_env", "GROQ_API_KEY");
    config.groq.model = GetString(json, "groq_model", "mixtral-8x7b-32768");

    // Parse timeout config
    config.timeout.connect_timeout_ms = GetInt(json, "connect_timeout_ms", 50);
    config.timeout.request_timeout_ms = GetInt(json, "request_timeout_ms", 500);
    config.timeout.max_wait_ms = GetInt(json, "max_wait_ms", 600);
    config.timeout.warmup_timeout_ms = GetInt(json, "warmup_timeout_ms", 60000);

    // Parse cache config
    config.cache.ttl_seconds = GetInt(json, "cache_ttl_seconds", 60);
    config.cache.max_entries = GetInt(json, "cache_max_entries", 100);
    config.cache.include_context_in_key = GetBool(json, "cache_include_context", true);

    // Parse context config
    config.context.history_size = GetInt(json, "history_size", 5);
    config.context.history_expire_min = GetInt(json, "history_expire_min", 5);

    // Parse log config
    std::string log_level = GetString(json, "log_level", "info");
    if (log_level == "trace") config.log.level = LogLevel::TRACE;
    else if (log_level == "debug") config.log.level = LogLevel::DEBUG;
    else if (log_level == "warn") config.log.level = LogLevel::WARN;
    else if (log_level == "error") config.log.level = LogLevel::ERROR;
    else config.log.level = LogLevel::INFO;

    config.log.log_ai_communication = GetBool(json, "log_ai_communication", false);

    // Parse debug config
    config.debug.disable_ai = GetBool(json, "disable_ai", false);
    config.debug.use_mock = GetBool(json, "use_mock", false);

    return true;
  }

  static std::string Serialize(const AIConfig& config) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"enabled\": " << (config.enabled ? "true" : "false") << ",\n";

    std::string backend_str = "ollama";
    if (config.backend_type == BackendType::DISABLED) backend_str = "disabled";
    else if (config.backend_type == BackendType::GROQ) backend_str = "groq";
    oss << "  \"backend_type\": \"" << backend_str << "\",\n";

    oss << "  \"ollama_endpoint\": \"" << config.ollama.endpoint << "\",\n";
    oss << "  \"ollama_model\": \"" << config.ollama.model << "\",\n";

    oss << "  \"groq_api_key_env\": \"" << config.groq.api_key_env << "\",\n";
    oss << "  \"groq_model\": \"" << config.groq.model << "\",\n";

    oss << "  \"connect_timeout_ms\": " << config.timeout.connect_timeout_ms << ",\n";
    oss << "  \"request_timeout_ms\": " << config.timeout.request_timeout_ms << ",\n";
    oss << "  \"max_wait_ms\": " << config.timeout.max_wait_ms << ",\n";
    oss << "  \"warmup_timeout_ms\": " << config.timeout.warmup_timeout_ms << ",\n";

    oss << "  \"cache_ttl_seconds\": " << config.cache.ttl_seconds << ",\n";
    oss << "  \"cache_max_entries\": " << config.cache.max_entries << ",\n";
    oss << "  \"cache_include_context\": " << (config.cache.include_context_in_key ? "true" : "false") << ",\n";

    oss << "  \"history_size\": " << config.context.history_size << ",\n";
    oss << "  \"history_expire_min\": " << config.context.history_expire_min << ",\n";

    std::string level_str = "info";
    if (config.log.level == LogLevel::TRACE) level_str = "trace";
    else if (config.log.level == LogLevel::DEBUG) level_str = "debug";
    else if (config.log.level == LogLevel::WARN) level_str = "warn";
    else if (config.log.level == LogLevel::ERROR) level_str = "error";
    oss << "  \"log_level\": \"" << level_str << "\",\n";
    oss << "  \"log_ai_communication\": " << (config.log.log_ai_communication ? "true" : "false") << ",\n";

    oss << "  \"disable_ai\": " << (config.debug.disable_ai ? "true" : "false") << ",\n";
    oss << "  \"use_mock\": " << (config.debug.use_mock ? "true" : "false") << "\n";
    oss << "}\n";

    return oss.str();
  }

 private:
  static std::string GetString(const std::string& json, const std::string& key,
                                const std::string& default_value) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_value;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return default_value;

    pos = json.find('"', pos);
    if (pos == std::string::npos) return default_value;

    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return default_value;

    return json.substr(pos + 1, end - pos - 1);
  }

  static int GetInt(const std::string& json, const std::string& key,
                    int default_value) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_value;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return default_value;

    // Skip whitespace
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ')) {
      ++pos;
    }

    std::string num_str;
    while (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '-')) {
      num_str += json[pos++];
    }

    if (num_str.empty()) return default_value;
    return std::stoi(num_str);
  }

  static bool GetBool(const std::string& json, const std::string& key,
                      bool default_value) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_value;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return default_value;

    if (json.find("true", pos) < json.find(',', pos) ||
        json.find("true", pos) < json.find('}', pos)) {
      return true;
    }
    if (json.find("false", pos) < json.find(',', pos) ||
        json.find("false", pos) < json.find('}', pos)) {
      return false;
    }
    return default_value;
  }
};

}  // namespace

AIConfigManager& AIConfigManager::Instance() {
  static AIConfigManager instance;
  return instance;
}

AIConfigManager::AIConfigManager() {
  config_ = GetDefaultConfig();
  Load();
}

AIConfig AIConfigManager::GetDefaultConfig() {
  AIConfig config;
  config.enabled = true;
  config.backend_type = BackendType::OLLAMA;

  // Ollama defaults
  config.ollama.endpoint = "http://localhost:11434";
  config.ollama.model = "mistral:7b";

  // Timeout defaults (Critical for freeze prevention)
  config.timeout.connect_timeout_ms = 50;    // 50ms - very short
  config.timeout.request_timeout_ms = 500;   // 500ms
  config.timeout.max_wait_ms = 600;          // 600ms max
  config.timeout.warmup_timeout_ms = 60000;  // 60s for warmup

  // Cache defaults
  config.cache.ttl_seconds = 60;
  config.cache.max_entries = 100;
  config.cache.include_context_in_key = true;

  // Context defaults
  config.context.history_size = 5;
  config.context.history_expire_min = 5;

  // Log defaults
  config.log.level = LogLevel::INFO;
  config.log.log_ai_communication = false;

  // Debug defaults
  config.debug.disable_ai = false;
  config.debug.use_mock = false;

  return config;
}

AIConfig AIConfigManager::GetConfig() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_;
}

std::string AIConfigManager::GetConfigPath() const {
  return GetUserProfileDirectory() +
#ifdef _WIN32
         "\\ai_config.json";
#else
         "/ai_config.json";
#endif
}

void AIConfigManager::Load() {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string path = GetConfigPath();

  // Check if file exists
  std::ifstream file(path);
  if (!file.is_open()) {
    // File doesn't exist, save default config
    SaveToJson(path);
    loaded_ = true;
    return;
  }

  // Read file content
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();
  file.close();

  // Parse JSON
  AIConfig loaded_config;
  if (SimpleJsonParser::Parse(content, loaded_config)) {
    config_ = loaded_config;
  }

  loaded_ = true;
}

void AIConfigManager::Reload() {
  loaded_ = false;
  Load();
}

bool AIConfigManager::LoadFromJson(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();
  file.close();

  return SimpleJsonParser::Parse(content, config_);
}

bool AIConfigManager::SaveToJson(const std::string& path) const {
  // Create directory if it doesn't exist
  std::filesystem::path dir_path = std::filesystem::path(path).parent_path();
  if (!dir_path.empty()) {
    std::filesystem::create_directories(dir_path);
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }

  file << SimpleJsonParser::Serialize(config_);
  file.close();
  return true;
}

bool AIConfigManager::IsEnabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_.enabled && !config_.debug.disable_ai;
}

}  // namespace ai
}  // namespace mozc
