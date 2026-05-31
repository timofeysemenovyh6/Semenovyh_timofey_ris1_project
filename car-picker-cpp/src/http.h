#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace http {

struct Request {
  std::string method;
  std::string path;
  std::string query;
  std::string http_version;
  std::map<std::string, std::string> headers;
  std::string body;
  std::map<std::string, std::string> form; // application/x-www-form-urlencoded
  std::map<std::string, std::string> cookies;
};

struct Response {
  int status = 200;
  std::string content_type = "text/html; charset=utf-8";
  std::map<std::string, std::string> headers;
  std::string body;
};

struct ServerConfig {
  std::string host = "127.0.0.1";
  uint16_t port = 8080;
  std::string static_dir = "static";
};

std::optional<Request> parse_request(const std::string& raw);
std::string build_response(const Response& res);

std::string url_decode(const std::string& s);
std::string url_encode(const std::string& s);
std::map<std::string, std::string> parse_urlencoded_form(const std::string& body);
std::map<std::string, std::string> parse_cookies(const std::string& cookie_header);
std::pair<std::string, std::string> split_path_query(const std::string& target);

bool starts_with(const std::string& s, const std::string& prefix);

using Handler = std::function<bool(const Request&, Response&)>;
int run_server(const ServerConfig& cfg, const Handler& handler);

} // namespace http

