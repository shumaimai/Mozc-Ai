// Copyright 2024 AI Mozc IME Project
// AI Logger Implementation

#include "ai_logger.h"
#include "ai_config.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <vector>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#include <share.h>
#define MKDIR(path) _mkdir(path)
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

std::mutex AILogger::mutex_;
std::string AILogger::log_path_;
bool AILogger::initialized_ = false;

namespace {

bool CreateDirectoryRecursive(const std::string& path) {
  if (path.empty()) return true;

  std::string current_path;
  std::string remaining = path;

#ifdef _WIN32
  if (remaining.size() >= 2 && remaining[1] == ':') {
    current_path = remaining.substr(0, 3);
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
        return false;
      }
    }
  }
  return true;
}

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

std::string GetUserLogDirectory() {
#ifdef _WIN32
  char path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
    return std::string(path) + "\\Google\\Mozc";
  }
  const char* localappdata = std::getenv("LOCALAPPDATA");
  if (localappdata) {
    return std::string(localappdata) + "\\Google\\Mozc";
  }
  return ".";
#else
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

std::string GetTempFallbackLogPath() {
#ifdef _WIN32
  char temp_path[MAX_PATH] = {};
  DWORD len = GetTempPathA(MAX_PATH, temp_path);
  if (len == 0 || len >= MAX_PATH) {
    return "";
  }
  std::string dir = std::string(temp_path) + "MozcAI";
  CreateDirectoryRecursive(dir);
  return dir + "\\ai_log.txt";
#else
  return "/tmp/mozc_ai_log.txt";
#endif
}

// Last-resort path that is easy to find in Explorer (user home).
std::string GetHomeFallbackLogPath() {
#ifdef _WIN32
  const char* profile = std::getenv("USERPROFILE");
  if (profile && profile[0] != '\0') {
    return std::string(profile) + "\\mozc_ai_log.txt";
  }
  return "mozc_ai_log.txt";
#else
  const char* home = std::getenv("HOME");
  if (home && home[0] != '\0') {
    return std::string(home) + "/mozc_ai_log.txt";
  }
  return "/tmp/mozc_ai_log.txt";
#endif
}

void EmitDebug(const std::string& msg) {
#ifdef _WIN32
  // Visible in Sysinternals DebugView (Capture Win32). Always available for IME.
  OutputDebugStringA(("[AI-Mozc] " + msg + "\n").c_str());
#endif
  std::cerr << "[AI-Mozc] " << msg << std::endl;
}

bool AppendLineToFile(const std::string& path, const std::string& line) {
#ifdef _WIN32
  FILE* file = _fsopen(path.c_str(), "a", _SH_DENYNO);
#else
  FILE* file = fopen(path.c_str(), "a");
#endif
  if (!file) {
    return false;
  }
  fwrite(line.data(), 1, line.size(), file);
  fflush(file);
  fclose(file);
  return true;
}

std::string ResolveLogDirectory() {
  // Prefer the same directory as ai_config.json (known to work for this user).
  std::string config_path = AIConfigManager::Instance().GetConfigPath();
  std::string config_dir = GetParentPath(config_path);
  if (!config_dir.empty()) {
    return config_dir;
  }
  return GetUserLogDirectory();
}

void WriteLogLocationMarker(const std::string& log_path) {
  std::string dir = GetParentPath(log_path);
  if (dir.empty()) {
    return;
  }
  std::string marker = dir +
#ifdef _WIN32
      "\\ai_log_location.txt";
#else
      "/ai_log_location.txt";
#endif
  std::string content = "ai_log.txt path:\n" + log_path + "\n";
  AppendLineToFile(marker, content);

  // Also drop a pointer next to ai_config.json when logging elsewhere.
  std::string config_dir = ResolveLogDirectory();
  if (!config_dir.empty() && config_dir != dir) {
    std::string config_marker = config_dir +
#ifdef _WIN32
        "\\ai_log_location.txt";
#else
        "/ai_log_location.txt";
#endif
    AppendLineToFile(config_marker, content);
  }
}

}  // namespace

void AILogger::Initialize() {
  EnsureOpen();
}

void AILogger::EnsureOpen() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (initialized_ && !log_path_.empty()) {
    return;
  }

  std::string log_dir = ResolveLogDirectory();
  CreateDirectoryRecursive(log_dir);

  std::vector<std::string> candidates;
  candidates.push_back(log_dir +
#ifdef _WIN32
                       "\\ai_log.txt"
#else
                       "/ai_log.txt"
#endif
  );
  std::string temp_path = GetTempFallbackLogPath();
  if (!temp_path.empty()) {
    candidates.push_back(temp_path);
  }
  candidates.push_back(GetHomeFallbackLogPath());

  std::string startup_prefix = "[" + GetTimestamp() + "] [INFO ] AI logger opened: ";
  for (const auto& candidate : candidates) {
    std::string startup = startup_prefix + candidate + "\n";
    if (AppendLineToFile(candidate, startup)) {
      log_path_ = candidate;
      initialized_ = true;
      WriteLogLocationMarker(log_path_);
      EmitDebug("logger opened: " + log_path_);
      return;
    }
    EmitDebug("logger open failed: " + candidate);
  }

  initialized_ = false;
  log_path_.clear();
  EmitDebug("logger open failed for all candidates");
}

std::string AILogger::GetLogPath() {
  if (!log_path_.empty()) {
    return log_path_;
  }
  return ResolveLogDirectory() +
#ifdef _WIN32
      "\\ai_log.txt";
#else
      "/ai_log.txt";
#endif
}

void AILogger::Trace(const std::string& msg) {
  Log(LogLevel::TRACE, msg);
}

void AILogger::Debug(const std::string& msg) {
  Log(LogLevel::DEBUG, msg);
}

void AILogger::Info(const std::string& msg) {
  Log(LogLevel::INFO, msg);
}

void AILogger::Warn(const std::string& msg) {
  Log(LogLevel::WARN, msg);
}

void AILogger::Error(const std::string& msg) {
  Log(LogLevel::ERROR, msg);
}

void AILogger::Perf(const std::string& operation, int64_t elapsed_ms) {
  std::ostringstream oss;
  oss << "[PERF] " << operation << ": " << elapsed_ms << "ms";
  Log(LogLevel::DEBUG, oss.str());
}

void AILogger::LogRequest(const std::string& prompt) {
  auto config = AIConfigManager::Instance().GetConfig();
  if (!config.log.log_ai_communication) {
    return;
  }

  // INFO so it appears with default log_level=info (was DEBUG before).
  std::ostringstream oss;
  oss << "[AI REQUEST] " << prompt;
  Log(LogLevel::INFO, oss.str());
}

void AILogger::LogResponse(const std::string& response) {
  auto config = AIConfigManager::Instance().GetConfig();
  if (!config.log.log_ai_communication) {
    return;
  }

  std::ostringstream oss;
  oss << "[AI RESPONSE] " << response;
  Log(LogLevel::INFO, oss.str());
}

void AILogger::Flush() {
  // Each line is flushed on write.
}

void AILogger::Log(LogLevel level, const std::string& msg) {
  EnsureOpen();

  if (!ShouldLog(level)) {
    return;
  }

  std::ostringstream line;
  line << "[" << GetTimestamp() << "] "
       << "[" << GetLevelString(level) << "] "
       << msg << "\n";

  // Always mirror to DebugView on Windows (works even when file logging fails).
  EmitDebug(GetLevelString(level) + std::string(": ") + msg);

  std::lock_guard<std::mutex> lock(mutex_);

  if (!initialized_ || log_path_.empty()) {
    return;
  }

  if (!AppendLineToFile(log_path_, line.str())) {
    EmitDebug("append failed to " + log_path_ + ": " + msg);
  }
}

bool AILogger::ShouldLog(LogLevel level) {
  auto config = AIConfigManager::Instance().GetConfig();
  return level >= config.log.level;
}

std::string AILogger::GetLevelString(LogLevel level) {
  switch (level) {
    case LogLevel::TRACE: return "TRACE";
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO:  return "INFO ";
    case LogLevel::WARN:  return "WARN ";
    case LogLevel::ERROR: return "ERROR";
    default: return "?????";
  }
}

std::string AILogger::GetTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()) % 1000;

  std::ostringstream oss;
  oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
      << "." << std::setfill('0') << std::setw(3) << ms.count();

  return oss.str();
}

ScopedTimer::ScopedTimer(const std::string& operation)
    : operation_(operation),
      start_(std::chrono::steady_clock::now()) {}

ScopedTimer::~ScopedTimer() {
  auto end = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      end - start_).count();
  AILogger::Perf(operation_, elapsed);
}

}  // namespace ai
}  // namespace mozc
