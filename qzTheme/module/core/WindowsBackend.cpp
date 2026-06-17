module;

#include <QColor>
#include <QByteArray>
#include <windows.h>
#include <dwmapi.h>

module qzTheme;

import :WindowsBackend;
import :ThemeTypes;
import :IBackend;

namespace qzTheme
{
    QColor WindowsBackend::getAccentColor() const
    {
        // 从注册表读取强调色（Windows 10+ 可靠的方式）
        // AccentColor 存储为 ABGR 格式 (0xAABBGGRR)
        DWORD accentColor = 0;
        DWORD dataSize = sizeof(accentColor);

        if (RegGetValueW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
            L"AccentColor",
            RRF_RT_REG_DWORD,
            nullptr,
            &accentColor,
            &dataSize) == ERROR_SUCCESS && accentColor != 0)
        {
            return QColor(accentColor & 0xFF, (accentColor >> 8) & 0xFF, (accentColor >> 16) & 0xFF);
        }

        // 回退到 DwmGetColorizationColor（ARGB 格式 0xAARRGGBB）
        const HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
        if (!hDwm) return {0xE8, 0x3B, 0x5C};

        const auto pDwmGetColor = reinterpret_cast<HRESULT(WINAPI*)(DWORD*, BOOL*)>(
            ::GetProcAddress(hDwm, "DwmGetColorizationColor")
        );

        QColor result(0xE8, 0x3B, 0x5C);
        if (pDwmGetColor)
        {
            DWORD color = 0;
            BOOL opaque = FALSE;
            if (SUCCEEDED(pDwmGetColor(&color, &opaque)))
            {
                result = QColor((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
            }
        }
        ::FreeLibrary(hDwm);
        return result;
    }

    Type WindowsBackend::getSystemTheme() const
    {
        static constexpr wchar_t REG_KEY_PATH[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
        static constexpr wchar_t REG_VALUE_NAME[] = L"AppsUseLightTheme";
        static constexpr DWORD DEFAULT_THEME_VALUE = 1;

        DWORD regValue = DEFAULT_THEME_VALUE;
        DWORD dataSize = sizeof(regValue);

        const LONG result = RegGetValueW(
            HKEY_CURRENT_USER,
            REG_KEY_PATH,
            REG_VALUE_NAME,
            RRF_RT_REG_DWORD,
            nullptr,
            &regValue,
            &dataSize
        );

        if (result != ERROR_SUCCESS) {
            return Type::Light;
        }

        return (regValue == 0) ? Type::Dark : Type::Light;
    }

    bool WindowsBackend::systemThemeChange(const QByteArray& eventType, void* message, qintptr* result)
    {
        if (const auto msg = static_cast<MSG*>(message); msg->message == WM_SETTINGCHANGE)
        {
            if (reinterpret_cast<LPCTSTR>(msg->lParam) != nullptr &&
                lstrcmpi(reinterpret_cast<LPCTSTR>(msg->lParam), L"ImmersiveColorSet") == 0)
            {
                return true;
            }
        }
        return false;
    }

}
