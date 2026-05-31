#include "http.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

namespace http {

static std::string to_lower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  return s;
}

bool starts_with(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::pair<std::string, std::string> split_path_query(const std::string& target) {
  auto pos = target.find('?');
  if (pos == std::string::npos) return {target, ""};
  return {target.substr(0, pos), target.substr(pos + 1)};
}

std::string url_decode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '+') {
      out.push_back(' ');
    } else if (c == '%' && i + 2 < s.size()) {
      int v = 0;
      for (int k = 0; k < 2; k++) {
        char h = s[i + 1 + k];
        v *= 16;
        if (h >= '0' && h <= '9') v += (h - '0');
        else if (h >= 'a' && h <= 'f') v += 10 + (h - 'a');
        else if (h >= 'A' && h <= 'F') v += 10 + (h - 'A');
      }
      out.push_back(char(v));
      i += 2;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string url_encode(const std::string& s) {
  std::string out;
  out.reserve(s.size() * 3);
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      out.push_back(static_cast<char>(c));
    } else if (c == ' ') {
      out.push_back('+');
    } else {
      char buf[8];
      std::snprintf(buf, sizeof buf, "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

std::map<std::string, std::string> parse_urlencoded_form(const std::string& body) {
  std::map<std::string, std::string> out;
  size_t start = 0;
  while (start < body.size()) {
    size_t amp = body.find('&', start);
    if (amp == std::string::npos) amp = body.size();
    std::string pair = body.substr(start, amp - start);
    size_t eq = pair.find('=');
    std::string k = (eq == std::string::npos) ? pair : pair.substr(0, eq);
    std::string v = (eq == std::string::npos) ? "" : pair.substr(eq + 1);
    out[url_decode(k)] = url_decode(v);
    start = amp + 1;
  }
  return out;
}

std::map<std::string, std::string> parse_cookies(const std::string& cookie_header) {
  std::map<std::string, std::string> out;
  size_t start = 0;
  while (start < cookie_header.size()) {
    size_t semi = cookie_header.find(';', start);
    if (semi == std::string::npos) semi = cookie_header.size();
    std::string part = cookie_header.substr(start, semi - start);
    while (!part.empty() && part.front() == ' ') part.erase(part.begin());
    size_t eq = part.find('=');
    if (eq != std::string::npos) out[part.substr(0, eq)] = part.substr(eq + 1);
    start = semi + 1;
  }
  return out;
}

static std::optional<int> parse_int(const std::string& s) {
  int v = 0;
  auto* b = s.data();
  auto* e = s.data() + s.size();
  auto [ptr, ec] = std::from_chars(b, e, v);
  if (ec != std::errc() || ptr != e) return std::nullopt;
  return v;
}

std::optional<Request> parse_request(const std::string& raw) {
  auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) return std::nullopt;

  Request req;
  std::string head = raw.substr(0, header_end);
  req.body = raw.substr(header_end + 4);

  std::istringstream hs(head);
  std::string request_line;
  if (!std::getline(hs, request_line)) return std::nullopt;
  if (!request_line.empty() && request_line.back() == '\r') request_line.pop_back();

  {
    std::istringstream rl(request_line);
    std::string target;
    rl >> req.method >> target >> req.http_version;
    auto [p, q] = split_path_query(target);
    req.path = p;
    req.query = q;
  }

  std::string line;
  while (std::getline(hs, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto pos = line.find(':');
    if (pos == std::string::npos) continue;
    std::string key = to_lower(line.substr(0, pos));
    std::string val = line.substr(pos + 1);
    while (!val.empty() && val.front() == ' ') val.erase(val.begin());
    req.headers[key] = val;
  }

  if (auto it = req.headers.find("cookie"); it != req.headers.end()) req.cookies = parse_cookies(it->second);

  if (req.method == "POST" || req.method == "PUT") {
    bool as_form = false;
    if (auto it = req.headers.find("content-type"); it != req.headers.end()) {
      std::string ctl = to_lower(it->second);
      if (starts_with(ctl, "application/x-www-form-urlencoded")) as_form = true;
    } else if (!req.body.empty() && req.body.find('=') != std::string::npos) {
      as_form = true;
    }
    if (as_form) req.form = parse_urlencoded_form(req.body);
  }
  return req;
}

static std::string status_text(int status) {
  switch (status) {
    case 200: return "OK";
    case 302: return "Found";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default: return "OK";
  }
}

std::string build_response(const Response& res) {
  std::ostringstream out;
  out << "HTTP/1.1 " << res.status << " " << status_text(res.status) << "\r\n";
  out << "Content-Type: " << res.content_type << "\r\n";
  for (const auto& [k, v] : res.headers) out << k << ": " << v << "\r\n";
  out << "Content-Length: " << res.body.size() << "\r\n";
  out << "Connection: close\r\n\r\n";
  out << res.body;
  return out.str();
}

static bool read_file(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

static std::string guess_mime(const std::string& path) {
  auto dot = path.find_last_of('.');
  std::string ext = (dot == std::string::npos) ? "" : to_lower(path.substr(dot + 1));
  if (ext == "css") return "text/css; charset=utf-8";
  if (ext == "js") return "text/javascript; charset=utf-8";
  if (ext == "png") return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "svg") return "image/svg+xml";
  return "application/octet-stream";
}

static bool safe_path(const std::string& p) {
  return p.find("..") == std::string::npos && p.find('\\') == std::string::npos;
}

static bool try_static(const ServerConfig& cfg, const Request& req, Response& res) {
  if (req.method != "GET") return false;
  if (!starts_with(req.path, "/static/")) return false;
  if (!safe_path(req.path)) return false;
  std::string disk = cfg.static_dir + req.path.substr(std::string("/static").size());
  std::string data;
  if (!read_file(disk, data)) return false;
  res.status = 200;
  res.content_type = guess_mime(disk);
  res.body = std::move(data);
  return true;
}

static bool recv_all(int fd, std::string& out) {
  out.clear();
  char buf[4096];
  while (true) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n == 0) return !out.empty();
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    out.append(buf, buf + n);
    if (out.find("\r\n\r\n") != std::string::npos) break;
    if (out.size() > 1024 * 1024) return false;
  }

  auto header_end = out.find("\r\n\r\n");
  std::string head = out.substr(0, header_end);
  size_t content_len = 0;

  std::istringstream hs(head);
  std::string line;
  std::getline(hs, line);
  while (std::getline(hs, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto pos = line.find(':');
    if (pos == std::string::npos) continue;
    std::string key = to_lower(line.substr(0, pos));
    std::string val = line.substr(pos + 1);
    while (!val.empty() && val.front() == ' ') val.erase(val.begin());
    if (key == "content-length") {
      if (auto v = parse_int(val)) content_len = size_t(*v);
    }
  }

  size_t have_body = out.size() - (header_end + 4);
  while (have_body < content_len) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) return false;
    out.append(buf, buf + n);
    have_body += size_t(n);
    if (out.size() > 5 * 1024 * 1024) return false;
  }
  // Первый recv часто приносит больше байт тела, чем в Content-Length — иначе парсятся «хвосты» и ломается POST (чат).
  if (content_len > 0) {
    const size_t total = header_end + 4 + content_len;
    if (out.size() > total) out.resize(total);
  }
  return true;
}

int run_server(const ServerConfig& cfg, const Handler& handler) {
  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) return 1;

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg.port);
  if (::inet_pton(AF_INET, cfg.host.c_str(), &addr.sin_addr) != 1) {
    ::close(server_fd);
    return 2;
  }

  if (::bind(server_fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
    std::cerr << "bind(" << cfg.host << ":" << cfg.port << ") failed: " << std::strerror(errno) << "\n";
    ::close(server_fd);
    return 3;
  }
  if (::listen(server_fd, 32) != 0) {
    std::cerr << "listen() failed: " << std::strerror(errno) << "\n";
    ::close(server_fd);
    return 4;
  }

  while (true) {
    int client = ::accept(server_fd, nullptr, nullptr);
    if (client < 0) continue;

    Response res;
    std::string raw;
    if (!recv_all(client, raw)) {
      res.status = 400;
      res.content_type = "text/plain; charset=utf-8";
      res.body = "Bad Request";
      auto bytes = build_response(res);
      ::send(client, bytes.data(), bytes.size(), 0);
      ::close(client);
      continue;
    }

    auto req_opt = parse_request(raw);
    if (!req_opt) {
      res.status = 400;
      res.content_type = "text/plain; charset=utf-8";
      res.body = "Bad Request";
      auto bytes = build_response(res);
      ::send(client, bytes.data(), bytes.size(), 0);
      ::close(client);
      continue;
    }

    Request req = std::move(*req_opt);
    bool handled = false;
    try {
      handled = handler(req, res);
      if (!handled) handled = try_static(cfg, req, res);
    } catch (...) {
      res.status = 500;
      res.content_type = "text/plain; charset=utf-8";
      res.body = "Internal Server Error";
      handled = true;
    }

    if (!handled) {
      res.status = 404;
      res.content_type = "text/plain; charset=utf-8";
      res.body = "Not Found";
    }

    auto bytes = build_response(res);
    ::send(client, bytes.data(), bytes.size(), 0);
    ::close(client);
  }
}

} // namespace http

