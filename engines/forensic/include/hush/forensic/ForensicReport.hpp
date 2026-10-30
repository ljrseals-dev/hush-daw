#pragma once
// Forensic-grade reporting: structured, exportable, limitation-first.
// Indicators only — never proof of authenticity, authorship, or tampering.
#include "hush/forensic/ForensicRack.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace hush::forensic {

enum class Severity : std::uint8_t { Info = 0, Active, Alert };

struct EvidenceItem {
  std::string module_id;       // stable id e.g. "ultrasonic", "level", "enf"
  std::string module_name;
  Severity severity{Severity::Info};
  std::string status;          // Idle / Active / Alert
  std::string summary;
  double confidence{0};        // 0..100 uncalibrated
  double location_hz{0};
  double location_sec{0};      // optional time anchor
  bool applicable{true};
  std::string methodology;     // short method note
  std::string note;
};

struct ChainOfCustodyNote {
  std::string timestamp_iso;   // caller-supplied; engine does not touch clock in RT
  std::string actor;           // e.g. "local-analyst"
  std::string action;          // e.g. "analyze_buffer"
  std::string detail;
};

struct ForensicGradeReport {
  std::string report_id;               // caller or derived from hash+seq
  std::string schema_version{"HUSH-FORENSIC-REPORT-1"};
  std::string analysis_version{"hush-forensic-2.2.1"};
  std::string file_sha256;
  std::string source_path_sanitized;   // path after PathSandbox resolve; may be empty
  double sample_rate{0};
  std::uint64_t frames{0};
  std::uint32_t channels{0};

  std::vector<EvidenceItem> evidence;
  std::vector<ChainOfCustodyNote> chain;
  std::vector<std::string> limitations;
  std::vector<std::string> hardening_notes;

  // Always populated by builders
  void ensure_standard_limitations();

  [[nodiscard]] std::string to_text() const;
  [[nodiscard]] std::string to_json() const;  // minimal JSON, no third-party lib
};

// Build a grade report from PCM (control thread / offline only).
ForensicGradeReport build_grade_report(const float* interleaved, std::size_t frames,
                                       std::size_t channels, double sample_rate,
                                       const std::string& file_sha256,
                                       const std::string& sanitized_path = {});

}  // namespace hush::forensic
