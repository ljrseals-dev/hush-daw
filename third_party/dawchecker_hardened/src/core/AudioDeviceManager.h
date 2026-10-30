#pragma once
#include <string>
#include <vector>

namespace dawchecker::audio {

struct AudioDeviceInfo
{
    std::string name;          // driver/device friendly name as reported by the OS
    std::string hostApi;       // "WASAPI", "ASIO", "CoreAudio", "ALSA", "PortAudio(other)"
    bool        isDefaultInput  = false;
    bool        isDefaultOutput = false;
    int         maxInputChannels  = 0;
    int         maxOutputChannels = 0;
    double      defaultSampleRate = 0.0;
};

// Cross-platform device/driver enumeration. Uses PortAudio when the build was
// configured with DAWCHECKER_HAVE_PORTAUDIO; otherwise falls back to a
// minimal platform-specific listing (WASAPI endpoint enumeration on Windows,
// ALSA hint enumeration on Linux) so the codec scanner still has something
// real to inspect.
class AudioDeviceManager
{
public:
    AudioDeviceManager();
    ~AudioDeviceManager();

    // Re-scans the system for currently available audio devices/drivers.
    // Returns false if enumeration failed outright (e.g. no audio subsystem
    // available at all); an empty-but-successful scan still returns true.
    bool refresh();

    const std::vector<AudioDeviceInfo>& devices() const noexcept { return devices_; }

private:
    std::vector<AudioDeviceInfo> devices_;
    bool backendInitialized_ = false;
};

} // namespace dawchecker::audio
