#pragma once
//==============================================================================
// SecurityHardening.h
//
// Runtime + build-time security posture utilities for DawChecker Secure.
//
// NOTE ON NAMING: the request that produced this module used the term "CMOS
// hardening." Actual CMOS/BIOS/UEFI firmware hardening (Secure Boot, TPM
// attestation, firmware passwords, etc.) is configured at the OS/firmware
// level and is out of scope for an application binary — an app cannot safely
// or meaningfully modify CMOS/UEFI settings, and any tool that tried to would
// itself be a serious security red flag. What this module *does* provide is
// application-level security hardening, which is the meaningful analogue for
// a desktop program: compiler/linker mitigations (see CMakeLists.txt),
// runtime self-integrity checks, secure memory handling, and safe-arithmetic
// helpers. See docs/HARDENING.md for the full mapping.
//==============================================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dawchecker::security {

// Result of the startup self-check sweep.
struct HardeningReport
{
    bool aslrActive = false;
    bool dataExecutionPreventionActive = false;
    bool stackProtectorPresumed = false;   // FORK: from compiler macro (__SSP*__) for this build; false if not compiled in
    bool controlFlowGuardOrCET = false;    // FORK: from __CET__ (-fcf-protection); false if not compiled in
    bool debuggerDetected = false;         // informational only — never used to alter behavior
    bool runningElevated = false;          // informational — flagged, not blocked
    std::optional<std::string> binarySha256; // self-hash if a companion .sha256 manifest exists
    std::vector<std::string> notes;
};

// Performs a best-effort, read-only sweep of the current process/OS security
// posture. This never disables itself, never phones home, and never takes
// evasive action — it only reports.
HardeningReport runStartupSelfCheck();

// Securely zero a memory region in a way that will not be elided by the
// optimizer (important for anything that touches raw audio buffers derived
// from potentially sensitive recordings).
void secureZero(void* data, std::size_t sizeBytes);

// RAII wrapper that secure-zeroes a buffer on scope exit. Use for any
// short-lived buffer holding audio samples pulled from a file/device that the
// user wants scrubbed of forensic watermarks.
class ScopedSecureBuffer
{
public:
    explicit ScopedSecureBuffer(std::size_t sizeBytes);
    ~ScopedSecureBuffer();

    ScopedSecureBuffer(const ScopedSecureBuffer&)            = delete;
    ScopedSecureBuffer& operator=(const ScopedSecureBuffer&) = delete;

    std::uint8_t* data() noexcept { return buffer_.data(); }
    std::size_t   size() const noexcept { return buffer_.size(); }

private:
    std::vector<std::uint8_t> buffer_;
};

// Checked arithmetic helpers — every buffer-size / sample-count calculation
// in the DSP path should go through these instead of raw +/-/* to avoid
// silent integer overflow (the classic root cause behind most audio-plugin
// CVEs: a crafted sample-rate/block-size combination overflowing a size_t).
std::optional<std::size_t> checkedMul(std::size_t a, std::size_t b) noexcept;
std::optional<std::size_t> checkedAdd(std::size_t a, std::size_t b) noexcept;

// Bounds-checked accessor used throughout the DSP modules in place of raw
// pointer indexing on externally-supplied buffers (plugin host buffers,
// file-loaded samples, network-received preset data, etc.).
template <typename T>
inline bool inBounds(std::size_t index, std::size_t containerSize) noexcept
{
    return index < containerSize;
}

// Validates that a file path does not attempt directory traversal before the
// app writes a cleaned/neutralized export next to a user-selected file.
bool isSafeExportPath(const std::string& path);

} // namespace dawchecker::security
