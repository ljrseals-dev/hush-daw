#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "AudioDeviceManager.h"

namespace dawchecker::codec {

enum class RiskLevel : std::uint8_t
{
    Info,     // recognized, ordinary, no concern
    Caution,  // recognized but worth a look (legacy/unsigned/uncommon)
    Alert     // matches a known-risky pattern; surfaced prominently in the GUI
};

struct CodecFinding
{
    std::string deviceName;   // as reported by the OS/driver
    std::string hostApi;      // WASAPI / ASIO / CoreAudio / ALSA
    std::string matchedCodec; // canonical name from the known-codec database, if any
    RiskLevel   risk = RiskLevel::Info;
    std::string note; // human-readable rationale shown in the GUI panel
};

// Scans currently-enumerated audio devices/drivers and classifies each
// against a small known-codec/driver database. This is a *name and metadata*
// based classifier (the same class of technique used by legitimate desktop
// security tools to flag known-risky drivers) — it does not inspect,
// fingerprint, or decode proprietary codec bitstreams.
class CodecScanner
{
public:
    // Runs a fresh device enumeration and classifies every result.
    std::vector<CodecFinding> scan();

    // True if the most recent scan() call produced at least one Alert-level
    // finding (e.g. Crystal Codec / Crystal ASIO — see notes in .cpp).
    bool hasCriticalAlert() const noexcept { return hasCriticalAlert_; }

private:
    audio::AudioDeviceManager deviceManager_;
    bool hasCriticalAlert_ = false;

    CodecFinding classify(const audio::AudioDeviceInfo& device) const;
};

} // namespace dawchecker::codec
