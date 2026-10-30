#pragma once
#include <mutex>
#include <optional>
#include <string>

namespace hush {

// Session-scoped path jail for take/WAV paths. Always rejects "..".
// Optional session root: relative paths resolve under root only.
class PathSandbox {
public:
  static void set_session_root(std::string root);
  static std::string session_root();
  static bool is_safe_relative(const std::string& path);
  static std::optional<std::string> resolve(const std::string& user_path);

private:
  static std::mutex mu_;
  static std::string root_;
};

}  // namespace hush
