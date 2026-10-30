#include "hush/parameter_queue.hpp"
#include <cstdio>
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::printf("FAIL %s\n",m);++fails;}else std::printf("PASS %s\n",m);}while(0)
int main() {
  using namespace hush;
  ParameterQueue<8> q;
  ParamCommand c{1, 0.5f, 0};
  CHECK(q.try_push(c), "push");
  ParamCommand o{};
  CHECK(q.try_pop(o) && o.target==1, "pop");
  CHECK(!q.try_pop(o), "empty returns false");
  for (int i=0;i<7;i++) CHECK(q.try_push({(unsigned)i, float(i), 0}), "fill");
  CHECK(!q.try_push({99,0.f,0}), "full returns false (no block, no alloc)");
  // drain and prove command values survive
  for (int i=0;i<7;i++) {
    CHECK(q.try_pop(o) && o.target==(unsigned)i && o.value==float(i), "drain value");
  }
  CHECK(ParameterQueue<8>::usable_capacity()==7, "usable capacity Capacity-1");
  std::printf("\nparam_queue failures: %d\n", fails);
  return fails?1:0;
}
