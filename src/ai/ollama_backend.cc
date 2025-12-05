// Copyright 2024 AI Mozc IME Project
// Ollama Backend Implementation

#include "ai/ai_backend.h"

#include <sstream>
#include <chrono>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#endif

namespace mozc {
namespace ai {

namespace {

// JSON escape helper
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

// JSON unescape helper
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

// Parse endpoint URL into host and port
bool ParseEndpoint(const std::string& endpoint, std::string& host, int& port) {
  // Default values
  host = "localhost";
  port = 11434;

  std::string url = endpoint;

  // Remove protocol prefix
  if (url.find("http://") == 0) {
    url = url.substr(7);
  } else if (url.find("https://") == 0) {
    url = url.substr(8);
  }

  // Find port separator
  size_t colon_pos = url.find(':');
  size_t slash_pos = url.find('/');

  if (colon_pos != std::string::npos) {
    host = url.substr(0, colon_pos);
    size_t port_end = (slash_pos != std::string::npos) ? slash_pos : url.size();
    std::string port_str = url.substr(colon_pos + 1, port_end - colon_pos - 1);
    try {
      port = std::stoi(port_str);
    } catch (...) {
      return false;
    }
  } else if (slash_pos != std::string::npos) {
    host = url.substr(0, slash_pos);
  } else {
    host = url;
  }

  return true;
}

}  // namespace

// Ollama Backend implementation
class OllamaBackend : public AIBackendInterface {
 public:
  explicit OllamaBackend(const OllamaConfig& config)
      : config_(config), initialized_(false) {}

  bool Initialize() override {
    // Parse endpoint
    if (!ParseEndpoint(config_.endpoint, host_, port_)) {
      host_ = "localhost";
      port_ = 11434;
    }

    // Set model
    model_ = config_.model.empty() ? "mistral:7b" : config_.model;

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
      return false;
    }
#endif

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

    // Build prompt
    std::string prompt = BuildPrompt(input, existing_candidates, context);

    // Build request body
    std::string body = BuildRequestBody(prompt);

    // Send HTTP request
    std::string response;
    if (!SendHttpRequest(body, timeout_ms, response)) {
      result.error_message = "HTTP request failed or timeout";
      return result;
    }

    // Parse response
    result.candidates = ParseResponse(response);
    result.success = !result.candidates.empty();

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
  }

  std::string Name() const override {
    return "Ollama";
  }

  std::string GetConfigInfo() const override {
    return "Ollama[" + host_ + ":" + std::to_string(port_) + "/" + model_ + "]";
  }

 private:
  std::string BuildPrompt(
      const std::string& input,
      const std::vector<std::string>& existing,
      const std::vector<std::string>& context) const {

    std::ostringstream oss;
    oss << "日本語入力の変換候補を提案してください。\n\n";

    // Context (keep short)
    if (!context.empty()) {
      oss << "直前の入力: ";
      for (size_t i = 0; i < std::min(context.size(), size_t(3)); ++i) {
        if (i > 0) oss << ", ";
        oss << context[i];
      }
      oss << "\n\n";
    }

    oss << "現在の入力: " << input << "\n\n";

    // Existing candidates (max 5)
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
        << "\"prompt\":\"" << EscapeJson(prompt) << "\","
        << "\"stream\":false}";
    return oss.str();
  }

  bool SendHttpRequest(const std::string& body, int timeout_ms,
                       std::string& response) const {
#ifdef _WIN32
    return SendHttpRequestWindows(body, timeout_ms, response);
#else
    return SendHttpRequestPosix(body, timeout_ms, response);
#endif
  }

#ifdef _WIN32
  bool SendHttpRequestWindows(const std::string& body, int timeout_ms,
                              std::string& response) const {
    // Open session
    HINTERNET hSession = WinHttpOpen(
        L"MozcAI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!hSession) {
      return false;
    }

    // Set timeouts (Critical for freeze prevention)
    WinHttpSetTimeouts(hSession,
        0,            // DNS resolve timeout
        50,           // Connection timeout (50ms - very short!)
        timeout_ms,   // Send timeout
        timeout_ms);  // Receive timeout

    // Convert host to wide string
    std::wstring whost(host_.begin(), host_.end());

    // Connect
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        whost.c_str(),
        static_cast<INTERNET_PORT>(port_),
        0);

    if (!hConnect) {
      WinHttpCloseHandle(hSession);
      return false;
    }

    // Open request
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        L"/api/generate",
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);

    if (!hRequest) {
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    // Add headers
    WinHttpAddRequestHeaders(
        hRequest,
        L"Content-Type: application/json",
        -1,
        WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
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

    // Receive response
    result = WinHttpReceiveResponse(hRequest, NULL);
    if (!result) {
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    // Read response data
    response.clear();
    DWORD bytesRead = 0;
    char buffer[4096];

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead)) {
      if (bytesRead == 0) break;
      response.append(buffer, bytesRead);
    }

    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return !response.empty();
  }
#else
  bool SendHttpRequestPosix(const std::string& body, int timeout_ms,
                            std::string& response) const {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      return false;
    }

    // Set non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Resolve host
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
      struct hostent* he = gethostbyname(host_.c_str());
      if (!he) {
        close(sock);
        return false;
      }
      std::memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    // Connect (non-blocking)
    int conn_result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (conn_result < 0 && errno != EINPROGRESS) {
      close(sock);
      return false;
    }

    // Wait for connection with timeout (50ms for connection)
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLOUT;

    int poll_result = poll(&pfd, 1, 50);  // 50ms connection timeout
    if (poll_result <= 0) {
      close(sock);
      return false;
    }

    // Check for connection error
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
    if (error != 0) {
      close(sock);
      return false;
    }

    // Build HTTP request
    std::ostringstream http_req;
    http_req << "POST /api/generate HTTP/1.1\r\n"
             << "Host: " << host_ << ":" << port_ << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    std::string request_str = http_req.str();

    // Send request
    ssize_t sent = send(sock, request_str.c_str(), request_str.size(), 0);
    if (sent < 0) {
      close(sock);
      return false;
    }

    // Read response with timeout
    response.clear();
    char buffer[4096];

    pfd.events = POLLIN;

    while (true) {
      int poll_res = poll(&pfd, 1, timeout_ms);
      if (poll_res <= 0) {
        break;  // Timeout or error
      }

      ssize_t bytes = recv(sock, buffer, sizeof(buffer), 0);
      if (bytes <= 0) {
        break;
      }

      response.append(buffer, bytes);
    }

    close(sock);

    // Extract body from HTTP response
    size_t body_start = response.find("\r\n\r\n");
    if (body_start != std::string::npos) {
      response = response.substr(body_start + 4);
    }

    return !response.empty();
  }
#endif

  std::vector<std::string> ParseResponse(const std::string& response) const {
    std::vector<std::string> candidates;

    // Find "response":"..." in JSON
    size_t start = response.find("\"response\":\"");
    if (start == std::string::npos) {
      return candidates;
    }

    start += 12;  // Length of "\"response\":\""

    // Find end of response string (handle escaped quotes)
    size_t end = start;
    while (end < response.size()) {
      if (response[end] == '"' && (end == start || response[end - 1] != '\\')) {
        break;
      }
      ++end;
    }

    if (end >= response.size()) {
      return candidates;
    }

    std::string content = response.substr(start, end - start);
    content = UnescapeJson(content);

    // Parse line by line
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
      // Trim whitespace
      size_t first = line.find_first_not_of(" \t\r\n");
      if (first == std::string::npos) continue;

      size_t last = line.find_last_not_of(" \t\r\n");
      line = line.substr(first, last - first + 1);

      // Remove numbering if present (e.g., "1. ", "2. ")
      if (line.size() > 2 && std::isdigit(line[0]) && line[1] == '.') {
        line = line.substr(2);
        // Trim again
        first = line.find_first_not_of(" \t");
        if (first != std::string::npos) {
          line = line.substr(first);
        }
      }

      // Validate: non-empty, reasonable length
      if (!line.empty() && line.size() <= 40) {
        candidates.push_back(line);
        if (candidates.size() >= 3) break;  // Max 3 candidates
      }
    }

    return candidates;
  }

  OllamaConfig config_;
  std::string host_;
  int port_;
  std::string model_;
  bool initialized_;
};

// Factory implementation
std::unique_ptr<AIBackendInterface> CreateOllamaBackend(const OllamaConfig& config) {
  return std::make_unique<OllamaBackend>(config);
}

std::unique_ptr<AIBackendInterface> CreateBackend(const AIConfig& config) {
  switch (config.backend_type) {
    case BackendType::OLLAMA:
      return CreateOllamaBackend(config.ollama);
    case BackendType::GROQ:
      return CreateGroqBackend(config.groq);
    case BackendType::DISABLED:
    default:
      return nullptr;
  }
}

}  // namespace ai
}  // namespace mozc
