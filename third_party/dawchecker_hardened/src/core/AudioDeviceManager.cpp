#include "AudioDeviceManager.h"

#if defined(DAWCHECKER_HAVE_PORTAUDIO)
    #include <portaudio.h>
#elif defined(DAWCHECKER_PLATFORM_WINDOWS)
    #include <windows.h>
    #include <mmdeviceapi.h>
    #include <functiondiscoverykeys_devpkey.h>
    #include <propvarutil.h>
    #include <wrl/client.h>
#elif defined(DAWCHECKER_HAVE_ALSA)
    #include <alsa/asoundlib.h>
#endif

namespace dawchecker::audio {

AudioDeviceManager::AudioDeviceManager()
{
#if defined(DAWCHECKER_HAVE_PORTAUDIO)
    backendInitialized_ = (Pa_Initialize() == paNoError);
#else
    backendInitialized_ = true; // platform paths below self-contain their init
#endif
}

AudioDeviceManager::~AudioDeviceManager()
{
#if defined(DAWCHECKER_HAVE_PORTAUDIO)
    if (backendInitialized_)
        Pa_Terminate();
#endif
}

#if defined(DAWCHECKER_HAVE_PORTAUDIO)

bool AudioDeviceManager::refresh()
{
    devices_.clear();
    if (!backendInitialized_)
        return false;

    const int count = Pa_GetDeviceCount();
    if (count < 0)
        return false;

    const PaDeviceIndex defaultIn  = Pa_GetDefaultInputDevice();
    const PaDeviceIndex defaultOut = Pa_GetDefaultOutputDevice();

    for (int i = 0; i < count; ++i)
    {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info == nullptr)
            continue;

        const PaHostApiInfo* hostApi = Pa_GetHostApiInfo(info->hostApi);

        AudioDeviceInfo dev;
        dev.name              = info->name ? info->name : "Unknown device";
        dev.hostApi           = hostApi && hostApi->name ? hostApi->name : "Unknown host API";
        dev.isDefaultInput    = (i == defaultIn);
        dev.isDefaultOutput   = (i == defaultOut);
        dev.maxInputChannels  = info->maxInputChannels;
        dev.maxOutputChannels = info->maxOutputChannels;
        dev.defaultSampleRate = info->defaultSampleRate;
        devices_.push_back(std::move(dev));
    }
    return true;
}

#elif defined(DAWCHECKER_PLATFORM_WINDOWS)

bool AudioDeviceManager::refresh()
{
    using Microsoft::WRL::ComPtr;
    devices_.clear();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool weInitializedCom = SUCCEEDED(hr);

    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                           IID_PPV_ARGS(&enumerator));
    if (FAILED(hr))
    {
        if (weInitializedCom) CoUninitialize();
        return false;
    }

    ComPtr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr))
    {
        if (weInitializedCom) CoUninitialize();
        return false;
    }

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device)))
            continue;

        ComPtr<IPropertyStore> props;
        if (FAILED(device->OpenPropertyStore(STGM_READ, &props)))
            continue;

        PROPVARIANT nameVar;
        PropVariantInit(&nameVar);
        AudioDeviceInfo dev;
        dev.hostApi = "WASAPI";
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &nameVar)) && nameVar.pwszVal)
        {
            char buf[512] = {0};
            WideCharToMultiByte(CP_UTF8, 0, nameVar.pwszVal, -1, buf, sizeof(buf) - 1, nullptr, nullptr);
            dev.name = buf;
        }
        PropVariantClear(&nameVar);

        devices_.push_back(std::move(dev));
    }

    if (weInitializedCom)
        CoUninitialize();
    return true;
}

#elif defined(DAWCHECKER_HAVE_ALSA)

bool AudioDeviceManager::refresh()
{
    devices_.clear();
    void** hints = nullptr;
    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
        return false;

    for (void** n = hints; *n != nullptr; ++n)
    {
        char* name = snd_device_name_get_hint(*n, "NAME");
        char* desc = snd_device_name_get_hint(*n, "DESC");

        AudioDeviceInfo dev;
        dev.hostApi = "ALSA";
        dev.name    = desc ? desc : (name ? name : "Unknown ALSA device");

        if (name) free(name);
        if (desc) free(desc);

        devices_.push_back(std::move(dev));
    }
    snd_device_name_free_hint(hints);
    return true;
}

#else

bool AudioDeviceManager::refresh()
{
    // No platform audio backend available in this build configuration.
    devices_.clear();
    return false;
}

#endif

} // namespace dawchecker::audio
