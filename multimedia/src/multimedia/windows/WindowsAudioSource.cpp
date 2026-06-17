// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsAudioSource_p.h"

#include <QtCore/qthread.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/private/qfunctions_win_p.h>
#include <QtCore/private/quniquehandle_types_p.h>
#include <qzMultimedia/private/AudioFormat_p.h>
#include <qzMultimedia/private/AudioSystemPlatformStreamSupport_p.h>
#include <qzMultimedia/private/MemoryResourceTlsf_p.h>
#include <qzMultimedia/private/ComTaskResource_p.h>
#include <qzMultimedia/private/WindowsAudioDevice_p.h>
#include <qzMultimedia/private/WindowsAudioDevices_p.h>
#include <qzMultimedia/private/WindowsAudioUtils_p.h>

#include <audioclient.h>
#include <mmdeviceapi.h>

namespace QtWASAPI {

using WindowsAudioUtils::audioClientErrorString;
using namespace std::chrono_literals;

namespace {

class SourceNotificationClient : public IMMNotificationClient
{
public:
    SourceNotificationClient() = default;
    virtual ~SourceNotificationClient() = default;

    std::atomic<bool> &deviceChangedFlag() { return m_deviceChanged; }

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_refCount); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) delete this;
        return ref;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override
    {
        if (riid == IID_IUnknown || riid == __uuidof(IMMNotificationClient)) {
            *ppv = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override
    {
        if (flow == eCapture && role == eMultimedia) {
            m_deviceChanged.store(true);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    LONG m_refCount = 1;
    std::atomic<bool> m_deviceChanged{false};
};

AudioFormat makeHostFormatForSource(const AudioDevice &device, const AudioFormat &format)
{
    const WindowsAudioDevice *winDevice = AudioDevicePrivate::handle<WindowsAudioDevice>(device);

    AudioFormat hostFormat = format;
    const int requestedChannelCount = format.channelCount();
    auto [minProbedChannels, maxProbedChannels] = winDevice->m_probedChannelCountRange;

    if (requestedChannelCount < device.minimumChannelCount()) {
        hostFormat.setChannelCount(minProbedChannels);
        hostFormat.setChannelConfig(
                AudioFormat::defaultChannelConfigForChannelCount(minProbedChannels));
    } else if (requestedChannelCount > device.maximumChannelCount()) {
        hostFormat.setChannelCount(maxProbedChannels);
        hostFormat.setChannelConfig(
                AudioFormat::defaultChannelConfigForChannelCount(maxProbedChannels));
    }

    const int requestedSampleRate = format.sampleRate();
    auto [minProbedSampleRate, maxProbedSampleRate] = winDevice->m_probedSampleRateRange;

    if (requestedSampleRate < device.minimumSampleRate())
        hostFormat.setSampleRate(minProbedSampleRate);
    else if (requestedSampleRate > device.maximumSampleRate())
        hostFormat.setSampleRate(maxProbedSampleRate);

    return hostFormat;
}
}

WASAPIAudioSourceStream::WASAPIAudioSourceStream(AudioDevice device, const AudioFormat &format,
                                                   std::optional<qsizetype> ringbufferSize,
                                                   WindowsAudioSource *parent,
                                                   float volume,
                                                   std::optional<int32_t> hardwareBufferFrames):
    PlatformAudioSourceStream{
        std::move(device),
        format,
        ringbufferSize,
        hardwareBufferFrames,
        volume,
    },
    m_wasapiHandle {
        CreateEvent(0, false, false, nullptr),
    },
    m_parent{
        parent
    },
    m_hostFormat {
        makeHostFormatForSource(m_audioDevice, format),
    },
    m_currentDeviceId{
        (parent && !parent->currentDeviceId().isEmpty()) ? parent->currentDeviceId()
        : (m_audioDevice.isDefault() && !WindowsAudioDevices::currentDefaultInputDeviceId().isEmpty())
            ? WindowsAudioDevices::currentDefaultInputDeviceId()
            : m_audioDevice.id(),
    }
{
    if (parent && parent->currentDeviceId().isEmpty())
        parent->setCurrentDeviceId(m_currentDeviceId);
}

WASAPIAudioSourceStream::~WASAPIAudioSourceStream()
{
    if (m_deviceEnumerator && m_notificationClient) {
        m_deviceEnumerator->UnregisterEndpointNotificationCallback(m_notificationClient);
        m_notificationClient->Release();
    }
    if (m_deviceEnumerator) {
        m_deviceEnumerator->Release();
    }
}

bool WASAPIAudioSourceStream::start(QIODevice *ioDevice)
{
    auto immDevice = WindowsAudioDevice::openDeviceById(m_currentDeviceId);
    if (!immDevice)
        return false;

    bool clientOpen = openAudioClient(std::move(immDevice));
    if (!clientOpen)
        return false;

    setQIODevice(ioDevice);
    createQIODeviceConnections(ioDevice);

    return startAudioClient();
}

QIODevice *WASAPIAudioSourceStream::start()
{
    auto immDevice = WindowsAudioDevice::openDeviceById(m_currentDeviceId);
    if (!immDevice)
        return nullptr;

    bool clientOpen = openAudioClient(std::move(immDevice));
    if (!clientOpen)
        return nullptr;

    QIODevice *ioDevice = createRingbufferReaderDevice();

    m_parent->updateStreamIdle(true, WindowsAudioSource::EmitStateSignal::False);

    setQIODevice(ioDevice);
    createQIODeviceConnections(ioDevice);

    bool started = startAudioClient();
    return started ? ioDevice : nullptr;
}

bool WASAPIAudioSourceStream::start(AudioCallback &&cb)
{
    auto immDevice = WindowsAudioDevice::openDeviceById(m_currentDeviceId);
    if (!immDevice)
        return false;

    bool clientOpen = openAudioClient(std::move(immDevice));
    if (!clientOpen)
        return false;

    m_audioCallback = std::move(cb);

    return startAudioClient();
}

void WASAPIAudioSourceStream::suspend()
{
    m_suspended = true;
    WindowsAudioUtils::audioClientStop(m_audioClient);
}

void WASAPIAudioSourceStream::resume()
{
    m_suspended = false;
    WindowsAudioUtils::audioClientStart(m_audioClient);
}

void WASAPIAudioSourceStream::stop(ShutdownPolicy shutdownPolicy)
{
    m_parent = nullptr;
    m_shutdownPolicy = shutdownPolicy;

    requestStop();
    disconnectQIODeviceConnections();
    WindowsAudioUtils::audioClientStop(m_audioClient);

    joinWorkerThread();
    WindowsAudioUtils::audioClientReset(m_audioClient);

    finalizeQIODevice(shutdownPolicy);
    if (shutdownPolicy == ShutdownPolicy::DiscardRingbuffer)
        emptyRingbuffer();
}

void WASAPIAudioSourceStream::updateStreamIdle(bool streamIsIdle)
{
    if (m_parent)
        m_parent->updateStreamIdle(streamIsIdle);
}

bool WASAPIAudioSourceStream::openAudioClient(ComPtr<IMMDevice> device)
{
    using namespace WindowsAudioUtils;

    std::optional<AudioClientCreationResult> clientData =
            createAudioClient(device, m_hostFormat, m_hardwareBufferFrames, m_wasapiHandle);

    if (!clientData)
        return false;

    m_audioClient = std::move(clientData->client);
    m_periodSize = clientData->periodSize;
    m_audioClientFrames = clientData->audioClientFrames;

    HRESULT hr = m_audioClient->GetService(IID_PPV_ARGS(m_captureClient.GetAddressOf()));
    if (FAILED(hr)) {
        qWarning() << "IAudioClient3::GetService failed to obtain IAudioCaptureClient"
                   << audioClientErrorString(hr);
        return false;
    }

    return true;
}

bool WASAPIAudioSourceStream::startAudioClient()
{
    using namespace WindowsAudioUtils;
    m_workerThread.reset(QThread::create([this] {
        setMCSSForPeriodSize(m_periodSize);

        std::optional<QComHelper> m_comHelper;

        if (m_hostFormat != m_format) {
            m_comHelper.emplace();
            m_resampler = std::make_unique<WindowsResampler>();
            m_resampler->setup(m_hostFormat, m_format);

            m_memoryResource = std::make_unique<TlsfMemoryResource>(512 * 1024);
        }

        setupDeviceMonitor();
        runProcessLoop();
    }));

    m_workerThread->setObjectName(u"WASAPIAudioSourceStream");
    m_workerThread->start();

    bool clientStarted = audioClientStart(m_audioClient);
    if (!clientStarted) {
        joinWorkerThread();
        return false;
    }

    return true;
}

void WASAPIAudioSourceStream::runProcessLoop()
{
    using namespace WindowsAudioUtils;
    for (;;) {
        constexpr std::chrono::milliseconds timeout = 2s;
        DWORD retval = WaitForSingleObject(m_wasapiHandle.get(), timeout.count());
        if (retval != WAIT_OBJECT_0) {
            if (m_suspended)
                continue;

            handleAudioClientError();
            return;
        }

        checkDeviceChange();

        if (isStopRequested())
            return;

        bool success = m_audioCallback ? processCallback() : processRingbuffer();
        if (!success) {
            handleAudioClientError();
            return;
        }
    }
}

template <typename Functor>
bool WASAPIAudioSourceStream::visitAudioClientBuffer(Functor &&f)
{
    for (;;) {
        unsigned char *hostBuffer;
        uint32_t hostBufferFrames;
        DWORD flags;
        uint64_t devicePosition;
        uint64_t QPCPosition;
        HRESULT hr = m_captureClient->GetBuffer(&hostBuffer, &hostBufferFrames, &flags,
                                                &devicePosition, &QPCPosition);
        if (FAILED(hr)) {
            qWarning() << "IAudioCaptureClient::GetBuffer failed" << audioClientErrorString(hr);
            return false;
        }

        if (hostBufferFrames == 0)
            return true;

        std::span hostBufferSpan{
            hostBuffer,
            static_cast<size_t>(m_hostFormat.bytesForFrames(hostBufferFrames)),
        };

        if (m_resampler) {
            Q_UNLIKELY_BRANCH;
            auto resampledBuffer =
                    m_resampler->resample(std::as_bytes(hostBufferSpan), m_memoryResource.get());
            PlatformAudioSourceStream::process(resampledBuffer,
                                                m_format.framesForBytes(resampledBuffer.size()));
        } else {
            f(std::as_bytes(hostBufferSpan), hostBufferFrames);
        }

        hr = m_captureClient->ReleaseBuffer(hostBufferFrames);

        if (FAILED(hr)) {
            qWarning() << "IAudioCaptureClient::ReleaseBuffer failed" << audioClientErrorString(hr);
            return false;
        }
    }
}

bool WASAPIAudioSourceStream::processRingbuffer() noexcept QT_MM_NONBLOCKING
{
    return visitAudioClientBuffer(
            [&](std::span<const std::byte> hostBuffer, uint32_t hostBufferFrames) {
        uint64_t framesWritten = PlatformAudioSourceStream::process(hostBuffer, hostBufferFrames);
        if (framesWritten != hostBufferFrames)
            updateStreamIdle(true);
    });
}

bool WASAPIAudioSourceStream::processCallback() noexcept QT_MM_NONBLOCKING
{
    return visitAudioClientBuffer([&](std::span<const std::byte> hostBuffer, uint32_t) {
        runAudioCallback(*m_audioCallback, std::as_bytes(hostBuffer), m_format, volume());
    });
}

void WASAPIAudioSourceStream::handleAudioClientError()
{
    using namespace WindowsAudioUtils;
    audioClientStop(m_audioClient);
    audioClientReset(m_audioClient);
    requestStop();

    invokeOnAppThread([this] {
        handleIOError(m_parent);
    });
}

void WASAPIAudioSourceStream::joinWorkerThread()
{
    requestStop();
    ::SetEvent(m_wasapiHandle.get());
    m_workerThread->wait();
    m_workerThread = {};
}

void WASAPIAudioSourceStream::setupDeviceMonitor()
{
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&m_deviceEnumerator));

    if (FAILED(hr)) {
        qWarning() << "WASAPIAudioSourceStream: Failed to create MMDeviceEnumerator";
        return;
    }

    auto *client = new SourceNotificationClient();
    hr = m_deviceEnumerator->RegisterEndpointNotificationCallback(client);

    if (FAILED(hr)) {
        qWarning() << "WASAPIAudioSourceStream: Failed to register notification callback";
        client->Release();
        return;
    }

    m_notificationClient = client;
    m_deviceChanged = false;
}

void WASAPIAudioSourceStream::checkDeviceChange()
{
    if (m_notificationClient) {
        auto *client = static_cast<SourceNotificationClient*>(m_notificationClient);
        if (client->deviceChangedFlag().exchange(false)) {
            reopenStreamWithNewDevice();
        }
    }
}

bool WASAPIAudioSourceStream::reopenStreamWithNewDevice()
{
    using namespace WindowsAudioUtils;

    if (m_audioClient) {
        audioClientStop(m_audioClient);
        audioClientReset(m_audioClient);
    }

    m_audioClient.Reset();
    m_captureClient.Reset();

    Sleep(100);

    ComPtr<IMMDevice> immDevice;
    if (m_deviceEnumerator) {
        HRESULT hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia,
                                                                   immDevice.GetAddressOf());
        if (FAILED(hr)) {
            qWarning() << "WASAPIAudioSourceStream: GetDefaultAudioEndpoint failed";
            return false;
        }

        ComTaskResource<WCHAR> newDeviceId;
        hr = immDevice->GetId(newDeviceId.address());
        if (SUCCEEDED(hr)) {
            m_currentDeviceId = QString::fromWCharArray(newDeviceId.get()).toUtf8();
            if (m_parent)
                m_parent->setCurrentDeviceId(m_currentDeviceId);
            WindowsAudioDevices::setCurrentDefaultInputDeviceId(m_currentDeviceId);
        }
    } else {
        immDevice = WindowsAudioDevice::openDeviceById(m_currentDeviceId);
        if (!immDevice) {
            qWarning() << "WASAPIAudioSourceStream: Failed to open device";
            return false;
        }
    }

    if (!openAudioClient(std::move(immDevice))) {
        qWarning() << "WASAPIAudioSourceStream: Failed to open audio client with new device";
        return false;
    }

    if (!m_suspended.load()) {
        if (!audioClientStart(m_audioClient)) {
            qWarning() << "WASAPIAudioSourceStream: Failed to start audio client after reopen";
            return false;
        }
    }

    return true;
}

WindowsAudioSource::WindowsAudioSource(AudioDevice audioDevice, const AudioFormat &fmt,
                                         QObject *parent)
    : BaseClass(std::move(audioDevice), fmt, parent)
{
}

}

