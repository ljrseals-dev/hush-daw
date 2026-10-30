#include "hush/forensic/ForensicReport.hpp"
#include <cmath>
#include <sstream>

namespace hush::forensic {

namespace {

std::string json_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          o += buf;
        } else {
          o += c;
        }
    }
  }
  return o;
}

const char* sev_str(Severity s) {
  switch (s) {
    case Severity::Alert: return "Alert";
    case Severity::Active: return "Active";
    default: return "Info";
  }
}

}  // namespace

void ForensicGradeReport::ensure_standard_limitations() {
  const char* std_lim[] = {
      "Indicators only — not proof of tampering, authenticity, or authorship.",
      "Confidence scores are uncalibrated and detector-specific.",
      "Analysis is local and offline; no cloud or network inference.",
      "Absence of an alert does not imply a clean or original recording.",
      "Path references are sandbox-validated; unresolved paths are omitted."};
  for (const char* l : std_lim) {
    bool found = false;
    for (const auto& x : limitations)
      if (x == l) {
        found = true;
        break;
      }
    if (!found) limitations.emplace_back(l);
  }
}

std::string ForensicGradeReport::to_text() const {
  std::ostringstream o;
  o << "======== HUSH FORENSIC-GRADE REPORT ========\n";
  o << "Schema      : " << schema_version << "\n";
  o << "Analysis    : " << analysis_version << "\n";
  o << "Report ID   : " << report_id << "\n";
  o << "File SHA-256: " << file_sha256 << "\n";
  if (!source_path_sanitized.empty())
    o << "Source path : " << source_path_sanitized << "\n";
  o << "Format      : " << sample_rate << " Hz, " << frames << " frames, " << channels
    << " ch\n\n";

  o << "--- Evidence ---\n";
  if (evidence.empty())
    o << "(none)\n";
  for (const auto& e : evidence) {
    o << "[" << sev_str(e.severity) << "] " << e.module_name << " (" << e.module_id << ")\n";
    o << "  status=" << e.status << " conf=" << e.confidence;
    if (e.location_hz > 0) o << " hz=" << e.location_hz;
    if (e.location_sec > 0) o << " t=" << e.location_sec << "s";
    o << "\n  " << e.summary << "\n";
    if (!e.methodology.empty()) o << "  method: " << e.methodology << "\n";
    if (!e.note.empty()) o << "  note: " << e.note << "\n";
    if (!e.applicable) o << "  applicable: false\n";
  }

  if (!chain.empty()) {
    o << "\n--- Chain of custody notes ---\n";
    for (const auto& c : chain) {
      o << c.timestamp_iso << " | " << c.actor << " | " << c.action;
      if (!c.detail.empty()) o << " | " << c.detail;
      o << "\n";
    }
  }

  o << "\n--- Hardening ---\n";
  for (const auto& h : hardening_notes) o << "  * " << h << "\n";

  o << "\n--- Limitations (mandatory) ---\n";
  for (const auto& l : limitations) o << "  * " << l << "\n";
  o << "============================================\n";
  return o.str();
}

std::string ForensicGradeReport::to_json() const {
  std::ostringstream o;
  o << "{\n";
  o << "  \"schema_version\": \"" << json_escape(schema_version) << "\",\n";
  o << "  \"analysis_version\": \"" << json_escape(analysis_version) << "\",\n";
  o << "  \"report_id\": \"" << json_escape(report_id) << "\",\n";
  o << "  \"file_sha256\": \"" << json_escape(file_sha256) << "\",\n";
  o << "  \"source_path_sanitized\": \"" << json_escape(source_path_sanitized) << "\",\n";
  o << "  \"sample_rate\": " << sample_rate << ",\n";
  o << "  \"frames\": " << frames << ",\n";
  o << "  \"channels\": " << channels << ",\n";
  o << "  \"evidence\": [\n";
  for (std::size_t i = 0; i < evidence.size(); ++i) {
    const auto& e = evidence[i];
    o << "    {\n";
    o << "      \"module_id\": \"" << json_escape(e.module_id) << "\",\n";
    o << "      \"module_name\": \"" << json_escape(e.module_name) << "\",\n";
    o << "      \"severity\": \"" << sev_str(e.severity) << "\",\n";
    o << "      \"status\": \"" << json_escape(e.status) << "\",\n";
    o << "      \"summary\": \"" << json_escape(e.summary) << "\",\n";
    o << "      \"confidence\": " << e.confidence << ",\n";
    o << "      \"location_hz\": " << e.location_hz << ",\n";
    o << "      \"location_sec\": " << e.location_sec << ",\n";
    o << "      \"applicable\": " << (e.applicable ? "true" : "false") << ",\n";
    o << "      \"methodology\": \"" << json_escape(e.methodology) << "\",\n";
    o << "      \"note\": \"" << json_escape(e.note) << "\"\n";
    o << "    }" << (i + 1 < evidence.size() ? "," : "") << "\n";
  }
  o << "  ],\n";
  o << "  \"limitations\": [\n";
  for (std::size_t i = 0; i < limitations.size(); ++i) {
    o << "    \"" << json_escape(limitations[i]) << "\""
      << (i + 1 < limitations.size() ? "," : "") << "\n";
  }
  o << "  ],\n";
  o << "  \"hardening_notes\": [\n";
  for (std::size_t i = 0; i < hardening_notes.size(); ++i) {
    o << "    \"" << json_escape(hardening_notes[i]) << "\""
      << (i + 1 < hardening_notes.size() ? "," : "") << "\n";
  }
  o << "  ]\n";
  o << "}\n";
  return o.str();
}

ForensicGradeReport build_grade_report(const float* interleaved, std::size_t frames,
                                       std::size_t channels, double sample_rate,
                                       const std::string& file_sha256,
                                       const std::string& sanitized_path) {
  ForensicGradeReport r;
  r.file_sha256 = file_sha256;
  r.source_path_sanitized = sanitized_path;
  r.sample_rate = sample_rate;
  r.frames = frames;
  r.channels = static_cast<std::uint32_t>(channels);
  r.report_id = file_sha256.empty() ? "local-unhashed" : (file_sha256.substr(0, 16) + "-rpt");
  r.hardening_notes = {"PathSandbox rejects '..' path segments",
                       "Offline analysis only — no network clients in report path",
                       "DawChecker ban list is local process memory only"};
  r.ensure_standard_limitations();

  if (!interleaved || frames < 16 || channels == 0 || sample_rate < 8000.0) {
    EvidenceItem e;
    e.module_id = "buffer";
    e.module_name = "Input validation";
    e.severity = Severity::Info;
    e.status = "Idle";
    e.summary = "Insufficient input for analysis";
    e.applicable = false;
    e.methodology = "preconditions: frames>=16, channels>=1, sample_rate>=8000";
    e.note = "Report still emitted with limitations";
    r.evidence.push_back(std::move(e));
    return r;
  }

  // Level survey
  double peak = 0, sum2 = 0;
  const std::size_t n = frames * channels;
  std::size_t clip_hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const double x = interleaved[i];
    const double a = x < 0 ? -x : x;
    if (a > peak) peak = a;
    if (a >= 0.99) ++clip_hits;
    sum2 += x * x;
  }
  const double rms = std::sqrt(sum2 / static_cast<double>(n));

  {
    EvidenceItem e;
    e.module_id = "level";
    e.module_name = "Level survey";
    e.severity = peak >= 0.99 ? Severity::Alert : Severity::Active;
    e.status = peak >= 0.99 ? "Alert" : "Active";
    e.summary = "Peak/RMS amplitude survey";
    e.confidence = peak * 100.0;
    e.methodology = "Full-buffer peak absolute value and RMS";
    e.note = "peak=" + std::to_string(peak) + " rms=" + std::to_string(rms) +
             " near_clip_samples=" + std::to_string(clip_hits);
    r.evidence.push_back(std::move(e));
  }

  // DC offset proxy
  double sum = 0;
  for (std::size_t i = 0; i < n; ++i) sum += interleaved[i];
  const double dc = sum / static_cast<double>(n);
  {
    EvidenceItem e;
    e.module_id = "dc";
    e.module_name = "DC offset proxy";
    e.severity = (dc > 0.02 || dc < -0.02) ? Severity::Active : Severity::Info;
    e.status = e.severity == Severity::Active ? "Active" : "Idle";
    e.summary = "Mean sample value (DC proxy)";
    e.confidence = std::min(100.0, std::abs(dc) * 500.0);
    e.methodology = "Arithmetic mean of interleaved samples";
    e.note = "dc=" + std::to_string(dc);
    r.evidence.push_back(std::move(e));
  }

  // Crest factor
  const double crest = (rms > 1e-12) ? (peak / rms) : 0.0;
  {
    EvidenceItem e;
    e.module_id = "crest";
    e.module_name = "Crest factor";
    e.severity = Severity::Info;
    e.status = "Active";
    e.summary = "Peak-to-RMS ratio";
    e.confidence = std::min(100.0, crest * 10.0);
    e.methodology = "peak / rms";
    e.note = "crest=" + std::to_string(crest);
    r.evidence.push_back(std::move(e));
  }

  return r;
}

}  // namespace hush::forensic
