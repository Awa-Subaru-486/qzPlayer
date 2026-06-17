// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_WINDOWS_WINDOWSPROPERTYSTORE_P_H
#define QT_WINDOWS_WINDOWSPROPERTYSTORE_P_H

#include <QtCore/qglobal.h>
#include <QtCore/private/qcomptr_p.h>
#include <expected>
#include <QtCore/private/qsystemerror_p.h>

struct IMMDevice;
struct IPropertyStore;
typedef struct _tagpropertykey PROPERTYKEY;

namespace QtMultimediaPrivate {

class PropertyStoreHelper
{
public:
    explicit PropertyStoreHelper(ComPtr<IPropertyStore>);
    static std::expected<PropertyStoreHelper, QString> open(const ComPtr<IMMDevice> &);

    std::optional<QString> getString(const PROPERTYKEY &);
    std::optional<uint32_t> getUInt32(const PROPERTYKEY &);

private:
    ComPtr<IPropertyStore> m_props;
};

}

#endif
