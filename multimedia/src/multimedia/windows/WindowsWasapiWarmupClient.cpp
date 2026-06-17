// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsWasapiWarmupClient_p.h"

#include <QtCore/qapplicationstatic.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <span>
#include <QtCore/qthread.h>
#include <QtCore/qtimer.h>
#include <QtCore/quuid.h>
#include <QtCore/private/qsystemerror_p.h>
#include <qzMultimedia/private/ComInitializer_p.h>
#include <qzMultimedia/private/ComTaskResource_p.h>
#include <qzMultimedia/private/WindowsAudioUtils_p.h>

#include <audioclient.h>
#include <guiddef.h>
#include <mmdeviceapi.h>
#include <powrprof.h>

namespace QtMultimediaPrivate {

using namespace std::chrono_literals;

namespace {

class WasapiWarmupClient
{
public:
    WasapiWarmupClient();
    ~WasapiWarmupClient();

private:
    ComPtr<IAudioClient3> m_warmupClient;
};

WasapiWarmupClient::WasapiWarmupClient()
{
    using namespace WindowsAudioUtils;
    ensureComInitializedOnThisThread();

    ComPtr<IMMDeviceEnumerator> deviceEnumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&deviceEnumerator));
    if (FAILED(hr)) {
        qWarning() << "Failed to create device enumerator" << audioClientErrorString(hr);
        return;
    }

    ComPtr<IMMDevice> device;
    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.GetAddressOf());
    if (FAILED(hr)) {
        if (hr != E_NOTFOUND)
            qWarning() << "Failed to retrieve default audio endpoint" << audioClientErrorString(hr);
        return;
    }

    hr = device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void **>(m_warmupClient.GetAddressOf()));
    if (FAILED(hr)) {
        qWarning() << "Failed to activate audio engine" << audioClientErrorString(hr);
        return;
    }

    ComTaskResource<WAVEFORMATEX> deviceFormat;
    UINT32 currentPeriodInFrames = 0;
    hr = m_warmupClient->GetCurrentSharedModeEnginePeriod(deviceFormat.address(),
                                                          &currentPeriodInFrames);
    if (FAILED(hr)) {
        qWarning() << "Failed to retrieve the current format and periodicity of the audio engine"
                   << audioClientErrorString(hr);
        return;
    }

    UINT32 defaultPeriodInFrames = 0;
    UINT32 fundamentalPeriodInFrames = 0;
    UINT32 minPeriodInFrames = 0;
    UINT32 maxPeriodInFrames = 0;
    hr = m_warmupClient->GetSharedModeEnginePeriod(deviceFormat.get(), &defaultPeriodInFrames,
                                                   &fundamentalPeriodInFrames, &minPeriodInFrames,
                                                   &maxPeriodInFrames);
    if (FAILED(hr)) {
        qWarning() << "Failed to retrieve the range of periodicities supported by the audio engine"
                   << audioClientErrorString(hr);
        return;
    }

    audioClientSetRole(m_warmupClient, QtMultimediaPrivate::AudioEndpointRole::SoundEffect);

    hr = m_warmupClient->InitializeSharedAudioStream(
            AUDCLNT_SHAREMODE_SHARED, defaultPeriodInFrames, deviceFormat.get(), nullptr);
    if (FAILED(hr)) {
        qWarning() << "Failed to initialize audio engine stream" << audioClientErrorString(hr);
        return;
    }

    hr = m_warmupClient->Start();
    if (FAILED(hr))
        qWarning() << "Failed to start audio engine" << audioClientErrorString(hr);
}

WasapiWarmupClient::~WasapiWarmupClient()
{
    using namespace WindowsAudioUtils;

    if (m_warmupClient) {
        HRESULT hr = m_warmupClient->Stop();
        if (FAILED(hr))
            qWarning() << "Failed to stop audio engine" << audioClientErrorString(hr);
    }
}

}

DEFINE_GUID(GUID_SLEEP_TIMEOUT, 0x29f6c1db, 0x86da, 0x48c5, 0x9f, 0xdb, 0xf2, 0xb6, 0x7b, 0x1f,
            0x44, 0xda);

class SleepTimeoutMonitor : public QObject
{
    Q_OBJECT

public:
    SleepTimeoutMonitor();
    ~SleepTimeoutMonitor();

signals:
    void sleepTimeoutChanged(std::chrono::seconds);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT handlePowerSettingsChanged(HWND hwnd, POWERBROADCAST_SETTING *pbs);

    std::wstring m_windowClass;
    HWND m_offscreenWindow{};
    HPOWERNOTIFY m_powerNotifySleepTimeout;
};

SleepTimeoutMonitor::SleepTimeoutMonitor()
{
    m_windowClass = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString()
            + L"PowerNotificationWindowClass";

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = nullptr;
    wc.lpszClassName = m_windowClass.c_str();
    wc.hCursor = nullptr;

    if (!RegisterClassEx(&wc)) {
        qWarning() << "RegisterClassEx failed" << QSystemError::windowsString();
        m_windowClass.clear();
        return;
    }

    m_offscreenWindow = CreateWindowEx(0, m_windowClass.c_str(), L"Power Notification Window", 0, 0,
                                       0, 0, 0, HWND_MESSAGE, nullptr, nullptr, this);

    if (!m_offscreenWindow) {
        qWarning() << "CreateWindowEx failed" << QSystemError::windowsString();
        return;
    }

    SetWindowLongPtr(m_offscreenWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    m_powerNotifySleepTimeout = RegisterPowerSettingNotification(
            m_offscreenWindow, &GUID_SLEEP_TIMEOUT, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!m_powerNotifySleepTimeout) {
        qWarning() << "RegisterPowerSettingNotification failed" << QSystemError::windowsString();
    }
}

SleepTimeoutMonitor::~SleepTimeoutMonitor()
{
    if (m_powerNotifySleepTimeout)
        UnregisterPowerSettingNotification(m_powerNotifySleepTimeout);

    if (m_offscreenWindow)
        DestroyWindow(m_offscreenWindow);
    if (!m_windowClass.empty())
        UnregisterClass(m_windowClass.c_str(), GetModuleHandle(nullptr));
}

LRESULT SleepTimeoutMonitor::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_POWERBROADCAST: {
        switch (wParam) {
        case PBT_POWERSETTINGCHANGE: {
            POWERBROADCAST_SETTING *pbs = reinterpret_cast<POWERBROADCAST_SETTING *>(lParam);
            return handlePowerSettingsChanged(hwnd, pbs);
        }
        default:
            break;
        }
        break;
    }
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

LRESULT SleepTimeoutMonitor::handlePowerSettingsChanged(HWND hwnd, POWERBROADCAST_SETTING *pbs)
{
    if (pbs->PowerSetting == GUID_SLEEP_TIMEOUT) {
        Q_ASSERT(pbs->DataLength == 4);
        union UnpackUnion {
            UCHAR data[4];
            DWORD sleepTimeoutInSeconds;
        } helper;

        std::copy_n(pbs->Data, pbs->DataLength, helper.data);
        auto self = reinterpret_cast<SleepTimeoutMonitor *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        emit self->sleepTimeoutChanged(std::chrono::seconds{ helper.sleepTimeoutInSeconds });
    }
    return 0;
}

namespace {

class WasapiWarmupClientHelper
{
public:
    WasapiWarmupClientHelper()
    {
        m_cleanupTimer.setInterval(120s);
        m_cleanupTimer.setTimerType(Qt::TimerType::VeryCoarseTimer);
        m_cleanupTimer.setSingleShot(true);
        m_cleanupTimer.callOnTimeout(&m_cleanupTimer, [this] {
            m_warmupClient = {};
        });

        if (!QThread::isMainThread())
            m_cleanupTimer.moveToThread(qApp->thread());

        QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [this] {
            m_warmupClient = {};
        });

        QObject::connect(&m_sleepTimeoutMonitor, &SleepTimeoutMonitor::sleepTimeoutChanged,
                         &m_cleanupTimer, [this](std::chrono::seconds sleepTimeout) {
            m_cleanupTimer.setInterval(std::min<std::chrono::milliseconds>(sleepTimeout / 2, 5min));
        });
    }

    void refresh()
    {
        Q_ASSERT(QThread::isMainThread());

        if (!m_warmupClient)
            m_warmupClient = std::make_unique<WasapiWarmupClient>();

        m_cleanupTimer.start();
    }

private:
    QTimer m_cleanupTimer;
    SleepTimeoutMonitor m_sleepTimeoutMonitor;
    std::unique_ptr<WasapiWarmupClient> m_warmupClient;
};

Q_APPLICATION_STATIC(WasapiWarmupClientHelper, warmupClient);

}

void refreshWarmupClient()
{
    const static bool useWarmupClient = [] {
        bool envVarSet = false;
        int disableWarmup = qEnvironmentVariableIntValue("QT_DISABLE_AUDIO_PREPARE", &envVarSet);
        return !envVarSet || disableWarmup == 0;
    }();

    if (!useWarmupClient)
        return;

    QMetaObject::invokeMethod(qApp, [] {
        warmupClient->refresh();
    });
}

}

#include "WindowsWasapiWarmupClient.moc"
