#include "hush/forensic/ForensicRack.hpp"
#include "hush/forensic/ForensicReport.hpp"
#include <cstdio>
#include <vector>
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::printf("FAIL %s\n",m);++fails;}else std::printf("PASS %s\n",m);}while(0)
int main() {
  using namespace hush::forensic;
  ForensicRack rack;
  rack.set_slot(Slot::HissReduction, {true, 0.5f, 0.f});
  std::vector<float> buf(256, 0.01f);
  rack.process(buf.data(), 128, 2);
  CHECK(rack.blocks_processed()==1, "block");

  auto legacy = analyze_buffer(buf.data(), 128, 2, 48000.0, "deadbeef");
  CHECK(!legacy.limitations.empty(), "legacy limitations");

  auto grade = build_grade_report(buf.data(), 128, 2, 48000.0, "abc123def456", "takes/a.wav");
  CHECK(grade.evidence.size() >= 2, "grade evidence");
  CHECK(!grade.limitations.empty(), "grade limitations");
  CHECK(grade.schema_version.find("FORENSIC") != std::string::npos, "schema");
  auto text = grade.to_text();
  CHECK(text.find("Limitations") != std::string::npos, "text has limitations");
  auto js = grade.to_json();
  CHECK(js.find("schema_version") != std::string::npos, "json schema");
  CHECK(js.find("\"evidence\"") != std::string::npos, "json evidence");

  std::printf("\nforensic failures: %d\n", fails);
  return fails?1:0;
}
