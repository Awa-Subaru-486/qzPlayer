// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsPropertystore_p.h"

#include <QtCore/qscopeguard.h>
#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>
#include <QtCore/private/qsystemerror_p.h>
#include <qzMultimedia/private/ComTaskResource_p.h>

#include <mmdeviceapi.h>
#include <propsys.h>
#include <propvarutil.h>

namespace QtMultimediaPrivate {

PropertyStoreHelper::PropertyStoreHelper(ComPtr<IPropertyStore> props) : m_props(std::move(props))
{
}

std::expected<PropertyStoreHelper, QString>
PropertyStoreHelper::open(const ComPtr<IMMDevice> &device)
{
    ComPtr<IPropertyStore> props;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, props.GetAddressOf());
    if (!SUCCEEDED(hr)) {
        return std::unexpected{ QSystemError::windowsComString(hr) };
    }
    return PropertyStoreHelper(std::move(props));
}

std::optional<QString> PropertyStoreHelper::getString(const PROPERTYKEY &property)
{
    PROPVARIANT variant;
    PropVariantInit(&variant);

    auto cleanup = qScopeGuard([&] {
        PropVariantClear(&variant);
    });

    if (!SUCCEEDED(m_props->GetValue(property, &variant)))
        return std::nullopt;

    ComTaskResource<WCHAR> str;
    HRESULT hr = PropVariantToStringAlloc(variant, str.address());
    if (SUCCEEDED(hr))
        return QString::fromWCharArray(variant.pwszVal);

    qWarning() << "PropertyStoreHelper::getString: PropVariantToStringAlloc failed"
               << QSystemError::windowsComString(hr);
    return std::nullopt;
}

std::optional<uint32_t> PropertyStoreHelper::getUInt32(const PROPERTYKEY &property)
{
    PROPVARIANT variant;
    PropVariantInit(&variant);

    if (!SUCCEEDED(m_props->GetValue(property, &variant)))
        return std::nullopt;

    ULONG ret;
    HRESULT hr = PropVariantToUInt32(variant, &ret);
    if (SUCCEEDED(hr))
        return uint32_t{ ret };

    qWarning() << "PropertyStoreHelper::getUInt32: PropVariantToUInt32 failed"
               << QSystemError::windowsComString(hr);
    return std::nullopt;
}

}

