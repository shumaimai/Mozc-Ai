// Copyright 2024 AI Mozc IME Project
// AI Logger Header

#ifndef MOZC_AI_AI_LOGGER_H_
#define MOZC_AI_AI_LOGGER_H_

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>

#include "ai_config.h"

namespace mozc {
namespace ai {

// Logger for AI-related operations
class AILogger {
 public:
  // Log levels
  static void Trace(const std::string& msg);
  static void Debug(const std::string& msg);
  static void Info(const std::string& msg);
  static void Warn(const std::string& msg);
  static void Error(const std::string& msg);

  // Performance logging
  static void Perf(const std::string& operation, int64_t elapsed_ms);

  // AI communication logging (for debugging)
  static void LogRequest(const std::string& prompt);
  static void LogResponse(const std::string& response);

  // Initialize logger
  static void Initialize();

  // Flush log file
  static void Flush();

  // Get log file path
  static std::string GetLogPath();

 private:
  static void Log(LogLevel level, const std::string& msg);
  static bool ShouldLog(LogLevel level);
  static std::string GetLevelString(LogLevel level);
  static std::string GetTimestamp();

  static std::mutex mutex_;
  static std::ofstream log_file_;
  static bool initialized_;
};

// RAII class for timing operations
class ScopedTimer {
 public:
  explicit ScopedTimer(const std::string& operation);
  ~ScopedTimer();

  // Disable copy
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

 private:
  std::string operation_;
  std::chrono::steady_clock::time_point start_;
};

// Convenience macro for scoped timing
#define AI_SCOPED_TIMER(name) \
  ::mozc::ai::ScopedTimer _scoped_timer_##__LINE__(name)

}  // namespace ai
}  // namespace mozc

#endif  // MOZC_AI_AI_LOGGER_H_
