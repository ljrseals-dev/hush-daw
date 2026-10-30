#include "hush/path_sandbox.hpp"
#include <cstdio>
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::printf("FAIL %s\n",m);++fails;}else std::printf("PASS %s\n",m);}while(0)
int main() {
  using namespace hush;
  PathSandbox::set_session_root("/tmp/hush-session");
  CHECK(!PathSandbox::is_safe_relative("../x"), "reject dotdot");
  CHECK(!PathSandbox::resolve("../etc/passwd").has_value(), "resolve reject");
  CHECK(!PathSandbox::resolve("/abs").has_value(), "reject abs under jail");
  auto r = PathSandbox::resolve("takes/a.wav");
  CHECK(r && *r == "/tmp/hush-session/takes/a.wav", "jail join");
  PathSandbox::set_session_root("");
  CHECK(PathSandbox::resolve("/tmp/x").has_value(), "unjailed abs ok");
  CHECK(!PathSandbox::resolve("a/../b").has_value(), "dotdot always bad");
  std::printf("\nsandbox failures: %d\n", fails);
  return fails?1:0;
}
