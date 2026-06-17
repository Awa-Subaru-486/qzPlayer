// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_WINDOWS_WINDOWSRESAMPLER_P_H
#define QT_WINDOWS_WINDOWSRESAMPLER_P_H

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/private/qcomptr_p.h>
#include <expected>
#include <qzMultimedia/AudioFormat.h>
#include <qzMultimedia/private/ComInitializer_p.h>
#include <qzMultimedia/private/PlatformAudioResampler_p.h>
#include <qzMultimedia/private/WindowsMediaFoundation_p.h>

#include <mftransform.h>

#include <memory_resource>

struct IMFSample;
struct IMFTransform;

class WindowsMediaFoundation;

// Windows 音频重采样器：基于 Media Foundation Transform 的音频格式转换
class QZ_MULTIMEDIA_EXPORT WindowsResampler : public PlatformAudioResampler
{
public:
    static bool isAvailable();

    WindowsResampler();
    ~WindowsResampler();

    // 设置输入/输出格式
    bool setup(const AudioFormat &in, const AudioFormat &out);
    void setStartTimeOffset(std::chrono::microseconds);

    // 重采样
    QByteArray resample(QByteArray);
    QByteArray resample(const QByteArrayView &);
    QByteArray resample(const ComPtr<IMFSample> &);

    AudioBuffer resample(const char *data, size_t size) override;

    std::pmr::vector<std::byte> resample(std::span<const std::byte>, std::pmr::memory_resource *);

    // 输入/输出格式
    AudioFormat inputFormat() const { return m_inputFormat; }
    AudioFormat outputFormat() const { return m_outputFormat; }

    // 缓冲区大小计算
    quint64 outputBufferSize(quint64 inputBufferSize) const;
    quint64 inputBufferSize(quint64 outputBufferSize) const;

    // 总处理字节数
    quint64 totalInputBytes() const { return m_totalInputBytes; }
    quint64 totalOutputBytes() const { return m_totalOutputBytes; }

private:
    qsizetype overAllocatedOutputBufferSize();
    template <typename Functor>
    auto processOutput(ComPtr<IMFMediaBuffer> buffer, Functor &&f)
            -> std::invoke_result_t<Functor, const ComPtr<IMFMediaBuffer> &>;

    std::expected<QByteArray, HRESULT> processOutput();

    ComInitializer m_comInitializer;
    WindowsMediaFoundation *m_wmf{ WindowsMediaFoundation::instance() };
    MFRuntimeInit m_wmfRuntime{ m_wmf };
    ComPtr<IMFTransform> m_resampler;
    ComPtr<IMFSample> m_inputSample;
    ComPtr<IMFSample> m_outputSample;

    quint64 m_totalInputBytes = 0;
    quint64 m_totalOutputBytes = 0;
    AudioFormat m_inputFormat;
    AudioFormat m_outputFormat;

    DWORD m_inputStreamID = 0;

    std::chrono::microseconds m_startTimeOffset;
};

#endif
