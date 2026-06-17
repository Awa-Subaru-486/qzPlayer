// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsAudioDevice_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qt_windows.h>
import qzLog;
#include <QtCore/private/qsystemerror_p.h>

#include <qzMultimedia/private/AudioFormat_p.h>
#include <qzMultimedia/private/ComInitializer_p.h>
#include <qzMultimedia/private/ComTaskResource_p.h>
#include <qzMultimedia/private/WindowsPropertystore_p.h>
#include <qzMultimedia/private/WindowsAudioUtils_p.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <propkeydef.h>

#include <set>

using QtMultimediaPrivate::PropertyStoreHelper;

namespace {

static qz::Log::LogCategory qLcAudioDeviceProbes("qz.multimedia.audiodevice.probes");

std::optional<EndpointFormFactor> inferFormFactor(PropertyStoreHelper &propertyStore)
{
    std::optional<uint32_t> val = propertyStore.getUInt32(PKEY_AudioEndpoint_FormFactor);
    if (val == EndpointFormFactor::UnknownFormFactor)
        return EndpointFormFactor(*val);

    return std::nullopt;
}

std::optional<AudioFormat::ChannelConfig>
inferChannelConfiguration(PropertyStoreHelper &propertyStore, int maximumChannelCount)
{
    std::optional<uint32_t> val = propertyStore.getUInt32(PKEY_AudioEndpoint_PhysicalSpeakers);
    if (val && val != 0)
        return WindowsAudioUtils::maskToChannelConfig(*val, maximumChannelCount);

    return std::nullopt;
}

int maxChannelCountForFormFactor(EndpointFormFactor formFactor)
{
    switch (formFactor) {
    case EndpointFormFactor::Headphones:
    case EndpointFormFactor::Headset:
        return 2;
    case EndpointFormFactor::SPDIF:
        return 6;

    case EndpointFormFactor::DigitalAudioDisplayDevice:
        return 8;

    case EndpointFormFactor::Microphone:
        return 32;

    default:
        return 128;
    }
}

struct FormatProbeResult
{
    void update(const AudioFormat &fmt)
    {
        supportedSampleFormats.insert(fmt.sampleFormat());
        updateChannelCount(fmt.channelCount());
        updateSamplingRate(fmt.sampleRate());
    }

    void updateChannelCount(int channelCount)
    {
        if (channelCount < channelCountRange.first)
            channelCountRange.first = channelCount;
        if (channelCount > channelCountRange.second)
            channelCountRange.second = channelCount;
    }

    void updateSamplingRate(int samplingRate)
    {
        if (samplingRate < sampleRateRange.first)
            sampleRateRange.first = samplingRate;
        if (samplingRate > sampleRateRange.second)
            sampleRateRange.second = samplingRate;
    }

    std::set<AudioFormat::SampleFormat> supportedSampleFormats;
    std::pair<int, int> channelCountRange{ std::numeric_limits<int>::max(), 0 };
    std::pair<int, int> sampleRateRange{ std::numeric_limits<int>::max(), 0 };

    [[maybe_unused]]
    friend QDebug operator<<(QDebug dbg, const FormatProbeResult &self)
    {
        QDebugStateSaver saver(dbg);
        dbg.nospace();

        dbg << "FormatProbeResult{supportedSampleFormats: " << self.supportedSampleFormats
            << ", channelCountRange: " << self.channelCountRange.first << " - " << self.channelCountRange.second
            << ", sampleRateRange: " << self.sampleRateRange.first << "-" << self.sampleRateRange.second
            << "}";
        return dbg;
    }
};

std::optional<AudioFormat> performIsFormatSupportedWithClosestMatch(const ComPtr<IAudioClient> &audioClient,
                                                                     const AudioFormat &fmt)
{
    using namespace WindowsAudioUtils;
    std::optional<WAVEFORMATEXTENSIBLE> formatEx = toWaveFormatExtensible(fmt);
    if (!formatEx) {
        qz::Log::cat_warn(qLcAudioDeviceProbes, "toWaveFormatExtensible failed");
        return std::nullopt;
    }

    qz::Log::cat_debug(qLcAudioDeviceProbes, "performIsFormatSupportedWithClosestMatch");
    ComTaskResource<WAVEFORMATEX> closestMatch;
    HRESULT result = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &formatEx->Format,
                                                    closestMatch.address());

    if (FAILED(result)) {
        qz::Log::cat_debug(qLcAudioDeviceProbes, "performIsFormatSupportedWithClosestMatch: error {}", QSystemError::windowsComString(result));
        return std::nullopt;
    }

    if (closestMatch) {
        AudioFormat closestMatchFormat = waveFormatExToFormat(*closestMatch);
        qz::Log::cat_debug(qLcAudioDeviceProbes, "performProbe returned closest match");
        return closestMatchFormat;
    }

    qz::Log::cat_debug(qLcAudioDeviceProbes, "performProbe successful");

    return fmt;
}

std::optional<FormatProbeResult> probeFormats(const ComPtr<IAudioClient> &audioClient,
                                              PropertyStoreHelper &propertyStore,
                                              const AudioFormat &preferredFormat)
{
    using namespace WindowsAudioUtils;

    std::optional<EndpointFormFactor> formFactor = inferFormFactor(propertyStore);
    int maxChannelsForFormFactor = formFactor ? maxChannelCountForFormFactor(*formFactor) : 128;

    qz::Log::cat_debug(qLcAudioDeviceProbes, "probing: maxChannelsForFormFactor {}", maxChannelsForFormFactor);

    std::optional<FormatProbeResult> limits;

    constexpr AudioFormat::SampleFormat initialSampleFormat = AudioFormat::SampleFormat::Float;

    AudioFormat initialProbeFormat;
    initialProbeFormat.setSampleFormat(initialSampleFormat);
    initialProbeFormat.setSampleRate(preferredFormat.sampleRate());
    initialProbeFormat.setChannelCount(maxChannelsForFormFactor);

    qz::Log::cat_debug(qLcAudioDeviceProbes, "probeFormats: probing for {} Hz, {} ch, fmt {}", initialProbeFormat.sampleRate(), initialProbeFormat.channelCount(), static_cast<int>(initialProbeFormat.sampleFormat()));

    std::optional<AudioFormat> initialProbeResult =
            performIsFormatSupportedWithClosestMatch(audioClient, initialProbeFormat);

    int maxChannelForFormat;
    if (initialProbeResult) {
        if (initialProbeResult->sampleRate() != preferredFormat.sampleRate()) {
            qz::Log::cat_debug(qLcAudioDeviceProbes, "probing: returned a different sample rate as closest match");
            return std::nullopt;
        }

        maxChannelForFormat = initialProbeResult->channelCount();
    } else {

        maxChannelForFormat = std::min(maxChannelsForFormFactor, 2);
    }

    AudioFormat::SampleFormat probeSampleFormat =
            initialProbeResult ? initialProbeResult->sampleFormat() : initialSampleFormat;

    for (int channelCount = 1; channelCount != maxChannelForFormat + 1; ++channelCount) {
        AudioFormat fmt;
        fmt.setSampleFormat(probeSampleFormat);
        fmt.setSampleRate(preferredFormat.sampleRate());
        fmt.setChannelCount(channelCount);

        std::optional<WAVEFORMATEXTENSIBLE> formatEx = toWaveFormatExtensible(fmt);
        if (!formatEx)
            continue;

        qz::Log::cat_debug(qLcAudioDeviceProbes, "probing {} Hz, {} ch", fmt.sampleRate(), fmt.channelCount());

        ComTaskResource<WAVEFORMATEX> closestMatch;
        HRESULT result = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &formatEx->Format,
                                                        closestMatch.address());

        if (FAILED(result)) {
            qz::Log::cat_debug(qLcAudioDeviceProbes, "probing format failed {}", QSystemError::windowsComString(result));
            continue;
        }

        if (closestMatch) {
            qz::Log::cat_debug(qLcAudioDeviceProbes, "probing format reported a closest match");
            continue;
        }

        if (!limits)
            limits = FormatProbeResult{};

        qz::Log::cat_debug(qLcAudioDeviceProbes, "probing format successful {} Hz, {} ch", fmt.sampleRate(), fmt.channelCount());
        limits->update(fmt);
    }

    qz::Log::cat_debug(qLcAudioDeviceProbes, "probing successful");

    return limits;
}

std::optional<AudioFormat> probePreferredFormat(const ComPtr<IAudioClient> &audioClient)
{
    using namespace WindowsAudioUtils;

    static const AudioFormat preferredFormat = [] {
        AudioFormat fmt;
        fmt.setSampleRate(44100);
        fmt.setChannelCount(2);
        fmt.setSampleFormat(AudioFormat::Int16);
        return fmt;
    }();

    std::optional<WAVEFORMATEXTENSIBLE> formatEx = toWaveFormatExtensible(preferredFormat);
    if (!formatEx)
        return std::nullopt;

    ComTaskResource<WAVEFORMATEX> closestMatch;
    HRESULT result = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &formatEx->Format,
                                                    closestMatch.address());

    if (FAILED(result))
        return std::nullopt;
    if (!closestMatch)
        return preferredFormat;

    AudioFormat closestMatchFormat = waveFormatExToFormat(*closestMatch);
    if (closestMatchFormat.isValid())
        return closestMatchFormat;
    return std::nullopt;
}

}

WindowsAudioDevice::WindowsAudioDevice(QByteArray id, ComPtr<IMMDevice> immDev, QString desc,
                                         AudioDevice::Mode mode)
    : AudioDevicePrivate(std::move(id), mode, std::move(desc))
{
    Q_ASSERT(immDev);

    ComPtr<IAudioClient> audioClient;
    HRESULT hr = immDev->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                                  reinterpret_cast<void **>(audioClient.GetAddressOf()));

    if (SUCCEEDED(hr)) {
        ComTaskResource<WAVEFORMATEX> mixFormat;
        hr = audioClient->GetMixFormat(mixFormat.address());
        if (SUCCEEDED(hr))
            preferredFormat = WindowsAudioUtils::waveFormatExToFormat(*mixFormat);
    } else {
        qWarning() << "WindowsAudioDeviceInfo: could not activate audio client:" << description
                   << QSystemError::windowsComString(hr);
        return;
    }

    auto propStoreHelper = PropertyStoreHelper::open(immDev);
    if (!propStoreHelper) {
        qWarning() << "WindowsAudioDeviceInfo: could not open property store:" << description
                   << propStoreHelper.error();
        return;
    }

    qz::Log::cat_debug(qLcAudioDeviceProbes, "probing formats for {}", description);

    std::optional<FormatProbeResult> probedFormats =
            probeFormats(audioClient, *propStoreHelper, preferredFormat);
    if (probedFormats) {

        supportedSampleFormats = qAllSupportedSampleFormats();

        minimumSampleRate = QtMultimediaPrivate::allSupportedSampleRates.front();
        maximumSampleRate = QtMultimediaPrivate::allSupportedSampleRates.back();

        minimumChannelCount = 1;

        maximumChannelCount = probedFormats->channelCountRange.second;

        m_probedChannelCountRange = probedFormats->channelCountRange;
        m_probedSampleRateRange = probedFormats->sampleRateRange;
    }

    if (!preferredFormat.isValid()) {
        std::optional<AudioFormat> probedFormat = probePreferredFormat(audioClient);
        if (probedFormat)
            preferredFormat = *probedFormat;
    }

    std::optional<AudioFormat::ChannelConfig> config =
            inferChannelConfiguration(*propStoreHelper, maximumChannelCount);

    channelConfiguration = config
            ? *config
            : AudioFormat::defaultChannelConfigForChannelCount(maximumChannelCount);
}

ComPtr<IMMDevice> WindowsAudioDevice::open() const
{
    return openDeviceById(id);
}

ComPtr<IMMDevice> WindowsAudioDevice::openDeviceById(const QByteArray &deviceId)
{
    ComInitializer init;
    ComPtr<IMMDeviceEnumerator> deviceEnumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&deviceEnumerator));
    if (FAILED(hr)) {
        qWarning() << "Failed to create device enumerator" << hr;
        return nullptr;
    }

    auto deviceIdStr = QString::fromUtf8(deviceId);

    ComPtr<IMMDevice> device;
    HRESULT result =
            deviceEnumerator->GetDevice(deviceIdStr.toStdWString().c_str(), device.GetAddressOf());
    if (FAILED(result)) {
        qWarning() << "IMMDeviceEnumerator::GetDevice failed" << deviceId
                   << QSystemError::windowsComString(result);
        return nullptr;
    }
    return device;
}

WindowsAudioDevice::~WindowsAudioDevice() = default;

