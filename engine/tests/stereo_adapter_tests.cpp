#include "hush/stereo_adapter.hpp"
#include <cstdio>
#include <vector>
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::printf("FAIL %s\n",m);++fails;}else std::printf("PASS %s\n",m);}while(0)
int main() {
  using namespace hush;
  StereoScratch s;
  s.configure(4);
  float inter[] = {1,2, 3,4, 5,6, 7,8};
  CHECK(s.deinterleave(inter, 4), "deint");
  CHECK(s.left()[0]==1.f && s.right()[0]==2.f, "L0 R0");
  float out[8]{};
  CHECK(s.interleave(out, 4), "int");
  CHECK(out[0]==1.f && out[1]==2.f && out[6]==7.f, "roundtrip");
  CHECK(!s.deinterleave(inter, 8), "cap");
  std::printf("\nstereo failures: %d\n", fails);
  return fails?1:0;
}
