#pragma once
// hush::security::DawChecker — midterm security console backend
// Banned IP / host denylist, local hardening report. No telemetry outbound.
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace hush::security {

struct HardeningReport {
  bool path_sandbox{true};
  bool offline_cmake{true};
  bool privacy_denylist{true};
  bool stack_protector{false};  // from compile macros when available
  bool pie{false};
  std::string notes;
};

class DawChecker {
public:
  void add_banned_ip(std::string ip);
  void remove_banned_ip(const std::string& ip);
  [[nodiscard]] bool is_banned(const std::string& ip) const;
  [[nodiscard]] std::vector<std::string> banned_list() const;
  void clear_banned();

  // Validate IP-ish string (v4 dotted or v6 compressed — syntactic only)
  static bool looks_like_ip(const std::string& s);

  HardeningReport hardening_report() const;

private:
  mutable std::mutex mu_;
  std::unordered_set<std::string> banned_;
};

}  // namespace hush::security
