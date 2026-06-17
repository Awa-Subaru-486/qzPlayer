module;

#include <QColor>
#include <QByteArray>
#include <memory>
#include <windows.h>
#include <dwmapi.h>

export module qzTheme:WindowsBackend;

import :ThemeTypes;
import :IBackend;

namespace qzTheme
{
    class WindowsBackend final : public IBackend
    {
    public:
        [[nodiscard]] auto getAccentColor() const -> QColor override;
        [[nodiscard]] auto getSystemTheme() const -> Type override;
        auto systemThemeChange(const QByteArray& eventType, void* message, qintptr* result) -> bool override;
    };

    [[nodiscard]] inline auto createWindowsBackend() -> std::unique_ptr<IBackend>
    {
        return std::make_unique<WindowsBackend>();
    }
}
