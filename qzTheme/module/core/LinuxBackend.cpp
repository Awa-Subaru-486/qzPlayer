module;

#include <QColor>
#include <QByteArray>
#include <QProcess>
#include <memory>

module qzTheme;

import :LinuxBackend;
import :ThemeTypes;
import :IBackend;

namespace qzTheme
{
    LinuxBackend::LinuxBackend()
        : m_lastTheme(getCurrentThemeFromSystem())
    {
    }

    QColor LinuxBackend::getAccentColor() const
    {
        return {0x3689E6};
    }

    Type LinuxBackend::getSystemTheme() const
    {
        return getCurrentThemeFromSystem();
    }

    bool LinuxBackend::systemThemeChange(const QByteArray& eventType, void* message, qintptr* result)
    {
        const Type current = getCurrentThemeFromSystem();
        if (current != m_lastTheme)
        {
            m_lastTheme = current;
            return true;
        }
        return false;
    }

    auto LinuxBackend::getCurrentThemeFromSystem() -> Type
    {
        QProcess process;
        process.start("gsettings", {"get", "org.gnome.desktop.interface", "color-scheme"});
        process.waitForFinished(1000);

        if (const QString output = process.readAllStandardOutput().trimmed();
            output.contains("dark", Qt::CaseInsensitive)) {
            return Type::Dark;
        }
        return Type::Light;
    }

}
