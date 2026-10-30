#include "CodecScanner.h"
#include <algorithm>
#include <cctype>

namespace dawchecker::codec {

namespace {

std::string toLower(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

struct KnownEntry
{
    std::string   matchSubstring; // lower-case substring to match against device name
    std::string   canonicalName;
    RiskLevel     risk;
    std::string   note;
};

// Small, illustrative known-codec/driver database. Entries are matched by
// case-insensitive substring against the OS-reported device/driver name.
// This list is intentionally conservative: ordinary, mainstream drivers are
// Info-level ("recognized, nothing to flag"); the two Alert entries are the
// ones this feature was specifically built to surface.
const std::vector<KnownEntry>& knownDatabase()
{
    static const std::vector<KnownEntry> db = {
        // --- Ordinary / mainstream — Info ---
        {"realtek",        "Realtek HD Audio",        RiskLevel::Info,
         "Standard onboard audio codec. No action needed."},
        {"conexant",       "Conexant HD Audio",       RiskLevel::Info,
         "Standard onboard audio codec. No action needed."},
        {"cirrus logic",   "Cirrus Logic Audio",      RiskLevel::Info,
         "Standard onboard/mobile audio codec. No action needed."},
        {"asio4all",       "ASIO4ALL",                RiskLevel::Info,
         "Widely used generic ASIO wrapper driver. No action needed."},
        {"focusrite",      "Focusrite USB ASIO",      RiskLevel::Info,
         "Vendor-signed interface driver. No action needed."},
        {"steinberg",      "Steinberg/Yamaha ASIO",   RiskLevel::Info,
         "Vendor-signed interface driver. No action needed."},
        {"universal audio","Universal Audio ASIO",    RiskLevel::Info,
         "Vendor-signed interface driver. No action needed."},
        {"core audio",     "Apple Core Audio",        RiskLevel::Info,
         "Native macOS audio subsystem. No action needed."},
        {"wasapi",         "Windows Audio (WASAPI)",  RiskLevel::Info,
         "Native Windows audio subsystem. No action needed."},

        // --- Uncommon/legacy — Caution ---
        {"via hd audio",   "VIA HD Audio",            RiskLevel::Caution,
         "Older codec family; confirm the driver came from the OEM/motherboard vendor, not a bundled installer."},
        {"c-media",        "C-Media Audio",           RiskLevel::Caution,
         "Frequently rebranded/white-labeled; verify the installer source if this appeared unexpectedly."},

        // --- Flagged patterns — Alert ---
        // "Crystal Codec" / "Crystal ASIO" have repeatedly turned up bundled
        // with unrelated, unsigned installers (keygens, cracked plugin
        // "activators", etc.) rather than shipped by a legitimate audio
        // hardware or interface vendor. A driver with system-level audio
        // access arriving via that path is a meaningful red flag: it can
        // sit in the audio pipeline unnoticed, and unlike a mainstream
        // vendor driver, "Crystal" naming has no single accountable vendor
        // behind it to verify against. DawChecker treats any device whose
        // name contains this pattern as a high-priority alert regardless of
        // detection sensitivity settings.
        {"crystal asio",   "Crystal ASIO",            RiskLevel::Alert,
         "Unverified driver commonly bundled with unofficial/cracked installers rather than distributed by an "
         "accountable audio hardware vendor. Recommended: confirm where this driver came from, and remove it if "
         "you don't recognize the install source."},
        {"crystal codec",  "Crystal Codec",           RiskLevel::Alert,
         "Unverified codec commonly bundled with unofficial/cracked installers rather than distributed by an "
         "accountable audio hardware vendor. Recommended: confirm where this driver came from, and remove it if "
         "you don't recognize the install source."},
    };
    return db;
}

} // namespace

CodecFinding CodecScanner::classify(const audio::AudioDeviceInfo& device) const
{
    CodecFinding finding;
    finding.deviceName = device.name;
    finding.hostApi    = device.hostApi;

    const std::string lowerName = toLower(device.name);

    for (const auto& entry : knownDatabase())
    {
        if (lowerName.find(entry.matchSubstring) != std::string::npos)
        {
            finding.matchedCodec = entry.canonicalName;
            finding.risk         = entry.risk;
            finding.note         = entry.note;
            return finding;
        }
    }

    finding.matchedCodec = "Unrecognized driver";
    finding.risk         = RiskLevel::Caution;
    finding.note         = "Not present in the known-codec database. Not necessarily unsafe — just unverified. "
                            "Consider checking the vendor/publisher if you're unsure why it's installed.";
    return finding;
}

std::vector<CodecFinding> CodecScanner::scan()
{
    std::vector<CodecFinding> findings;
    hasCriticalAlert_ = false;

    if (!deviceManager_.refresh())
        return findings;

    findings.reserve(deviceManager_.devices().size());
    for (const auto& device : deviceManager_.devices())
    {
        CodecFinding finding = classify(device);
        if (finding.risk == RiskLevel::Alert)
            hasCriticalAlert_ = true;
        findings.push_back(std::move(finding));
    }
    return findings;
}

} // namespace dawchecker::codec
