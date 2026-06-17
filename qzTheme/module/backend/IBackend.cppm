module;

#include <QByteArray>
#include <QColor>
#include <QObject>
#include <functional>

export module qzTheme:IBackend;

import :ThemeTypes;

namespace qzTheme
{
    export class IBackend : public QObject
    {
    public:
        ~IBackend() override = default;
        [[nodiscard]] virtual auto getAccentColor() const -> QColor { return {0x3689E6}; }
        [[nodiscard]] virtual auto getSystemTheme() const -> Type { return Type::Light; }
        virtual auto systemThemeChange(const QByteArray& eventType, void* message, qintptr* result) -> bool { return false; }

        using ThemeChangeCallback = std::function<void()>;
        virtual void setThemeChangeCallback(ThemeChangeCallback callback) { m_themeChangeCallback = std::move(callback); }

    protected:
        void notifyThemeChanged() { if (m_themeChangeCallback) m_themeChangeCallback(); }

    private:
        ThemeChangeCallback m_themeChangeCallback;
    };
}
