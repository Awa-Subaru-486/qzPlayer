// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsAudioSink_p.h"

#include <QtCore/private/qsystemerror_p.h>
#include <QtCore/private/qfunctions_win_p.h>
#include <qzMultimedia/private/MemoryResourceTlsf_p.h>
#include <qzMultimedia/private/ComTaskResource_p.h>
#include <qzMultimedia/private/WindowsAudioDevice_p.h>
#include <qzMultimedia/private/WindowsAudioDevices_p.h>
#include <qzMultimedia/private/WindowsResampler_p.h>

#include <audioclient.h>
#include <mmdeviceapi.h>

namespace QtWASAPI {

using WindowsAudioUtils::audioClientErrorString;
using namespace std::chrono_literals;

namespace {

class SinkNotificationClient : public IMMNotificationClient
{
public:
    SinkNotificationClient() = default;
    virtual ~SinkNotificationClient() = default;

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
        if (flow == eRender && role == eMultimedia) {
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

AudioFormat makeHostFormatForSink(const AudioDevice &device, const AudioFormat &format)
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

    return hostFormat;
}

}

WASAPIAudioSinkStream::WASAPIAudioSinkStream(AudioDevice device, const AudioFormat &format, std::optional<qsizetype> ringbufferSize,
                                               WindowsAudioSink *parent, float volume, std::optional<int32_t> hardwareBufferFrames, AudioEndpointRole role):
    PlatformAudioSinkStream{
        std::move(device),
        format,
        ringbufferSize,
        hardwareBufferFrames,
        volume,
    },
    m_role{
          role,
    },
    m_wasapiHandle {
        CreateEvent(0, false, false, nullptr),
    },
    m_parent{
        parent
    },
    m_hostFormat {
        makeHostFormatForSink(m_audioDevice, format),
    },
    m_currentDeviceId{
        (parent && !parent->currentDeviceId().isEmpty()) ? parent->currentDeviceId()
        : (m_audioDevice.isDefault() && !WindowsAudioDevices::currentDefaultOutputDeviceId().isEmpty())
            ? WindowsAudioDevices::currentDefaultOutputDeviceId()
            : m_audioDevice.id(),
    }
{
    if (parent && parent->currentDeviceId().isEmpty())
        parent->setCurrentDeviceId(m_currentDeviceId);
}

WASAPIAudioSinkStream::~WASAPIAudioSinkStream()
{
    if (m_deviceEnumerator && m_notificationClient) {
        m_deviceEnumerator->UnregisterEndpointNotificationCallback(m_notificationClient);
        m_notificationClient->Release();
    }
    if (m_deviceEnumerator) {
        m_deviceEnumerator->Release();
    }
}

bool WASAPIAudioSinkStream::open()
{
    return true;
}

bool WASAPIAudioSinkStream::start(QIODevice *ioDevice)
{
    auto immDevice = WindowsAudioDevice::openDeviceById(m_currentDeviceId);
    if (!immDevice)
        return false;

    bool clientOpen = openAudioClient(std::move(immDevice), m_role);
    if (!clientOpen)
        return false;

    setQIODevice(ioDevice);
    createQIODeviceConnections(ioDevice);
    pullFromQIODevice();

    return startAudioClient(StreamType::Ringbuffer);
}

QIODevice *WASAPIAudioSinkStream::start()
{
    auto immDevice = WindowsAudioDevice::openDeviceById(m_currentDeviceId);
    if (!immDevice)
        return nullptr;

    bool clientOpen = openAudioClient(std::move(immDevice), m_role);
    if (!clientOpen)
        return nullptr;

    QIODevice *ioDevice = createRingbufferWriterDevice();

    m_parent->updateStreamIdle(true, WindowsAudioSink::EmitStateSignal::False);

    setQIODevice(ioDevice);
    createQIODeviceConnections(ioDevice);

    bool started = startAudioClient(StreamType::Ringbuffer);
    return started ? ioDevice : nullptr;
}

bool WASAPIAudioSinkStream::start(AudioCallback audioCallback)
{
    auto immDevice = WindowsAudioDevice::openDeviceById(m_currentDeviceId);
    if (!immDevice)
        return false;

    bool clientOpen = openAudioClient(std::move(immDevice), m_role);
    if (!clientOpen)
        return false;

    m_audioCallback = std::move(audioCallback);

    return startAudioClient(StreamType::Callback);
}

void WASAPIAudioSinkStream::suspend()
{
    m_suspended = true;
    WindowsAudioUtils::audioClientStop(m_audioClient);
}

void WASAPIAudioSinkStream::resume()
{
    m_suspended = false;
    WindowsAudioUtils::audioClientStart(m_audioClient);
}

void WASAPIAudioSinkStream::stop(ShutdownPolicy shutdownPolicy)
{
    using namespace WindowsAudioUtils;

    m_parent = nullptr;
    m_shutdownPolicy = shutdownPolicy;

    requestStop();
    switch (shutdownPolicy) {
    case ShutdownPolicy::DiscardRingbuffer: {
        audioClientStop(m_audioClient);
        joinWorkerThread();
        audioClientReset(m_audioClient);

        return;
    }
    case ShutdownPolicy::DrainRingbuffer: {
        m_ringbufferDrained.callOnActivated([self = shared_from_this()]() mutable {
            self->joinWorkerThread();
            self = {};
        });
        return;
    }
    default:
        Q_UNREACHABLE_RETURN();
    }
}

void WASAPIAudioSinkStream::updateStreamIdle(bool streamIsIdle)
{
    if (m_parent)
        m_parent->updateStreamIdle(streamIsIdle);
}

bool WASAPIAudioSinkStream::openAudioClient(ComPtr<IMMDevice> device, AudioEndpointRole role)
{
    using namespace WindowsAudioUtils;

    std::optional<AudioClientCreationResult> clientData =
            createAudioClient(device, m_hostFormat, m_hardwareBufferFrames, m_wasapiHandle, role);

    if (!clientData)
        return false;

    m_audioClient = std::move(clientData->client);
    m_periodSize = clientData->periodSize;
    m_audioClientFrames = clientData->audioClientFrames;

    HRESULT hr = m_audioClient->GetService(IID_PPV_ARGS(m_renderClient.GetAddressOf()));
    if (FAILED(hr)) {
        qWarning() << "IAudioClient3::GetService failed to obtain IAudioRenderClient"
                   << audioClientErrorString(hr);
        return false;
    }

    if (m_audioDevice.preferredFormat().sampleRate() != m_hostFormat.sampleRate())
        audioClientSetRate(m_audioClient, m_hostFormat.sampleRate());

    return true;
}

bool WASAPIAudioSinkStream::startAudioClient(StreamType streamType)
{
    using namespace WindowsAudioUtils;
    m_workerThread.reset(QThread::create([this, streamType] {
        setMCSSForPeriodSize(m_periodSize);
        fillInitialHostBuffer();
        std::optional<QComHelper> m_comHelper;

        if (m_hostFormat != m_format) {
            m_comHelper.emplace();
            m_resampler = std::make_unique<WindowsResampler>();
            m_resampler->setup(m_format, m_hostFormat);

            m_memoryResource = std::make_unique<TlsfMemoryResource>(512 * 1024);
        }

        setupDeviceMonitor();

        switch (streamType) {
        case StreamType::Ringbuffer:
            return runProcessRingbufferLoop();
        case StreamType::Callback:
            return runProcessCallbackLoop();
        }
    }));
    m_workerThread->setObjectName(u"WASAPIAudioSinkStream");
    m_workerThread->start();

    bool started = WindowsAudioUtils::audioClientStart(m_audioClient);
    if (!started) {
        joinWorkerThread();
        return false;
    }
    return true;
}

void WASAPIAudioSinkStream::fillInitialHostBuffer()
{
    processRingbuffer();
}

void WASAPIAudioSinkStream::runProcessRingbufferLoop()
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

        if (isStopRequested()) {
            switch (m_shutdownPolicy.load(std::memory_order_relaxed)) {
            case ShutdownPolicy::DiscardRingbuffer:
                return;
            case ShutdownPolicy::DrainRingbuffer: {
                bool bufferDrained = visitRingbuffer([](const auto &ringbuffer) {
                    return ringbuffer.used() == 0;
                });
                if (bufferDrained) {
                    audioClientStop(m_audioClient);
                    audioClientReset(m_audioClient);

                    m_ringbufferDrained.set();
                    return;
                }
                break;
            }
            default:
                Q_UNREACHABLE_RETURN();
            }
        }

        bool success = processRingbuffer();
        if (!success) {
            handleAudioClientError();
            return;
        }
    }
}

void WASAPIAudioSinkStream::runProcessCallbackLoop()
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

        bool success = processCallback();
        if (!success) {
            handleAudioClientError();
            return;
        }
    }
}

template <typename Functor>
bool WASAPIAudioSinkStream::visitAudioClientBuffer(Functor &&f)
{
    uint32_t numFramesPadding;
    HRESULT hr = m_audioClient->GetCurrentPadding(&numFramesPadding);
    if (FAILED(hr)) {
        qWarning() << "IAudioClient3::GetCurrentPadding failed" << audioClientErrorString(hr);
        return false;
    }

    const uint32_t requiredFrames = m_audioClientFrames - numFramesPadding;
    if (requiredFrames == 0)
        return true;

    unsigned char *hostBuffer{};
    hr = m_renderClient->GetBuffer(requiredFrames, &hostBuffer);
    if (FAILED(hr)) {
        qWarning() << "IAudioRenderClient::getBuffer failed" << audioClientErrorString(hr);
        return false;
    }

    std::span<std::byte> hostBufferSpan{
        reinterpret_cast<std::byte *>(hostBuffer),
        static_cast<size_t>(m_hostFormat.bytesForFrames(requiredFrames)),
    };

    uint64_t consumedFrames;
    if (m_resampler) {
        Q_UNLIKELY_BRANCH;

        std::pmr::vector<std::byte> resampleBuffer{
            size_t(m_format.bytesForFrames(requiredFrames)),
            m_memoryResource.get(),
        };
        consumedFrames = f(std::as_writable_bytes(std::span{ resampleBuffer }), requiredFrames);

        auto resampledBuffer = m_resampler->resample(resampleBuffer, m_memoryResource.get());

        Q_ASSERT(resampledBuffer.size() == size_t(hostBufferSpan.size()));
        std::copy_n(resampledBuffer.data(), resampledBuffer.size(), hostBufferSpan.data());
    } else {
        consumedFrames = f(hostBufferSpan, requiredFrames);
    }

    const DWORD flags = consumedFrames != 0 ? 0 : AUDCLNT_BUFFERFLAGS_SILENT;

    hr = m_renderClient->ReleaseBuffer(requiredFrames, flags);
    if (FAILED(hr)) {
        qWarning() << "IAudioRenderClient::ReleaseBuffer failed" << audioClientErrorString(hr);
        return false;
    }

    return true;
}

bool WASAPIAudioSinkStream::processRingbuffer() noexcept QT_MM_NONBLOCKING
{
    return visitAudioClientBuffer([&](std::span<std::byte> hostBuffer, uint32_t requiredFrames) {
        uint64_t consumedFrames = PlatformAudioSinkStream::process(hostBuffer, requiredFrames);
        return consumedFrames;
    });
}

bool WASAPIAudioSinkStream::processCallback() noexcept QT_MM_NONBLOCKING
{
    return visitAudioClientBuffer([&](std::span<std::byte> hostBuffer, uint32_t requiredFrames) {
        runAudioCallback(m_audioCallback, hostBuffer, m_format, volume());
        return requiredFrames;
    });
}

void WASAPIAudioSinkStream::joinWorkerThread()
{
    requestStop();
    ::SetEvent(m_wasapiHandle.get());
    m_workerThread->wait();
    m_workerThread = {};
}

void WASAPIAudioSinkStream::handleAudioClientError()
{
    using namespace WindowsAudioUtils;
    requestStop();
    audioClientStop(m_audioClient);
    audioClientReset(m_audioClient);

    invokeOnAppThread([this] {
        handleIOError(m_parent);
    });
}

void WASAPIAudioSinkStream::setupDeviceMonitor()
{
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&m_deviceEnumerator));

    if (FAILED(hr)) {
        qWarning() << "WASAPIAudioSinkStream: Failed to create MMDeviceEnumerator";
        return;
    }

    auto *client = new SinkNotificationClient();
    hr = m_deviceEnumerator->RegisterEndpointNotificationCallback(client);

    if (FAILED(hr)) {
        qWarning() << "WASAPIAudioSinkStream: Failed to register notification callback";
        client->Release();
        return;
    }

    m_notificationClient = client;
    m_deviceChanged = false;
}

void WASAPIAudioSinkStream::checkDeviceChange()
{
    if (m_notificationClient) {
        auto *client = static_cast<SinkNotificationClient*>(m_notificationClient);
        if (client->deviceChangedFlag().exchange(false)) {
            reopenStreamWithNewDevice();
        }
    }
}

bool WASAPIAudioSinkStream::reopenStreamWithNewDevice()
{
    using namespace WindowsAudioUtils;

    if (m_audioClient) {
        audioClientStop(m_audioClient);
        audioClientReset(m_audioClient);
    }

    m_audioClient.Reset();
    m_renderClient.Reset();

    Sleep(100);

    ComPtr<IMMDevice> immDevice;
    if (m_deviceEnumerator) {
        HRESULT hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia,
                                                                  immDevice.GetAddressOf());
        if (FAILED(hr)) {
            qWarning() << "WASAPIAudioSinkStream: GetDefaultAudioEndpoint failed";
            return false;
        }

        ComTaskResource<WCHAR> newDeviceId;
        hr = immDevice->GetId(newDeviceId.address());
        if (SUCCEEDED(hr)) {
            m_currentDeviceId = QString::fromWCharArray(newDeviceId.get()).toUtf8();
            if (m_parent)
                m_parent->setCurrentDeviceId(m_currentDeviceId);
            WindowsAudioDevices::setCurrentDefaultOutputDeviceId(m_currentDeviceId);
        }
    } else {
        immDevice = WindowsAudioDevice::openDeviceById(m_currentDeviceId);
        if (!immDevice) {
            qWarning() << "WASAPIAudioSinkStream: Failed to open device";
            return false;
        }
    }

    if (!openAudioClient(std::move(immDevice), m_role)) {
        qWarning() << "WASAPIAudioSinkStream: Failed to open audio client with new device";
        return false;
    }

    if (!m_suspended.load()) {
        fillInitialHostBuffer();
        if (!audioClientStart(m_audioClient)) {
            qWarning() << "WASAPIAudioSinkStream: Failed to start audio client after reopen";
            return false;
        }
    }

    return true;
}

WindowsAudioSink::WindowsAudioSink(AudioDevice audioDevice, const AudioFormat &fmt,
                                     QObject *parent)
    : BaseClass(std::move(audioDevice), fmt, parent)
{
}

}

