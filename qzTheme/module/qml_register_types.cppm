module;

#include <QtQml/qqmlengine.h>
#include <qqml.h>
#include "ThemeConfig.hpp"
#include "qzTheme_export.hpp"

export module qml_register_types_qzTheme;

import qzTheme;

namespace qz {
    constexpr auto qz_theme_uri = "qz.theme";
    constexpr int version_major = 1;
    constexpr int version_minor = 0;
#define qz_theme_url_ma_mi qz_theme_uri, version_major, version_minor
}

export namespace qz
{
    QZTHEME_EXPORT auto qml_register_types_qz_theme(QQmlEngine *) -> void
    {
        qmlRegisterType<qzTheme::ThemeConfig>(qz_theme_url_ma_mi, "ThemeConfig");
    }
}
