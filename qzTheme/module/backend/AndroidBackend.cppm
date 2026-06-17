module;

#include <QColor>
#include <QByteArray>
#include <memory>

export module qzTheme:AndroidBackend;

import :ThemeTypes;
import :IBackend;

namespace qzTheme
{
    class AndroidBackend final : public IBackend
    {
    public:
        AndroidBackend();
        ~AndroidBackend() override;

        [[nodiscard]] auto getAccentColor() const -> QColor override;
        [[nodiscard]] auto getSystemTheme() const -> Type override;
        auto systemThemeChange(const QByteArray& eventType, void* message, qintptr* result) -> bool override;

        void onThemeChanged(int themeMode);

    private:
        bool registerNativeMethods();
        Type m_lastTheme{Type::Light};
    };

    [[nodiscard]] inline auto createAndroidBackend() -> std::unique_ptr<IBackend>
    {
        return std::make_unique<AndroidBackend>();
    }
}
