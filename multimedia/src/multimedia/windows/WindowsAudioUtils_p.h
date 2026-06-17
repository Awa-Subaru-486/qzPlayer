// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_WINDOWS_WINDOWSAUDIOUTILS_P_H
#define QT_WINDOWS_WINDOWSAUDIOUTILS_P_H

#include <QtCore/qstring.h>
#include <QtCore/private/qcomptr_p.h>
#include <QtCore/private/quniquehandle_types_p.h>
#include <qzMultimedia/AudioFormat.h>
#include <qzMultimedia/private/AudioSystem_p.h>

#include <mmreg.h>

#include <chrono>
#include <optional>

struct IAudioClient;
struct IAudioClient3;
struct IMFMediaType;
struct IMMDevice;
typedef LONGLONG REFERENCE_TIME;

class WindowsMediaFoundation;

// Windows 音频工具函数
namespace WindowsAudioUtils
{
using QtMultimediaPrivate::AudioEndpointRole;

// 参考时间类型（100纳秒为单位）
using reference_time = std::chrono::duration<long long, std::ratio<1, 10000000>>;
static_assert(reference_time(1) == std::chrono::nanoseconds(100));

// 格式转换：AudioFormat ↔ WAVEFORMATEXTENSIBLE
bool formatToWaveFormatExtensible(const AudioFormat &format, WAVEFORMATEXTENSIBLE &wfx);
std::optional<WAVEFORMATEXTENSIBLE> toWaveFormatExtensible(const AudioFormat &format);

AudioFormat waveFormatExToFormat(const WAVEFORMATEX &in);
QZ_MULTIMEDIA_EXPORT AudioFormat mediaTypeToFormat(IMFMediaType *mediaType);
ComPtr<IMFMediaType> formatToMediaType(WindowsMediaFoundation &, const AudioFormat &format);
AudioFormat::ChannelConfig maskToChannelConfig(UINT32 mask, int count);

// 音频客户端创建结果
struct AudioClientCreationResult
{
    ComPtr<IAudioClient3> client;
    reference_time periodSize;
    qsizetype audioClientFrames;
};
// 创建音频客户端
std::optional<AudioClientCreationResult>
createAudioClient(const ComPtr<IMMDevice> &, const AudioFormat &,
                  std::optional<qsizetype> hardwareBufferFrames,
                  const QUniqueWin32NullHandle &wasapiEventHandle,
                  std::optional<AudioEndpointRole> = {});

// 音频客户端控制
bool audioClientStart(const ComPtr<IAudioClient3> &);
bool audioClientStop(const ComPtr<IAudioClient3> &);
bool audioClientReset(const ComPtr<IAudioClient3> &);
bool audioClientSetRate(const ComPtr<IAudioClient3> &, int rate);
bool audioClientSetRole(const ComPtr<IAudioClient3> &client, AudioEndpointRole role);

// 获取缓冲区大小
std::optional<quint32> getBufferSizeInFrames(const ComPtr<IAudioClient3> &client);
// 设备周期信息
struct AudioClientDevicePeriod
{
    reference_time defaultDuration;
    reference_time minimalDuration;
};
std::optional<AudioClientDevicePeriod> getDevicePeriod(const ComPtr<IAudioClient3> &client);

// 设置 MCS 周期大小
void setMCSSForPeriodSize(reference_time);

// 错误字符串
QString audioClientErrorString(HRESULT);

}

#endif
