#include "hush/security/DawChecker.hpp"
#include <cstdio>
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::printf("FAIL %s\n",m);++fails;}else std::printf("PASS %s\n",m);}while(0)
int main() {
  using namespace hush::security;
  DawChecker c;
  c.add_banned_ip("203.0.113.10");
  c.add_banned_ip("not an ip");  // ignored
  CHECK(c.is_banned("203.0.113.10"), "banned");
  CHECK(!c.is_banned("198.51.100.1"), "not banned");
  CHECK(c.banned_list().size()==1, "list size");
  auto h = c.hardening_report();
  CHECK(h.path_sandbox && h.offline_cmake, "hardening flags");
  std::printf("\ndawchecker failures: %d\n", fails);
  return fails?1:0;
}
