#include "hush/path_sandbox.hpp"
#include <cctype>

namespace hush {

std::mutex PathSandbox::mu_;
std::string PathSandbox::root_;

void PathSandbox::set_session_root(std::string root) {
  while (root.size() > 1 && (root.back() == '/' || root.back() == '\\'))
    root.pop_back();
  std::lock_guard<std::mutex> lk(mu_);
  root_ = std::move(root);
}

std::string PathSandbox::session_root() {
  std::lock_guard<std::mutex> lk(mu_);
  return root_;
}

namespace {
bool has_dotdot(const std::string& path) {
  std::size_t i = 0;
  while (i < path.size()) {
    std::size_t j = path.find_first_of("/\\", i);
    if (j == std::string::npos) j = path.size();
    if (path.substr(i, j - i) == "..") return true;
    i = j + 1;
  }
  return false;
}
bool is_absolute(const std::string& path) {
  if (path.empty()) return false;
  if (path[0] == '/' || path[0] == '\\') return true;
  if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':')
    return true;
  return false;
}
}  // namespace

bool PathSandbox::is_safe_relative(const std::string& path) {
  if (path.empty() || path.size() > 4096) return false;
  if (path.find('\0') != std::string::npos) return false;
  if (is_absolute(path)) return false;
  if (has_dotdot(path)) return false;
  return true;
}

std::optional<std::string> PathSandbox::resolve(const std::string& user_path) {
  if (user_path.empty() || user_path.size() > 4096) return std::nullopt;
  if (user_path.find('\0') != std::string::npos) return std::nullopt;
  if (has_dotdot(user_path)) return std::nullopt;

  std::string root;
  {
    std::lock_guard<std::mutex> lk(mu_);
    root = root_;
  }
  if (!root.empty()) {
    if (!is_safe_relative(user_path)) return std::nullopt;
    return root + "/" + user_path;
  }
  // Unjailed: absolute or relative OK; ".." never OK
  return user_path;
}

}  // namespace hush
