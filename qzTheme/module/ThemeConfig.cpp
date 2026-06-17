#include "ThemeConfig.hpp"

import qzTheme;

namespace qzTheme
{
    ThemeConfig::ThemeConfig(QObject* parent)
        : QObject(parent)
        , m_backend(&ThemeBackend::instance())
    {
        const auto* backend = &ThemeBackend::instance();
        connect(backend, &ThemeBackend::isDarkChanged, this, &ThemeConfig::isDarkChanged);
        connect(backend, &ThemeBackend::activeThemeChanged, this, &ThemeConfig::activeThemeChanged);
        connect(backend, &ThemeBackend::accentColorChanged, this, &ThemeConfig::accentColorChanged);
        connect(backend, &ThemeBackend::fontChanged, this, &ThemeConfig::fontChanged);
        connect(backend, &ThemeBackend::useSystemAccentColorChanged, this, &ThemeConfig::useSystemAccentColorChanged);
    }

    ThemeConfig::~ThemeConfig() = default;

    bool ThemeConfig::isDark() const
    {
        return ThemeBackend::instance().isDark();
    }

    Type ThemeConfig::activeTheme() const
    {
        return ThemeBackend::instance().activeTheme();
    }

    void ThemeConfig::setActiveTheme(Type theme)
    {
        ThemeBackend::instance().set_activeTheme(theme);
    }

    QColor ThemeConfig::accentColor() const
    {
        return ThemeBackend::instance().accentColor();
    }

    QFont ThemeConfig::font() const
    {
        return ThemeBackend::instance().font();
    }

    bool ThemeConfig::useSystemAccentColor() const
    {
        return ThemeBackend::instance().useSystemAccentColor();
    }

    void ThemeConfig::setUseSystemAccentColor(bool use)
    {
        ThemeBackend::instance().set_useSystemAccentColor(use);
    }

    void ThemeConfig::overrideAccentColor(const QColor& color)
    {
        ThemeBackend::instance().overrideAccentColor(color);
    }

    void ThemeConfig::restoreAccentColor()
    {
        ThemeBackend::instance().restoreAccentColor();
    }
}
