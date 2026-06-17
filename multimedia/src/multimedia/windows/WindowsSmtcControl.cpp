// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsSmtcControl_p.h"

import qzLog;
#include <vector>
#include <QCoreApplication>

// COM Interop（桌面应用入口）
#include <windows.h>
#include <initguid.h>
#include <inspectable.h>
#include <activation.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <propsys.h>
#include <propkey.h>
#include <shellapi.h>
#include <SystemMediaTransportControlsInterop.h>

// C++/WinRT 投影
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>

using namespace winrt;

using SMTC = winrt::Windows::Media::SystemMediaTransportControls;
using SMTCButton = winrt::Windows::Media::SystemMediaTransportControlsButton;
using SMTCButtonPressedEventArgs =
    winrt::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs;
using MediaPlaybackStatus = winrt::Windows::Media::MediaPlaybackStatus;
using MediaPlaybackType = winrt::Windows::Media::MediaPlaybackType;
using TimelineProperties = winrt::Windows::Media::SystemMediaTransportControlsTimelineProperties;
using RandomAccessStreamReference = winrt::Windows::Storage::Streams::RandomAccessStreamReference;
using InMemoryRandomAccessStream = winrt::Windows::Storage::Streams::InMemoryRandomAccessStream;
using BitmapEncoder = winrt::Windows::Graphics::Imaging::BitmapEncoder;
using BitmapPixelFormat = winrt::Windows::Graphics::Imaging::BitmapPixelFormat;
using BitmapAlphaMode = winrt::Windows::Graphics::Imaging::BitmapAlphaMode;

static qz::Log::LogCategory qLcSmtc("qz.multimedia.smtc");

struct WindowsSmtcControl::SmtcImpl
{
    SMTC smtc{ nullptr };
    winrt::event_token buttonPressedToken{};
};

WindowsSmtcControl::WindowsSmtcControl(QObject *parent)
    : QObject(parent), m_impl(std::make_unique<SmtcImpl>())
{
}

WindowsSmtcControl::~WindowsSmtcControl()
{
    if (m_initialized && m_impl && m_impl->smtc) {
        m_impl->smtc.ButtonPressed(m_impl->buttonPressedToken);
        m_impl->smtc.DisplayUpdater().ClearAll();
        m_impl->smtc.PlaybackStatus(MediaPlaybackStatus::Closed);
        m_impl->smtc.IsEnabled(false);
        m_impl->smtc = nullptr;
    }
    // 清理 MTA（在工作线程中析构时调用）
    if (m_initialized)
        uninit_apartment();
}

void WindowsSmtcControl::init(WId windowId)
{
    if (m_initialized)
        return;

    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!hwnd) {
        qz::Log::cat_warn(qLcSmtc, "Cannot get window handle");
        return;
    }

    // 使用 applicationName 设置窗口的 AppUserModelID，SMTC 会读取此值显示应用名称
    const auto appName = QCoreApplication::applicationName();
    if (!appName.isEmpty()) {
        IPropertyStore *store = nullptr;
        HRESULT hr = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store));
        if (SUCCEEDED(hr) && store) {
            PROPVARIANT pv{};
            pv.vt = VT_LPWSTR;
            const auto wstr = new wchar_t[appName.size() + 1];
            appName.toWCharArray(wstr);
            wstr[appName.size()] = L'\0';
            pv.pwszVal = wstr;
            store->SetValue(PKEY_AppUserModel_ID, pv);
            store->Commit();
            store->Release();
            pv.pwszVal = nullptr; // 防止 PropVariantClear 释放我们管理的内存
            delete[] wstr;
        }
    }

    // 在工作线程中初始化 MTA，避免与 Qt 主线程的 STA 冲突
    try {
        init_apartment(apartment_type::multi_threaded);
    } catch (const winrt::hresult_error &e) {
        qz::Log::cat_warn(qLcSmtc, "init_apartment(MTA) failed: {:x}", static_cast<unsigned>(e.code().value));
        return;
    }

    // COM Interop: 获取 SMTC
    Microsoft::WRL::ComPtr<IInspectable> factoryInspectable;
    Microsoft::WRL::Wrappers::HStringReference classNameRef(
        L"Windows.Media.SystemMediaTransportControls");
    HRESULT hr = RoGetActivationFactory(classNameRef.Get(), __uuidof(IInspectable),
                                        &factoryInspectable);
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcSmtc, "RoGetActivationFactory failed: {:x}", static_cast<unsigned>(hr));
        return;
    }

    ISystemMediaTransportControlsInterop *interop = nullptr;
    hr = factoryInspectable->QueryInterface(__uuidof(ISystemMediaTransportControlsInterop),
                                            (void **)&interop);
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcSmtc, "QI for Interop failed: {:x}", static_cast<unsigned>(hr));
        return;
    }

    IInspectable *smtcInspectable = nullptr;
    hr = interop->GetForWindow(hwnd, __uuidof(IInspectable), (void **)&smtcInspectable);
    interop->Release();
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcSmtc, "GetForWindow failed: {:x}", static_cast<unsigned>(hr));
        return;
    }

    // 桥接到 C++/WinRT
    winrt::copy_from_abi(m_impl->smtc, smtcInspectable);
    smtcInspectable->Release();

    if (!m_impl->smtc) {
        qz::Log::cat_warn(qLcSmtc, "SMTC is null after copy_from_abi");
        return;
    }

    // 配置按钮
    auto &smtc = m_impl->smtc;
    smtc.IsPlayEnabled(true);
    smtc.IsPauseEnabled(true);
    smtc.IsStopEnabled(true);
    smtc.IsNextEnabled(true);
    smtc.IsPreviousEnabled(true);
    smtc.IsEnabled(true);

    // 注册按钮事件
    m_impl->buttonPressedToken = smtc.ButtonPressed(
        [this](SMTC const &, SMTCButtonPressedEventArgs const &args) {
            switch (args.Button()) {
            case SMTCButton::Play:     emit playRequested();     break;
            case SMTCButton::Pause:    emit pauseRequested();    break;
            case SMTCButton::Stop:     emit stopRequested();     break;
            case SMTCButton::Next:     emit nextRequested();     break;
            case SMTCButton::Previous: emit previousRequested(); break;
            default: break;
            }
        });

    m_initialized = true;
    qz::Log::cat_debug(qLcSmtc, "SMTC initialized successfully in worker thread");
}

void WindowsSmtcControl::setPlaybackState(MediaPlayer::PlaybackState state)
{
    if (!m_initialized || !m_impl->smtc)
        return;

    MediaPlaybackStatus status = MediaPlaybackStatus::Closed;
    switch (state) {
    case MediaPlayer::PlayingState: status = MediaPlaybackStatus::Playing; break;
    case MediaPlayer::PausedState:  status = MediaPlaybackStatus::Paused;  break;
    case MediaPlayer::StoppedState: status = MediaPlaybackStatus::Stopped; break;
    }
    m_impl->smtc.PlaybackStatus(status);
}

void WindowsSmtcControl::updateMetadata(const MediaMetaData &metaData, bool hasVideo)
{
    if (!m_initialized || !m_impl->smtc)
        return;

    auto updater = m_impl->smtc.DisplayUpdater();
    updater.ClearAll();

    if (hasVideo) {
        updater.Type(MediaPlaybackType::Video);
        auto props = updater.VideoProperties();
        if (metaData.keys().contains(MediaMetaData::Title))
            props.Title(reinterpret_cast<const wchar_t *>(
                metaData.stringValue(MediaMetaData::Title).utf16()));
    } else {
        updater.Type(MediaPlaybackType::Music);
        auto props = updater.MusicProperties();
        if (metaData.keys().contains(MediaMetaData::Title))
            props.Title(reinterpret_cast<const wchar_t *>(
                metaData.stringValue(MediaMetaData::Title).utf16()));
        if (metaData.keys().contains(MediaMetaData::Author)
            || metaData.keys().contains(MediaMetaData::ContributingArtist)) {
            auto artist = metaData.keys().contains(MediaMetaData::ContributingArtist)
                              ? metaData.stringValue(MediaMetaData::ContributingArtist)
                              : metaData.stringValue(MediaMetaData::Author);
            props.Artist(reinterpret_cast<const wchar_t *>(artist.utf16()));
        }
        if (metaData.keys().contains(MediaMetaData::AlbumArtist))
            props.AlbumArtist(reinterpret_cast<const wchar_t *>(
                metaData.stringValue(MediaMetaData::AlbumArtist).utf16()));
        if (metaData.keys().contains(MediaMetaData::AlbumTitle))
            props.AlbumTitle(reinterpret_cast<const wchar_t *>(
                metaData.stringValue(MediaMetaData::AlbumTitle).utf16()));
        if (metaData.keys().contains(MediaMetaData::TrackNumber))
            props.TrackNumber(metaData.value(MediaMetaData::TrackNumber).toInt());
    }

    updater.Update();
}

void WindowsSmtcControl::updateThumbnail(const QImage &image) const
{
    if (!m_initialized || !m_impl->smtc || image.isNull())
        return;

    try {
        // QImage -> BGRA 像素数据 -> InMemoryRandomAccessStream -> BitmapEncoder -> PNG
        QImage img = image.convertToFormat(QImage::Format_ARGB32);
        const int width = img.width();
        const int height = img.height();
        std::vector<uint8_t> pixels(width * height * 4);
        const uint8_t *src = img.constBits();
        for (int i = 0; i < width * height; ++i) {
            pixels[i * 4 + 0] = src[i * 4 + 0]; // B
            pixels[i * 4 + 1] = src[i * 4 + 1]; // G
            pixels[i * 4 + 2] = src[i * 4 + 2]; // R
            pixels[i * 4 + 3] = src[i * 4 + 3]; // A
        }

        auto stream = InMemoryRandomAccessStream();
        auto encoder = BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), stream).get();
        encoder.SetPixelData(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied,
                             width, height, 96.0, 96.0, pixels);
        encoder.FlushAsync().get();

        stream.Seek(0);
        auto streamRef = RandomAccessStreamReference::CreateFromStream(stream);

        auto updater = m_impl->smtc.DisplayUpdater();
        updater.Thumbnail(streamRef);
        updater.Update();
    } catch (const winrt::hresult_error &e) {
        qz::Log::cat_warn(qLcSmtc, "updateThumbnail failed: {}", e.code().value);
    }
}

void WindowsSmtcControl::clearMetadata()
{
    if (!m_initialized || !m_impl->smtc)
        return;
    m_impl->smtc.DisplayUpdater().ClearAll();
    m_impl->smtc.DisplayUpdater().Update();
}

void WindowsSmtcControl::setPosition(qint64 positionMs)
{
    if (!m_initialized || !m_impl->smtc)
        return;
    TimelineProperties timeline;
    timeline.Position(winrt::Windows::Foundation::TimeSpan{ positionMs * 10000 });
    m_impl->smtc.UpdateTimelineProperties(timeline);
}

void WindowsSmtcControl::setDuration(qint64 durationMs)
{
    if (!m_initialized || !m_impl->smtc)
        return;
    TimelineProperties timeline;
    timeline.StartTime(winrt::Windows::Foundation::TimeSpan{ 0 });
    timeline.EndTime(winrt::Windows::Foundation::TimeSpan{ durationMs * 10000 });
    timeline.MinSeekTime(winrt::Windows::Foundation::TimeSpan{ 0 });
    timeline.MaxSeekTime(winrt::Windows::Foundation::TimeSpan{ durationMs * 10000 });
    m_impl->smtc.UpdateTimelineProperties(timeline);
}
