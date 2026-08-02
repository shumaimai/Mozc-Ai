// Copyright 2024 AI Mozc IME Project
// OpenAI-Compatible Backend Implementation (DeepSeek, Groq API, OpenAI, etc.)

#include "ai_backend.h"
#include "ai_config.h"

#include <sstream>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#define AI_LOG(msg) std::cerr << "[AI-Mozc OpenAI] " << msg << std::endl

namespace mozc {
namespace ai {

namespace {

std::string EscapeJson(const std::string& s) {
  std::string result;
  result.reserve(s.size() * 2);
  for (char c : s) {
    switch (c) {
      case '"': result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += c;
    }
  }
  return result;
}

std::string UnescapeJson(const std::string& s) {
  std::string result;
  result.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      switch (s[i + 1]) {
        case 'n': result += '\n'; ++i; break;
        case 'r': result += '\r'; ++i; break;
        case 't': result += '\t'; ++i; break;
        case '"': result += '"'; ++i; break;
        case '\\': result += '\\'; ++i; break;
        default: result += s[i];
      }
    } else {
      result += s[i];
    }
  }
  return result;
}

struct ParsedEndpoint {
  std::string host;
  int port = 443;
  std::string path = "/v1/chat/completions";
  bool use_ssl = true;
};

bool ParseApiEndpoint(const std::string& endpoint, ParsedEndpoint& parsed) {
  std::string url = endpoint;
  parsed.use_ssl = true;
  parsed.port = 443;

  if (url.find("https://") == 0) {
    url = url.substr(8);
    parsed.use_ssl = true;
  } else if (url.find("http://") == 0) {
    url = url.substr(7);
    parsed.use_ssl = false;
    parsed.port = 80;
  }

  size_t slash_pos = url.find('/');
  std::string host_port = (slash_pos != std::string::npos)
                              ? url.substr(0, slash_pos)
                              : url;
  std::string base_path = (slash_pos != std::string::npos)
                              ? url.substr(slash_pos)
                              : "";

  size_t colon_pos = host_port.find(':');
  if (colon_pos != std::string::npos) {
    parsed.host = host_port.substr(0, colon_pos);
    try {
      parsed.port = std::stoi(host_port.substr(colon_pos + 1));
    } catch (...) {
      return false;
    }
  } else {
    parsed.host = host_port;
  }

  if (parsed.host.empty()) {
    return false;
  }

  if (base_path.empty()) {
    parsed.path = "/v1/chat/completions";
  } else {
    if (base_path.back() == '/') {
      base_path.pop_back();
    }
    parsed.path = base_path + "/chat/completions";
  }

  return true;
}

std::vector<std::string> ParseCandidatesFromContent(const std::string& content) {
  std::vector<std::string> candidates;
  std::istringstream iss(content);
  std::string line;

  while (std::getline(iss, line)) {
    size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) continue;

    size_t last = line.find_last_not_of(" \t\r\n");
    line = line.substr(first, last - first + 1);

    if (line.size() > 2 && std::isdigit(static_cast<unsigned char>(line[0])) &&
        line[1] == '.') {
      line = line.substr(2);
      first = line.find_first_not_of(" \t");
      if (first != std::string::npos) {
        line = line.substr(first);
      }
    }

    if (!line.empty() && line.size() <= 40) {
      candidates.push_back(line);
      if (candidates.size() >= 3) break;
    }
  }

  return candidates;
}

}  // namespace

class OpenAICompatibleBackend : public AIBackendInterface {
 public:
  explicit OpenAICompatibleBackend(const OpenAICompatibleConfig& config)
      : config_(config) {}

  bool Initialize() override {
    if (!ParseApiEndpoint(config_.endpoint, endpoint_)) {
      AI_LOG("Failed to parse API endpoint: " + config_.endpoint);
      return false;
    }

    model_ = config_.model.empty() ? "deepseek-chat" : config_.model;

    const char* api_key = std::getenv(config_.api_key_env.c_str());
    if (!api_key || api_key[0] == '\0') {
      AI_LOG("API key not found in environment variable: " + config_.api_key_env);
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
      const std::string& input,
      const std::vector<std::string>& existing_candidates,
      const std::vector<std::string>& context,
      int timeout_ms) override {

    AIGenerationResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (!initialized_) {
      result.error_message = "Backend not initialized";
      return result;
    }

    std::string prompt = BuildPrompt(input, existing_candidates, context);
    std::string body = BuildRequestBody(prompt);

    std::string response;
    if (!SendHttpRequest(body, timeout_ms, response)) {
      result.error_message = "HTTP request failed or timeout";
      return result;
    }

    result.candidates = ParseResponse(response);
    result.success = !result.candidates.empty();
    if (!result.success) {
      result.error_message = "No candidates in API response";
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
  }

  std::string Name() const override {
    return "OpenAI-Compatible";
  }

  std::string GetConfigInfo() const override {
    return "OpenAI[" + endpoint_.host + ":" + std::to_string(endpoint_.port) +
           endpoint_.path + "/" + model_ + "]";
  }

 private:
  std::string BuildPrompt(
      const std::string& input,
      const std::vector<std::string>& existing,
      const std::vector<std::string>& context) const {

    std::ostringstream oss;
    oss << "日本語入力の変換候補を提案してください。\n\n";

    if (!context.empty()) {
      oss << "直前の入力: ";
      for (size_t i = 0; i < std::min(context.size(), size_t(3)); ++i) {
        if (i > 0) oss << ", ";
        oss << context[i];
      }
      oss << "\n\n";
    }

    oss << "現在の入力: " << input << "\n\n";

    if (!existing.empty()) {
      oss << "既存候補（これら以外を提案）: ";
      for (size_t i = 0; i < std::min(existing.size(), size_t(5)); ++i) {
        if (i > 0) oss << ", ";
        oss << existing[i];
      }
      oss << "\n\n";
    }

    oss << "3つの候補を改行区切りで出力（説明不要）:\n";
    return oss.str();
  }

  std::string BuildRequestBody(const std::string& prompt) const {
    std::ostringstream oss;
    oss << "{\"model\":\"" << model_ << "\","
        << "\"messages\":[{\"role\":\"user\",\"content\":\""
        << EscapeJson(prompt) << "\"}],"
        << "\"stream\":false,"
        << "\"temperature\":0.7,"
        << "\"max_tokens\":100}";
    return oss.str();
  }

  bool SendHttpRequest(const std::string& body, int timeout_ms,
                       std::string& response) const {
#ifdef _WIN32
    return SendHttpRequestWindows(body, timeout_ms, response);
#else
    (void)body;
    (void)timeout_ms;
    (void)response;
    AI_LOG("OpenAI-compatible backend requires Windows (WinHTTP HTTPS)");
    return false;
#endif
  }

#ifdef _WIN32
  bool SendHttpRequestWindows(const std::string& body, int timeout_ms,
                              std::string& response) const {
    HINTERNET hSession = WinHttpOpen(
        L"MozcAI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!hSession) {
      return false;
    }

    WinHttpSetTimeouts(hSession, 0, 50, timeout_ms, timeout_ms);

    std::wstring whost(endpoint_.host.begin(), endpoint_.host.end());

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        whost.c_str(),
        static_cast<INTERNET_PORT>(endpoint_.port),
        0);

    if (!hConnect) {
      WinHttpCloseHandle(hSession);
      return false;
    }

    std::wstring wpath(endpoint_.path.begin(), endpoint_.path.end());
    DWORD flags = endpoint_.use_ssl ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        wpath.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);

    if (!hRequest) {
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    std::string auth_header = "Authorization: Bearer " + api_key_;
    std::wstring wauth(auth_header.begin(), auth_header.end());

    WinHttpAddRequestHeaders(
        hRequest,
        L"Content-Type: application/json",
        -1,
        WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(
        hRequest,
        wauth.c_str(),
        static_cast<DWORD>(-1),
        WINHTTP_ADDREQ_FLAG_ADD);

    BOOL result = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        (LPVOID)body.c_str(),
        static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()),
        0);

    if (!result) {
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    result = WinHttpReceiveResponse(hRequest, NULL);
    if (!result) {
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    response.clear();
    DWORD bytesRead = 0;
    char buffer[4096];

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead)) {
      if (bytesRead == 0) break;
      response.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return !response.empty();
  }
#endif

  std::vector<std::string> ParseResponse(const std::string& response) const {
    size_t start = response.find("\"content\":\"");
    if (start == std::string::npos) {
      start = response.find("\"content\": \"");
      if (start == std::string::npos) {
        return {};
      }
      start += 12;
    } else {
      start += 11;
    }

    size_t end = start;
    while (end < response.size()) {
      if (response[end] == '"' && (end == start || response[end - 1] != '\\')) {
        break;
      }
      ++end;
    }

    if (end >= response.size()) {
      return {};
    }

    std::string content = UnescapeJson(response.substr(start, end - start));
    return ParseCandidatesFromContent(content);
  }

  OpenAICompatibleConfig config_;
  ParsedEndpoint endpoint_;
  std::string model_;
  std::string api_key_;
  bool initialized_ = false;
};

std::unique_ptr<AIBackendInterface> CreateOpenAICompatibleBackend(
    const OpenAICompatibleConfig& config) {
  return std::make_unique<OpenAICompatibleBackend>(config);
}

}  // namespace ai
}  // namespace mozc
