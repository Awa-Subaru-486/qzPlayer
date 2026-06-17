module;

#include <QtSystemDetection>
#include <memory>

export module qzTheme:Backend;

import :IBackend;

#ifdef Q_OS_WIN
import :WindowsBackend;
#elif defined(Q_OS_ANDROID)
import :AndroidBackend;
#elif defined(Q_OS_LINUX)
import :LinuxBackend;
#endif

namespace qzTheme
{
    export [[nodiscard]] auto createBackend() -> std::unique_ptr<IBackend>
    {
#ifdef Q_OS_WIN
        return createWindowsBackend();
#elif defined(Q_OS_ANDROID)
        return createAndroidBackend();
#elif defined(Q_OS_LINUX)
        return createLinuxBackend();
#else
        return std::make_unique<IBackend>();
#endif
    }
}
