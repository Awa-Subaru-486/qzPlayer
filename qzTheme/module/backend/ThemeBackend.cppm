module;

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QObject>
#include <QQmlEngine>
#include <QSettings>
#include <QtSystemDetection>
#include <memory>

#ifdef Q_OS_ANDROID
#include <jni.h>
#endif

#include "qzTheme_export.hpp"

export module qzTheme:ThemeBackend;

import :ThemeTypes;
import :IBackend;
import :Backend;

namespace qzTheme
{
    export class QZTHEME_EXPORT ThemeBackend : public QObject, QAbstractNativeEventFilter
    {
        Q_OBJECT

    public:
        static auto instance() -> ThemeBackend&
        {
            static auto s_instance = new ThemeBackend();
            QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
            return *s_instance;
        }

        ~ThemeBackend() override
        {
            qApp->removeNativeEventFilter(this);
        }

        auto set_activeTheme(Type in_activeTheme) -> void
        {
            if(m_activeTheme == in_activeTheme) return;
            m_activeTheme = in_activeTheme;
            updateTheme(false);
            emit activeThemeChanged();
        }

        [[nodiscard]] auto activeTheme() const -> Type
        {
            return m_activeTheme;
        }

        auto set_isDark(bool in_isDark) -> void
        {
            if(m_isDark == in_isDark) return;
            m_isDark = in_isDark;
            emit isDarkChanged();
        }

        [[nodiscard]] auto isDark() const -> bool
        {
            return m_isDark;
        }

        auto set_useSystemAccentColor(bool in_useSystemAccentColor) -> void
        {
            if(m_useSystemAccentColor == in_useSystemAccentColor) return;
            m_useSystemAccentColor = in_useSystemAccentColor;
            emit useSystemAccentColorChanged();
        }

        [[nodiscard]] auto useSystemAccentColor() const -> bool
        {
            return m_useSystemAccentColor;
        }

        void set_accentColor(const QColor& in_accentColor)
        {
            if(m_accentColor == in_accentColor) return;
            m_accentColor = in_accentColor;
            emit accentColorChanged();
        }

        [[nodiscard]] QColor accentColor() const
        {
            return m_accentColor;
        }

        /**
         * @brief 临时覆盖强调色（例如从图像提取）
         * @param color 新的强调色
         */
        void overrideAccentColor(const QColor& color)
        {
            if (!m_accentColorOverridden)
            {
                m_savedAccentColor = m_accentColor;
            }
            m_accentColorOverridden = true;
            set_accentColor(color);
        }

        /**
         * @brief 恢复被覆盖前的原始强调色
         */
        void restoreAccentColor()
        {
            if (!m_accentColorOverridden) return;
            m_accentColorOverridden = false;
            set_accentColor(m_savedAccentColor);
        }

        void set_font(const QFont& in_font)
        {
            if(m_font == in_font) return;
            m_font = in_font;
            emit fontChanged();
        }

        [[nodiscard]] QFont font() const
        {
            return m_font;
        }

    signals:
        void isDarkChanged();
        void activeThemeChanged();
        void accentColorChanged();
        void fontChanged();
        void useSystemAccentColorChanged();

    private:
        explicit ThemeBackend()
            : QObject(nullptr)
        {
            qApp->installNativeEventFilter(this);
            m_themeService = createBackend();
            m_font = QFont(QStringLiteral("Microsoft YaHei UI"), 12);

#ifdef Q_OS_ANDROID
            m_themeService->setThemeChangeCallback([this]() {
                if (m_followSystem) {
                    m_activeTheme = Type::System;
                    updateTheme(true);
                }
            });
#endif

            updateTheme(true);
        }

        auto nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) -> bool override
        {
            if (!m_followSystem)
                return false;

            if (m_themeService->systemThemeChange(eventType, message, result))
            {
                m_activeTheme = Type::System;
                updateTheme(true);
                return true;
            }
            return false;
        }

        auto updateTheme(bool followSystem) -> void
        {
            switch(m_activeTheme)
            {
                case Type::System:
                {
                    m_followSystem = true;
                    m_activeTheme = m_themeService->getSystemTheme();
                    if (m_useSystemAccentColor)
                        set_accentColor(m_themeService->getAccentColor());
                    updateTheme(true);
                    emit activeThemeChanged();
                    break;
                }
                case Type::Light:
                {
                    if (!followSystem)
                        m_followSystem = false;
                    set_isDark(false);
                    break;
                }
                case Type::Dark:
                {
                    if (!followSystem)
                        m_followSystem = false;
                    set_isDark(true);
                    break;
                }
            }
        }

        bool m_isDark{};
        bool m_useSystemAccentColor{true};
        bool m_followSystem{true};
        bool m_accentColorOverridden{false};
        Type m_activeTheme{Type::System};
        QFont m_font{};
        QColor m_accentColor{};
        QColor m_savedAccentColor{};
        std::unique_ptr<IBackend> m_themeService{};
    };
}

#include "ThemeBackend.moc"
