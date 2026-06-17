#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "qzTheme_export.hpp"

namespace ThemeType
{
    Q_NAMESPACE_EXPORT(QZTHEME_EXPORT)
    QML_ELEMENT
    enum class Type
    {
        System,
        Light,
        Dark
    };
    Q_ENUM_NS(Type)
}
