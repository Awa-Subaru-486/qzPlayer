// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_WINDOWS_WMFSUPPORT_P_H
#define QT_WINDOWS_WMFSUPPORT_P_H
#include <qzMultimedia/qtmultimediaexports.h>
#include <QtCore/qtconfigmacros.h>
#include <span>
#include <QtCore/qbytearray.h>
#include <expected>
#include <QtCore/private/qcomobject_p.h>
#include <QtCore/private/qcomptr_p.h>

#include <mfobjects.h>

#include <memory_resource>

// WMF 支持工具：Media Foundation 缓冲区操作辅助
namespace WMF {

template <typename Functor>
using IMFBufferReaderReturnType = std::invoke_result_t<Functor, std::span<BYTE>, std::span<BYTE>>;

// 锁定并访问 IMFMediaBuffer
template <typename Functor>
[[nodiscard]]
auto withLockedBuffer(IMFMediaBuffer *buffer, Functor &&f)
        -> std::expected<IMFBufferReaderReturnType<Functor>, HRESULT>
{
    if (!buffer)
        return std::unexpected{ E_POINTER };

    BYTE *data = nullptr;
    DWORD maxLength = 0;
    DWORD currentLength = 0;

    HRESULT hr = buffer->Lock(&data, &maxLength, &currentLength);
    if (FAILED(hr))
        return std::unexpected{ hr };

    auto unlockGuard = qScopeGuard([buffer]() {
        buffer->Unlock();
    });

    if constexpr (std::is_void_v<IMFBufferReaderReturnType<Functor>>) {
        f(std::span{ data, static_cast<size_t>(currentLength) }, std::span{ data, static_cast<size_t>(maxLength) });
        return {};
    } else
        return f(std::span{ data, static_cast<size_t>(currentLength) }, std::span{ data, static_cast<size_t>(maxLength) });
}

template <typename Functor>
[[nodiscard]]
auto withLockedBuffer(const ComPtr<IMFMediaBuffer> &buffer, Functor &&f)
        -> std::expected<IMFBufferReaderReturnType<Functor>, HRESULT>
{
    return withLockedBuffer(buffer.Get(), std::forward<Functor>(f));
}

// QByteArray 支持的 IMFMediaBuffer 实现
class ByteArrayMFMediaBuffer final : public QComObject<IMFMediaBuffer>
{
public:
    static HRESULT CreateInstance(QByteArray data, IMFMediaBuffer **ppBuffer,
                                  bool isReadOnly = false);
    static HRESULT CreateInstance(qsizetype capacity, IMFMediaBuffer **ppBuffer);

    STDMETHODIMP Lock(BYTE **ppbBuffer, DWORD *pcbMaxLength, DWORD *pcbCurrentLength) override;
    STDMETHODIMP Unlock() override;
    STDMETHODIMP GetCurrentLength(DWORD *pcbCurrentLength) override;
    STDMETHODIMP SetCurrentLength(DWORD cbCurrentLength) override;
    STDMETHODIMP GetMaxLength(DWORD *pcbMaxLength) override;

    // 取出内部 QByteArray
    QByteArray takeByteArray();

private:

    explicit ByteArrayMFMediaBuffer(QByteArray &&data, bool isReadOnly);
    ~ByteArrayMFMediaBuffer() = default;

    DWORD GetMaxLengthInternal() const;

    std::atomic_flag m_isLocked = ATOMIC_FLAG_INIT;
    DWORD m_currentLength{ 0 };
    QByteArray m_byteArray;
    const bool m_isReadOnly{};

public:
    ByteArrayMFMediaBuffer(const ByteArrayMFMediaBuffer &) = delete;
    ByteArrayMFMediaBuffer &operator=(const ByteArrayMFMediaBuffer &) = delete;
    ByteArrayMFMediaBuffer(ByteArrayMFMediaBuffer &&) = delete;
    ByteArrayMFMediaBuffer &operator=(ByteArrayMFMediaBuffer &&) = delete;
};

// PMR 内存资源支持的 IMFMediaBuffer 实现
class PmrMediaBuffer final : public QComObject<IMFMediaBuffer>
{
public:
    static HRESULT CreateInstance(std::span<const std::byte>, std::pmr::memory_resource *,
                                  IMFMediaBuffer **ppBuffer);
    static HRESULT CreateInstance(qsizetype capacity, std::pmr::memory_resource *,
                                  IMFMediaBuffer **ppBuffer);

    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP Lock(BYTE **ppbBuffer, DWORD *pcbMaxLength, DWORD *pcbCurrentLength) override;
    STDMETHODIMP Unlock() override;
    STDMETHODIMP GetCurrentLength(DWORD *pcbCurrentLength) override;
    STDMETHODIMP SetCurrentLength(DWORD cbCurrentLength) override;
    STDMETHODIMP GetMaxLength(DWORD *pcbMaxLength) override;

private:

    PmrMediaBuffer(std::span<const std::byte>, std::pmr::memory_resource *);
    PmrMediaBuffer(qsizetype capacity, std::pmr::memory_resource *);
    ~PmrMediaBuffer();

    std::atomic_flag m_isLocked = ATOMIC_FLAG_INIT;
    std::pmr::memory_resource *m_memoryResource;

    DWORD m_currentLength{ 0 };
    DWORD m_maxLength;
    std::byte *const m_buffer;

    std::atomic<LONG> m_referenceCount = 1;

public:
    Q_DISABLE_COPY_MOVE(PmrMediaBuffer)
};

}

#endif
