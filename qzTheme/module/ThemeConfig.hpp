#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

#include "ThemeTypes.hpp"

namespace qzTheme
{
    class ThemeConfig : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool isDark READ isDark NOTIFY isDarkChanged)
        Q_PROPERTY(ThemeType::Type activeTheme READ activeTheme WRITE setActiveTheme NOTIFY activeThemeChanged)
        Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)
        Q_PROPERTY(QFont font READ font NOTIFY fontChanged)
        Q_PROPERTY(bool useSystemAccentColor READ useSystemAccentColor WRITE setUseSystemAccentColor NOTIFY useSystemAccentColorChanged)

    public:
        explicit ThemeConfig(QObject* parent = nullptr);
        ~ThemeConfig() override;

    public Q_SLOTS:
        [[nodiscard]] bool isDark() const;
        [[nodiscard]] ThemeType::Type activeTheme() const;
        void setActiveTheme(ThemeType::Type theme);
        [[nodiscard]] QColor accentColor() const;
        [[nodiscard]] QFont font() const;
        [[nodiscard]] bool useSystemAccentColor() const;
        void setUseSystemAccentColor(bool use);
        void overrideAccentColor(const QColor& color);
        void restoreAccentColor();

    signals:
        void isDarkChanged();
        void activeThemeChanged();
        void accentColorChanged();
        void fontChanged();
        void useSystemAccentColorChanged();

    private:
        void* m_backend; // ThemeBackend* (使用 void* 避免模块冲突)
    };
}
