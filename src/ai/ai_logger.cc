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
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#include <share.h>
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

// Static member initialization
std::mutex AILogger::mutex_;
std::ofstream AILogger::log_file_;
bool AILogger::initialized_ = false;

namespace {

// Create directory recursively (cross-platform, no std::filesystem dependency)
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

std::string& ActiveLogPath() {
  static std::string path;
  return path;
}

std::string PrimaryLogPath() {
  return GetUserLogDirectory() +
#ifdef _WIN32
      "\\ai_log.txt";
#else
      "/ai_log.txt";
#endif
}

std::string GetFallbackLogPath() {
#ifdef _WIN32
  char temp_path[MAX_PATH] = {};
  DWORD len = GetTempPathA(MAX_PATH, temp_path);
  if (len == 0 || len >= MAX_PATH) {
    return "ai_log.txt";
  }
  std::string dir = std::string(temp_path) + "MozcAI";
  CreateDirectoryRecursive(dir);
  return dir + "\\ai_log.txt";
#else
  return "/tmp/mozc_ai_log.txt";
#endif
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

}  // namespace

void AILogger::Initialize() {
  EnsureOpen();
}

void AILogger::EnsureOpen() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (initialized_ && log_file_.is_open()) {
    return;
  }

  initialized_ = false;
  log_file_.close();

  std::string primary = PrimaryLogPath();
  CreateDirectoryRecursive(GetUserLogDirectory());

  std::string log_path = primary;
  log_file_.open(primary, std::ios::app);
  if (!log_file_.is_open()) {
    log_path = GetFallbackLogPath();
    log_file_.open(log_path, std::ios::app);
    if (!log_file_.is_open()) {
      std::cerr << "[AI-Mozc Logger] Failed to open log file: " << primary
                << " and fallback: " << log_path << std::endl;
      return;
    }
    std::cerr << "[AI-Mozc Logger] Using fallback log: " << log_path << std::endl;
  }

  ActiveLogPath() = log_path;
  initialized_ = true;
  std::string startup = "[" + GetTimestamp() + "] [INFO ] AI logger opened: " +
                        log_path + "\n";
  log_file_ << startup;
  log_file_.flush();
}

std::string AILogger::GetLogPath() {
  if (!ActiveLogPath().empty()) {
    return ActiveLogPath();
  }
  return PrimaryLogPath();
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
  // Also output to stderr for visibility
  std::cerr << "[AI-Mozc ERROR] " << msg << std::endl;
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

  std::ostringstream oss;
  oss << "[AI REQUEST] " << prompt;
  Log(LogLevel::TRACE, oss.str());
}

void AILogger::LogResponse(const std::string& response) {
  auto config = AIConfigManager::Instance().GetConfig();
  if (!config.log.log_ai_communication) {
    return;
  }

  std::ostringstream oss;
  oss << "[AI RESPONSE] " << response;
  Log(LogLevel::TRACE, oss.str());
}

void AILogger::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (log_file_.is_open()) {
    log_file_.flush();
  }
}

void AILogger::Log(LogLevel level, const std::string& msg) {
  EnsureOpen();

  if (!ShouldLog(level)) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  std::ostringstream line;
  line << "[" << GetTimestamp() << "] "
       << "[" << GetLevelString(level) << "] "
       << msg << "\n";
  const std::string line_str = line.str();

  if (log_file_.is_open()) {
    log_file_ << line_str;
    if (level >= LogLevel::WARN) {
      log_file_.flush();
    }
    return;
  }

  const std::string path = ActiveLogPath().empty() ? PrimaryLogPath() : ActiveLogPath();
  if (!AppendLineToFile(path, line_str)) {
    std::cerr << "[AI-Mozc] " << GetLevelString(level) << ": " << msg << std::endl;
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

// ScopedTimer implementation
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
