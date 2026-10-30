#include "hush/security/DawChecker.hpp"
#include <cctype>

namespace hush::security {

void DawChecker::add_banned_ip(std::string ip) {
  if (!looks_like_ip(ip)) return;
  std::lock_guard<std::mutex> lk(mu_);
  banned_.insert(std::move(ip));
}

void DawChecker::remove_banned_ip(const std::string& ip) {
  std::lock_guard<std::mutex> lk(mu_);
  banned_.erase(ip);
}

bool DawChecker::is_banned(const std::string& ip) const {
  std::lock_guard<std::mutex> lk(mu_);
  return banned_.count(ip) > 0;
}

std::vector<std::string> DawChecker::banned_list() const {
  std::lock_guard<std::mutex> lk(mu_);
  return {banned_.begin(), banned_.end()};
}

void DawChecker::clear_banned() {
  std::lock_guard<std::mutex> lk(mu_);
  banned_.clear();
}

bool DawChecker::looks_like_ip(const std::string& s) {
  if (s.empty() || s.size() > 45) return false;
  // reject path traversal / spaces
  for (char c : s) {
    if (c == ' ' || c == '/' || c == '\\' || c == '\0') return false;
  }
  bool has_dot = s.find('.') != std::string::npos;
  bool has_colon = s.find(':') != std::string::npos;
  return has_dot || has_colon;
}

HardeningReport DawChecker::hardening_report() const {
  HardeningReport r;
  r.path_sandbox = true;
  r.offline_cmake = true;
  r.privacy_denylist = true;
#if defined(__SSP__) || defined(__SSP_ALL__) || defined(__SSP_STRONG__)
  r.stack_protector = true;
#endif
#if defined(__PIE__) || defined(__pie__)
  r.pie = true;
#endif
  r.notes = "DawChecker is local-only; no network bind by default. "
            "Vendored dawchecker_hardened detectors are offline indicators.";
  return r;
}

}  // namespace hush::security
