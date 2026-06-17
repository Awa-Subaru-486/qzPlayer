module;

#include <QColor>
#include <QByteArray>
#include <memory>

export module qzTheme:LinuxBackend;

import :ThemeTypes;
import :IBackend;

namespace qzTheme
{
    class LinuxBackend final : public IBackend
    {
    public:
        LinuxBackend();

        [[nodiscard]] auto getAccentColor() const -> QColor override;
        [[nodiscard]] auto getSystemTheme() const -> Type override;
        auto systemThemeChange(const QByteArray& eventType, void* message, qintptr* result) -> bool override;

    private:
        [[nodiscard]] static auto getCurrentThemeFromSystem() -> Type;

        Type m_lastTheme;
    };

    [[nodiscard]] inline auto createLinuxBackend() -> std::unique_ptr<IBackend>
    {
        return std::make_unique<LinuxBackend>();
    }
}
