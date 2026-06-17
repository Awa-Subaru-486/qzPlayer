// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#include "WmfSupport_p.h"

#include <mferror.h>

namespace WMF {

HRESULT ByteArrayMFMediaBuffer::CreateInstance(QByteArray data, IMFMediaBuffer **ppBuffer,
                                                bool isReadOnly)
{
    if (!ppBuffer)
        return E_POINTER;

    DWORD size = data.size();

    ByteArrayMFMediaBuffer *pBuffer =
            new (std::nothrow) ByteArrayMFMediaBuffer(std::move(data), isReadOnly);
    if (!pBuffer)
        return E_OUTOFMEMORY;

    pBuffer->SetCurrentLength(size);

    HRESULT hr =
            pBuffer->QueryInterface(__uuidof(IMFMediaBuffer), reinterpret_cast<void **>(ppBuffer));

    pBuffer->Release();
    return hr;
}

HRESULT ByteArrayMFMediaBuffer::CreateInstance(qsizetype capacity, IMFMediaBuffer **ppBuffer)
{
    if (!ppBuffer)
        return E_POINTER;

    QByteArray buffer{ capacity, Qt::Initialization::Uninitialized };
    ByteArrayMFMediaBuffer *pBuffer =
            new (std::nothrow) ByteArrayMFMediaBuffer(std::move(buffer), false);
    if (!pBuffer)
        return E_OUTOFMEMORY;

    HRESULT hr =
            pBuffer->QueryInterface(__uuidof(IMFMediaBuffer), reinterpret_cast<void **>(ppBuffer));

    pBuffer->Release();
    return hr;
}

HRESULT ByteArrayMFMediaBuffer::Lock(BYTE **ppbBuffer, DWORD *pcbMaxLength,
                                      DWORD *pcbCurrentLength)
{
    if (!ppbBuffer)
        return E_POINTER;

    if (m_isLocked.test_and_set(std::memory_order_acquire))
        return MF_E_INVALIDREQUEST;

    if (m_isReadOnly)

        *ppbBuffer = const_cast<BYTE *>(reinterpret_cast<const BYTE *>(m_byteArray.constData()));
    else
        *ppbBuffer = reinterpret_cast<BYTE *>(m_byteArray.data());

    if (pcbMaxLength)
        *pcbMaxLength = GetMaxLengthInternal();

    if (pcbCurrentLength)
        *pcbCurrentLength = m_currentLength;

    return S_OK;
}

HRESULT ByteArrayMFMediaBuffer::Unlock()
{
    m_isLocked.clear(std::memory_order_release);
    return S_OK;
}

HRESULT ByteArrayMFMediaBuffer::GetCurrentLength(DWORD *pcbCurrentLength)
{
    if (!pcbCurrentLength)
        return E_POINTER;

    *pcbCurrentLength = m_currentLength;
    return S_OK;
}

HRESULT ByteArrayMFMediaBuffer::SetCurrentLength(DWORD cbCurrentLength)
{
    if (cbCurrentLength > GetMaxLengthInternal())
        return E_INVALIDARG;

    m_currentLength = cbCurrentLength;
    return S_OK;
}

HRESULT ByteArrayMFMediaBuffer::GetMaxLength(DWORD *pcbMaxLength)
{
    if (!pcbMaxLength)
        return E_POINTER;

    *pcbMaxLength = GetMaxLengthInternal();
    return S_OK;
}

ByteArrayMFMediaBuffer::ByteArrayMFMediaBuffer(QByteArray &&data, bool isReadOnly)
    : m_byteArray(std::move(data)), m_isReadOnly(isReadOnly)
{
}

DWORD ByteArrayMFMediaBuffer::GetMaxLengthInternal() const
{
    return static_cast<DWORD>(m_byteArray.size());
}

QByteArray ByteArrayMFMediaBuffer::takeByteArray()
{
    return std::move(m_byteArray);
}

HRESULT PmrMediaBuffer::CreateInstance(std::span<const std::byte> data,
                                        std::pmr::memory_resource *resource,
                                        IMFMediaBuffer **ppBuffer)
{
    if (!ppBuffer || !resource)
        return E_POINTER;

    *ppBuffer = nullptr;

    auto *buffer = resource->allocate(sizeof(PmrMediaBuffer), alignof(PmrMediaBuffer));
    if (!buffer)
        return E_OUTOFMEMORY;

    PmrMediaBuffer *pBuffer = new (buffer) PmrMediaBuffer(data, resource);

    HRESULT hr =
            pBuffer->QueryInterface(__uuidof(IMFMediaBuffer), reinterpret_cast<void **>(ppBuffer));

    pBuffer->Release();

    return hr;
}

HRESULT PmrMediaBuffer::CreateInstance(qsizetype capacity, std::pmr::memory_resource *resource,
                                        IMFMediaBuffer **ppBuffer)
{
    if (!ppBuffer || !resource)
        return E_POINTER;

    *ppBuffer = nullptr;
    void *buffer = resource->allocate(sizeof(PmrMediaBuffer), alignof(PmrMediaBuffer));
    PmrMediaBuffer *pBuffer = new (buffer) PmrMediaBuffer(capacity, resource);
    if (!pBuffer)
        return E_OUTOFMEMORY;

    HRESULT hr =
            pBuffer->QueryInterface(__uuidof(IMFMediaBuffer), reinterpret_cast<void **>(ppBuffer));
    pBuffer->Release();
    return hr;
}

ULONG PmrMediaBuffer::AddRef()
{
    return m_referenceCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG PmrMediaBuffer::Release()
{
    const LONG referenceCount = m_referenceCount.fetch_sub(1, std::memory_order_release) - 1;
    if (referenceCount == 0) {

        std::atomic_thread_fence(std::memory_order_acquire);

        std::pmr::memory_resource *resource = m_memoryResource;

        this->~PmrMediaBuffer();
        resource->deallocate(this, sizeof(PmrMediaBuffer), alignof(PmrMediaBuffer));
    }

    return referenceCount;
}

STDMETHODIMP PmrMediaBuffer::Lock(BYTE **ppbBuffer, DWORD *pcbMaxLength, DWORD *pcbCurrentLength)
{
    if (!ppbBuffer)
        return E_POINTER;

    if (m_isLocked.test_and_set(std::memory_order_acquire))
        return MF_E_INVALIDREQUEST;

    *ppbBuffer = reinterpret_cast<BYTE *>(m_buffer);

    if (pcbMaxLength)
        *pcbMaxLength = m_maxLength;

    if (pcbCurrentLength)
        *pcbCurrentLength = m_currentLength;

    return S_OK;
}

STDMETHODIMP PmrMediaBuffer::Unlock()
{
    m_isLocked.clear(std::memory_order_release);
    return S_OK;
}

STDMETHODIMP PmrMediaBuffer::GetCurrentLength(DWORD *pcbCurrentLength)
{
    if (!pcbCurrentLength)
        return E_POINTER;

    *pcbCurrentLength = m_currentLength;
    return S_OK;
}

STDMETHODIMP PmrMediaBuffer::SetCurrentLength(DWORD cbCurrentLength)
{
    if (cbCurrentLength > m_maxLength)
        return E_INVALIDARG;

    m_currentLength = cbCurrentLength;
    return S_OK;
}

STDMETHODIMP PmrMediaBuffer::GetMaxLength(DWORD *pcbMaxLength)
{
    if (!pcbMaxLength)
        return E_POINTER;

    *pcbMaxLength = m_maxLength;
    return S_OK;
}

static constexpr auto mfBufferAlignment = 16;

PmrMediaBuffer::PmrMediaBuffer(std::span<const std::byte> data, std::pmr::memory_resource *resource)
    : PmrMediaBuffer(data.size(), resource)
{
    m_currentLength = data.size(), std::copy(data.begin(), data.end(), m_buffer);
}

PmrMediaBuffer::PmrMediaBuffer(qsizetype capacity, std::pmr::memory_resource *resource)
    : m_memoryResource(resource),
      m_maxLength(DWORD(capacity)),
      m_buffer(static_cast<std::byte *>(resource->allocate(capacity, mfBufferAlignment)))
{
}

PmrMediaBuffer::~PmrMediaBuffer()
{
    m_memoryResource->deallocate(m_buffer, m_maxLength, mfBufferAlignment);
}

}

