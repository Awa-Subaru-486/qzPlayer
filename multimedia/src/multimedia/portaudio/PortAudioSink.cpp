// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PortAudioSink_p.h"
#include "PortAudioDevice_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qmutex.h>
#include <QtCore/qwaitcondition.h>
#include <qzMultimedia/private/AudioHelpers_p.h>

#ifdef Q_OS_WINDOWS
#include <initguid.h>
#include <mmdeviceapi.h>
#endif

namespace {

PaSampleFormat toPaSampleFormat(AudioFormat::SampleFormat format)
{
    switch (format) {
    case AudioFormat::SampleFormat::Float:
        return paFloat32;
    case AudioFormat::SampleFormat::Int16:
        return paInt16;
    case AudioFormat::SampleFormat::Int32:
        return paInt32;
    case AudioFormat::SampleFormat::UInt8:
        return paUInt8;
    default:
        return paFloat32;
    }
}

std::optional<AudioHelperInternal::NativeSampleFormat> toNativeSampleFormat(PaSampleFormat format)
{
    using namespace AudioHelperInternal;
    switch (format) {
    case paFloat32:
        return NativeSampleFormat::float32_t;
    case paInt16:
        return NativeSampleFormat::int16_t;
    case paInt32:
        return NativeSampleFormat::int32_t;
    case paUInt8:
        return NativeSampleFormat::uint8_t;
    default:
        return std::nullopt;
    }
}

}

#ifdef Q_OS_WINDOWS

class PortAudioNotificationClient : public IMMNotificationClient
{
public:
    PortAudioNotificationClient() = default;
    virtual ~PortAudioNotificationClient() = default;

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
        if (flow == eRender && role == eConsole) {
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

#endif

PortAudioSinkStream::PortAudioSinkStream(AudioDevice device, const AudioFormat &format,
                                           std::optional<qsizetype> ringbufferSize,
                                           PortAudioSink *parent, float volume,
                                           std::optional<int32_t> hardwareBufferSize,
                                           QtMultimediaPrivate::AudioEndpointRole role)
    : PlatformAudioSinkStream(std::move(device), format, ringbufferSize, hardwareBufferSize, volume)
    , m_parent(parent)
{
    Q_UNUSED(role);

    auto *dev = AudioDevicePrivate::handle<PortAudioDevice>(m_audioDevice);
    if (dev)
        m_deviceIndex = dev->deviceIndex();
}

PortAudioSinkStream::~PortAudioSinkStream()
{
    stop(ShutdownPolicy::DiscardRingbuffer);

#ifdef Q_OS_WINDOWS
    if (m_deviceEnumerator && m_notificationClient) {
        m_deviceEnumerator->UnregisterEndpointNotificationCallback(m_notificationClient);
        m_notificationClient->Release();
    }
    if (m_deviceEnumerator) {
        m_deviceEnumerator->Release();
    }
    if (m_workerWakeEvent) {
        CloseHandle(m_workerWakeEvent);
        m_workerWakeEvent = nullptr;
    }
    restoreTimerResolution();
#endif
}

bool PortAudioSinkStream::open()
{
    return openStream();
}

bool PortAudioSinkStream::openStream()
{
    if (m_paStream)
        return true;

    PaSampleFormat paFormat = toPaSampleFormat(m_format.sampleFormat());
    m_nativeFormat = toNativeSampleFormat(paFormat);

    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(m_deviceIndex);
    double suggestedLatency = 0.2;
    if (deviceInfo && deviceInfo->defaultHighOutputLatency > 0) {
        suggestedLatency = qMax(0.2, deviceInfo->defaultHighOutputLatency);
    }

    PaStreamParameters outputParams;
    outputParams.device = m_deviceIndex;
    outputParams.channelCount = m_format.channelCount();
    outputParams.sampleFormat = paFormat;
    outputParams.suggestedLatency = suggestedLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;

    unsigned long framesPerBuffer = paFramesPerBufferUnspecified;
    if (deviceInfo) {
        const PaHostApiInfo *hostApiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
        if (hostApiInfo && hostApiInfo->type == paWASAPI) {
            framesPerBuffer = qMax<unsigned long>(
                1024,
                static_cast<unsigned long>(m_format.sampleRate() * suggestedLatency * 0.5));
        }
    }

    PaError err = Pa_OpenStream(
        &m_paStream,
        nullptr,
        &outputParams,
        m_format.sampleRate(),
        framesPerBuffer,
        paClipOff,
        &PortAudioSinkStream::paCallback,
        this
    );

    if (err != paNoError) {
        qWarning() << "Pa_OpenStream failed:" << Pa_GetErrorText(err);
        return false;
    }

    return true;
}

void PortAudioSinkStream::closeStream()
{
    if (m_paStream) {
        Pa_CloseStream(m_paStream);
        m_paStream = nullptr;
    }
}

bool PortAudioSinkStream::reopenStreamWithNewDevice()
{
    PaDeviceIndex newDevice = Pa_GetDefaultOutputDevice();
    if (newDevice == paNoDevice) {
        qWarning() << "No default output device available";
        return false;
    }

    if (m_paStream) {
        Pa_StopStream(m_paStream);
        Pa_CloseStream(m_paStream);
        m_paStream = nullptr;
    }

    Pa_Sleep(100);

    m_deviceIndex = newDevice;

    if (!openStream()) {
        qWarning() << "=== PortAudio: Failed to open new device ===";
        return false;
    }

    if (!m_suspended.load()) {
        PaError err = Pa_StartStream(m_paStream);
        if (err != paNoError) {
            qWarning() << "Pa_StartStream failed after reopen:" << Pa_GetErrorText(err);
            return false;
        }
    }
    return true;
}

int PortAudioSinkStream::paCallback(const void *input, void *output, unsigned long frameCount,
                                     const PaStreamCallbackTimeInfo *timeInfo,
                                     PaStreamCallbackFlags statusFlags, void *userData)
{
    Q_UNUSED(input);
    Q_UNUSED(timeInfo);
    Q_UNUSED(statusFlags);

    auto *self = static_cast<PortAudioSinkStream *>(userData);

    if (self->m_suspended.load(std::memory_order_relaxed)) {
        std::memset(output, 0, frameCount * self->m_format.bytesPerFrame());
        return paContinue;
    }

    std::span<std::byte> hostBuffer{
        reinterpret_cast<std::byte *>(output),
        static_cast<size_t>(self->m_format.bytesForFrames(frameCount))
    };

    uint64_t consumedFrames = self->process(hostBuffer, frameCount, self->m_nativeFormat);

    if (consumedFrames < frameCount) {
        qsizetype bytesConsumed = self->m_format.bytesForFrames(consumedFrames);
        qsizetype bytesToClear = hostBuffer.size() - bytesConsumed;
        if (bytesToClear > 0) {
            std::memset(static_cast<char *>(output) + bytesConsumed, 0, bytesToClear);
        }
    }

#ifdef Q_OS_WINDOWS
    if (self->m_workerWakeEvent) {
        SetEvent(self->m_workerWakeEvent);
    }
#endif

    return paContinue;
}

void PortAudioSinkStream::joinWorkerThread()
{
    if (m_workerThread && m_workerThread->joinable()) {
        m_workerThread->join();
    }
    m_workerThread.reset();
}

#ifdef Q_OS_WINDOWS

void PortAudioSinkStream::setupDeviceMonitor()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            qWarning() << "=== PortAudio: CoInitializeEx failed ===";
            return;
        }
    }

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&m_deviceEnumerator));

    if (FAILED(hr)) {
        qWarning() << "=== PortAudio: Failed to create MMDeviceEnumerator ===";
        return;
    }

    m_notificationClient = new PortAudioNotificationClient();
    hr = m_deviceEnumerator->RegisterEndpointNotificationCallback(m_notificationClient);

    if (FAILED(hr)) {
        qWarning() << "=== PortAudio: Failed to register notification callback ===";
        m_notificationClient->Release();
        m_notificationClient = nullptr;
    }
}

void PortAudioSinkStream::checkDeviceChange()
{
    if (m_notificationClient) {
        auto *client = static_cast<PortAudioNotificationClient*>(m_notificationClient);
        if (client->deviceChangedFlag().exchange(false)) {
            reopenStreamWithNewDevice();
        }
    }
}

void PortAudioSinkStream::increaseTimerResolution()
{
    if (!m_timerResolutionIncreased) {
        TIMECAPS tc;
        if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR) {
            if (timeBeginPeriod(tc.wPeriodMin) == TIMERR_NOERROR) {
                m_timerResolutionIncreased = true;
            }
        }
    }
}

void PortAudioSinkStream::restoreTimerResolution()
{
    if (m_timerResolutionIncreased) {
        TIMECAPS tc;
        if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR) {
            timeEndPeriod(tc.wPeriodMin);
        }
        m_timerResolutionIncreased = false;
    }
}

#endif

bool PortAudioSinkStream::start(QIODevice *device)
{
    if (!openStream())
        return false;

    setQIODevice(device);
    createQIODeviceConnections(device);

#ifdef Q_OS_WINDOWS
    increaseTimerResolution();
    m_workerWakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
#endif

    const int ringbufferSize = ringbufferSizeInBytes();
    const int targetPreFill = ringbufferSize * 3 / 4;

    for (int i = 0; i < 500 && bytesFree() > 0; ++i) {
        pullFromQIODevice();
        qsizetype filled = ringbufferSize - bytesFree();
        if (filled >= targetPreFill)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    PaError err = Pa_StartStream(m_paStream);
    if (err != paNoError) {
        qWarning() << "Pa_StartStream failed:" << Pa_GetErrorText(err);
        closeStream();
#ifdef Q_OS_WINDOWS
        restoreTimerResolution();
        if (m_workerWakeEvent) {
            CloseHandle(m_workerWakeEvent);
            m_workerWakeEvent = nullptr;
        }
#endif
        return false;
    }

#ifdef Q_OS_WINDOWS
    setupDeviceMonitor();
#endif

    m_workerThread = std::make_unique<std::thread>([this]() {
#ifdef Q_OS_WINDOWS
        while (!isStopRequested(std::memory_order_relaxed)) {
            if (!m_suspended.load(std::memory_order_relaxed)) {
                pullFromQIODevice();
            }
            checkDeviceChange();
            WaitForSingleObject(m_workerWakeEvent, 2);
        }
#else
        while (!isStopRequested(std::memory_order_relaxed)) {
            if (!m_suspended.load(std::memory_order_relaxed)) {
                pullFromQIODevice();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
#endif
    });

    connectIdleHandler([this] {
        if (m_parent)
            m_parent->updateStreamIdle(isIdle());
    });

    return true;
}

QIODevice *PortAudioSinkStream::start()
{
    if (!openStream())
        return nullptr;

    QIODevice *device = createRingbufferWriterDevice();
    setQIODevice(device);
    createQIODeviceConnections(device);

#ifdef Q_OS_WINDOWS
    increaseTimerResolution();
    m_workerWakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
#endif

    PaError err = Pa_StartStream(m_paStream);
    if (err != paNoError) {
        qWarning() << "Pa_StartStream failed:" << Pa_GetErrorText(err);
        closeStream();
#ifdef Q_OS_WINDOWS
        restoreTimerResolution();
        if (m_workerWakeEvent) {
            CloseHandle(m_workerWakeEvent);
            m_workerWakeEvent = nullptr;
        }
#endif
        return nullptr;
    }

#ifdef Q_OS_WINDOWS
    setupDeviceMonitor();

    m_workerThread = std::make_unique<std::thread>([this]() {
        while (!isStopRequested(std::memory_order_relaxed)) {
            checkDeviceChange();
            WaitForSingleObject(m_workerWakeEvent, 100);
        }
    });
#endif

    connectIdleHandler([this] {
        if (m_parent)
            m_parent->updateStreamIdle(isIdle());
    });

    return device;
}

bool PortAudioSinkStream::start(AudioCallback callback)
{
    Q_UNUSED(callback);
    return false;
}

void PortAudioSinkStream::suspend()
{
    m_suspended.store(true);
    if (m_paStream) {
        Pa_StopStream(m_paStream);
    }
}

void PortAudioSinkStream::resume()
{
    m_suspended.store(false);
    if (m_paStream) {
        Pa_StartStream(m_paStream);
    }
}

void PortAudioSinkStream::stop(ShutdownPolicy shutdownPolicy)
{
    m_parent = nullptr;
    m_shutdownPolicy.store(shutdownPolicy);

    requestStop();

    if (m_paStream) {
        if (shutdownPolicy == ShutdownPolicy::DiscardRingbuffer) {
            Pa_AbortStream(m_paStream);
        } else {
            Pa_StopStream(m_paStream);
        }
    }

#ifdef Q_OS_WINDOWS
    if (m_workerWakeEvent) {
        SetEvent(m_workerWakeEvent);
    }
#endif

    m_condition.wakeAll();
    joinWorkerThread();
    closeStream();

#ifdef Q_OS_WINDOWS
    restoreTimerResolution();
    if (m_workerWakeEvent) {
        CloseHandle(m_workerWakeEvent);
        m_workerWakeEvent = nullptr;
    }
#endif
}

void PortAudioSinkStream::updateStreamIdle(bool streamIsIdle)
{
    setIdleState(streamIsIdle);
}

PortAudioSink::PortAudioSink(AudioDevice device, const AudioFormat &format, QObject *parent)
    : BaseClass(std::move(device), format, parent)
{
}

