module;

#include <QColor>
#include <QByteArray>
#include <QtCore/qjnitypes.h>
#include <QtGui/QGuiApplication>
#include <memory>

module qzTheme;

import :AndroidBackend;
import :ThemeTypes;
import :IBackend;

QT_BEGIN_NAMESPACE

Q_DECLARE_JNI_CLASS(QtThemeBackend, "qz/theme/android/QtThemeBackend")

static void onThemeChanged(JNIEnv *, jobject, jlong nativePtr, jint themeMode)
{
    auto *backend = reinterpret_cast<qzTheme::AndroidBackend*>(static_cast<size_t>(nativePtr));
    if (backend)
        backend->onThemeChanged(static_cast<int>(themeMode));
}
Q_DECLARE_JNI_NATIVE_METHOD(onThemeChanged)

QT_END_NAMESPACE

namespace qzTheme
{
    AndroidBackend::AndroidBackend()
        : m_lastTheme(getSystemTheme())
    {
        registerNativeMethods();

        using namespace QtJniTypes;
        const auto nativePtr = static_cast<jlong>(reinterpret_cast<size_t>(this));
        QtThemeBackend::callStaticMethod<void>("registerBackend", nativePtr);
    }

    AndroidBackend::~AndroidBackend()
    {
        using namespace QtJniTypes;
        QtThemeBackend::callStaticMethod<void>("unregisterBackend");
    }

    QColor AndroidBackend::getAccentColor() const
    {
        using namespace QtJniTypes;
        const jint color = QtThemeBackend::callStaticMethod<jint>("getSystemAccentColor");
        return QColor(static_cast<QRgb>(color));
    }

    Type AndroidBackend::getSystemTheme() const
    {
        using namespace QtJniTypes;
        const jint mode = QtThemeBackend::callStaticMethod<jint>("getSystemThemeMode");
        return (mode == 1) ? Type::Dark : Type::Light;
    }

    bool AndroidBackend::systemThemeChange(const QByteArray& eventType, void* message, qintptr* result)
    {
        return false;
    }

    void AndroidBackend::onThemeChanged(int themeMode)
    {
        const Type newTheme = (themeMode == 1) ? Type::Dark : Type::Light;
        if (newTheme != m_lastTheme)
        {
            m_lastTheme = newTheme;
            // JNI callback runs on Android main thread, ensure signal emission on Qt thread
            QMetaObject::invokeMethod(this, [this]() {
                notifyThemeChanged();
            }, Qt::QueuedConnection);
        }
    }

    bool AndroidBackend::registerNativeMethods()
    {
        static const bool registered = []{
            const auto context = QNativeInterface::QAndroidApplication::context();
            QtJniTypes::QtThemeBackend::callStaticMethod<void>("setContext", context);

            return QtJniTypes::QtThemeBackend::registerNativeMethods({
                Q_JNI_NATIVE_METHOD(onThemeChanged),
            });
        }();
        return registered;
    }

}
