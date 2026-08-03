// Copyright 2024 AI Mozc IME Project
// AI Configuration Manager Implementation

#include "ai_config.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#define MKDIR(path) _mkdir(path)
// Undefine Windows macros that conflict with our code
#undef ERROR
#undef min
#undef max
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

namespace mozc {
namespace ai {

namespace {

// Build log helper - outputs to stderr for visibility during build
void BuildLog(const std::string& msg) {
  std::cerr << "[AI-Mozc] " << msg << std::endl;
}

// Create directory recursively (cross-platform, no std::filesystem dependency)
bool CreateDirectoryRecursive(const std::string& path) {
  if (path.empty()) return true;

  std::string current_path;
  std::string remaining = path;

#ifdef _WIN32
  // Handle Windows paths
  if (remaining.size() >= 2 && remaining[1] == ':') {
    current_path = remaining.substr(0, 3);  // "C:\"
    remaining = remaining.substr(3);
  }
  char separator = '\\';
#else
  if (!remaining.empty() && remaining[0] == '/') {
    current_path = "/";
    remaining = remaining.substr(1);
  }
  char separator = '/';
#endif

  size_t pos = 0;
  while ((pos = remaining.find(separator)) != std::string::npos || !remaining.empty()) {
    std::string component;
    if (pos != std::string::npos) {
      component = remaining.substr(0, pos);
      remaining = remaining.substr(pos + 1);
    } else {
      component = remaining;
      remaining.clear();
    }

    if (component.empty()) continue;

    if (!current_path.empty() && current_path.back() != separator) {
      current_path += separator;
    }
    current_path += component;

    struct stat st;
    if (stat(current_path.c_str(), &st) != 0) {
      if (MKDIR(current_path.c_str()) != 0 && errno != EEXIST) {
        BuildLog("Failed to create directory: " + current_path);
        return false;
      }
    }
  }
  return true;
}

// Get parent directory path
std::string GetParentPath(const std::string& path) {
#ifdef _WIN32
  char separator = '\\';
#else
  char separator = '/';
#endif
  size_t pos = path.rfind(separator);
  if (pos == std::string::npos) return "";
  return path.substr(0, pos);
}

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
    BuildLog("Parsing configuration JSON...");

    // Parse enabled
    config.enabled = GetBool(json, "enabled", true);

    // Parse backend_type
    std::string backend = GetString(json, "backend_type", "deepseek");
    if (backend == "disabled") {
      config.backend_type = BackendType::DISABLED;
    } else if (backend == "groq") {
      config.backend_type = BackendType::GROQ;
    } else if (backend == "ollama") {
      config.backend_type = BackendType::OLLAMA;
    } else if (backend == "deepseek" || backend == "openai_compatible" ||
               backend == "openai") {
      config.backend_type = BackendType::OPENAI_COMPATIBLE;
    } else {
      config.backend_type = BackendType::OPENAI_COMPATIBLE;
    }

    // Parse ollama config
    config.ollama.endpoint = GetString(json, "ollama_endpoint",
                                        "http://localhost:11434");
    config.ollama.model = GetString(json, "ollama_model", "gemma3:1b");

    // Parse groq config
    config.groq.api_key_env = GetString(json, "groq_api_key_env", "GROQ_API_KEY");
    config.groq.model = GetString(json, "groq_model", "mixtral-8x7b-32768");

    // Parse OpenAI-compatible config (DeepSeek, etc.)
    config.openai_compatible.endpoint = GetString(
        json, "api_endpoint", "https://api.deepseek.com/v1");
    config.openai_compatible.model = GetString(json, "api_model", "deepseek-chat");
    config.openai_compatible.api_key_env = GetString(
        json, "api_key_env", "DEEPSEEK_API_KEY");

    // Parse timeout config
    config.timeout.connect_timeout_ms = GetInt(json, "connect_timeout_ms", 5000);
    config.timeout.request_timeout_ms = GetInt(json, "request_timeout_ms", 15000);
    config.timeout.max_wait_ms = GetInt(json, "max_wait_ms", 16000);
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

    BuildLog("Configuration parsed successfully");
    return true;
  }

  static std::string Serialize(const AIConfig& config) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"enabled\": " << (config.enabled ? "true" : "false") << ",\n";

    std::string backend_str = "deepseek";
    if (config.backend_type == BackendType::DISABLED) backend_str = "disabled";
    else if (config.backend_type == BackendType::GROQ) backend_str = "groq";
    else if (config.backend_type == BackendType::OLLAMA) backend_str = "ollama";
    else if (config.backend_type == BackendType::OPENAI_COMPATIBLE) {
      backend_str = "deepseek";
    }
    oss << "  \"backend_type\": \"" << backend_str << "\",\n";

    oss << "  \"ollama_endpoint\": \"" << config.ollama.endpoint << "\",\n";
    oss << "  \"ollama_model\": \"" << config.ollama.model << "\",\n";

    oss << "  \"groq_api_key_env\": \"" << config.groq.api_key_env << "\",\n";
    oss << "  \"groq_model\": \"" << config.groq.model << "\",\n";

    oss << "  \"api_endpoint\": \"" << config.openai_compatible.endpoint << "\",\n";
    oss << "  \"api_model\": \"" << config.openai_compatible.model << "\",\n";
    oss << "  \"api_key_env\": \"" << config.openai_compatible.api_key_env << "\",\n";

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
    while (pos < json.size() && (std::isdigit(static_cast<unsigned char>(json[pos])) || json[pos] == '-')) {
      num_str += json[pos++];
    }

    if (num_str.empty()) return default_value;
    try {
      return std::stoi(num_str);
    } catch (...) {
      return default_value;
    }
  }

  static bool GetBool(const std::string& json, const std::string& key,
                      bool default_value) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_value;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return default_value;

    size_t comma_pos = json.find(',', pos);
    size_t brace_pos = json.find('}', pos);
    size_t end_pos = std::min(comma_pos, brace_pos);

    size_t true_pos = json.find("true", pos);
    size_t false_pos = json.find("false", pos);

    if (true_pos != std::string::npos && true_pos < end_pos) {
      return true;
    }
    if (false_pos != std::string::npos && false_pos < end_pos) {
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
  BuildLog("Initializing AIConfigManager...");
  config_ = GetDefaultConfig();
  Load();
  BuildLog("AIConfigManager initialized");
}

AIConfig AIConfigManager::GetDefaultConfig() {
  AIConfig config;
  config.enabled = true;
  config.backend_type = BackendType::OPENAI_COMPATIBLE;

  // Ollama defaults
  config.ollama.endpoint = "http://localhost:11434";
  config.ollama.model = "gemma3:1b";

  // OpenAI-compatible defaults (DeepSeek)
  config.openai_compatible.endpoint = "https://api.deepseek.com/v1";
  config.openai_compatible.model = "deepseek-chat";
  config.openai_compatible.api_key_env = "DEEPSEEK_API_KEY";

  // Timeout defaults (cloud API needs longer; still bounded)
  config.timeout.connect_timeout_ms = 5000;
  config.timeout.request_timeout_ms = 15000;
  config.timeout.max_wait_ms = 16000;
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
  BuildLog("Loading config from: " + path);

  // Check if file exists
  std::ifstream file(path);
  if (!file.is_open()) {
    BuildLog("Config file not found, creating default config");
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
    BuildLog("Config loaded successfully");
  } else {
    BuildLog("Failed to parse config, using defaults");
  }

  loaded_ = true;
}

void AIConfigManager::Reload() {
  BuildLog("Reloading configuration...");
  loaded_ = false;
  Load();
}

bool AIConfigManager::LoadFromJson(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    BuildLog("Cannot open file: " + path);
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
  std::string dir_path = GetParentPath(path);
  if (!dir_path.empty()) {
    if (!CreateDirectoryRecursive(dir_path)) {
      BuildLog("Failed to create config directory: " + dir_path);
      return false;
    }
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    BuildLog("Cannot create config file: " + path);
    return false;
  }

  file << SimpleJsonParser::Serialize(config_);
  file.close();
  BuildLog("Config saved to: " + path);
  return true;
}

bool AIConfigManager::IsEnabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_.enabled && !config_.debug.disable_ai;
}

}  // namespace ai
}  // namespace mozc
