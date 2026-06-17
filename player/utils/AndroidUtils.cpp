#include "AndroidUtils.hpp"
#include <QtCore/qjnitypes.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

Q_DECLARE_JNI_CLASS(QtAndroidUtils, "qz/player/android/QtAndroidUtils")

QT_END_NAMESPACE

namespace qz {

// Global callback for battery status changes
static std::function<void()> g_batteryStatusCallback;

// Global callback for PiP mode changes
static std::function<void(bool)> g_pictureInPictureCallback;

// Register the callback
static void registerBatteryCallback(std::function<void()> callback)
{
    g_batteryStatusCallback = callback;
}

// JNI native method to notify battery status change
extern "C" JNIEXPORT void JNICALL
Java_qz_player_android_QtAndroidUtils_notifyBatteryStatusChanged(JNIEnv *, jclass)
{
    try {
        if (g_batteryStatusCallback) {
            g_batteryStatusCallback();
        }
    } catch (...) {
        // Catch all exceptions to prevent JNI crash
        qWarning() << "Exception in battery status callback";
    }
}

AndroidUtils::AndroidUtils(QObject* parent)
    : QObject(parent)
{
    const auto context = QNativeInterface::QAndroidApplication::context();
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("setContext", context);

    // Register battery status callback
    registerBatteryCallback([this]() {
        QMetaObject::invokeMethod(this, "batteryStatusChanged", Qt::QueuedConnection);
    });

    // Register PiP mode callback
    g_pictureInPictureCallback = [this](bool isInPiP) {
        QMetaObject::invokeMethod(this, [this, isInPiP]() {
            m_isPictureInPicture = isInPiP;
            emit pictureInPictureChanged(isInPiP);
        }, Qt::QueuedConnection);
    };

    // Install event filter to intercept Android back key at application level
    QGuiApplication::instance()->installEventFilter(this);
}

AndroidUtils::~AndroidUtils()
{
    // Clear the callback to avoid dangling pointer
    g_batteryStatusCallback = nullptr;
    g_pictureInPictureCallback = nullptr;

    // Remove event filter
    QGuiApplication::instance()->removeEventFilter(this);
}

bool AndroidUtils::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Back) {
            emit backPressed();
            return true; // Consume the event, prevent default Activity.finish()
        }
    }
    return QObject::eventFilter(watched, event);
}

int AndroidUtils::screenOrientation() const
{
    return m_screenOrientation;
}

void AndroidUtils::set_screenOrientation(int orientation)
{
    if (m_screenOrientation == orientation)
        return;

    m_screenOrientation = orientation;
    setRequestedOrientation(orientation);
    emit screenOrientationChanged();
}

void AndroidUtils::setRequestedOrientation(int orientation)
{
    // Map our enum to Android ActivityInfo constants
    // SCREEN_ORIENTATION_UNSPECIFIED = -1
    // SCREEN_ORIENTATION_LANDSCAPE = 0
    // SCREEN_ORIENTATION_PORTRAIT = 1
    // SCREEN_ORIENTATION_REVERSE_LANDSCAPE = 8
    // SCREEN_ORIENTATION_REVERSE_PORTRAIT = 9
    int androidOrientation = -1;
    switch (orientation) {
    case Unspecified:       androidOrientation = -1; break;
    case Portrait:          androidOrientation = 1;  break;
    case Landscape:         androidOrientation = 0;  break;
    case ReversePortrait:   androidOrientation = 9;  break;
    case ReverseLandscape:  androidOrientation = 8;  break;
    }

    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("setRequestedOrientation", androidOrientation);
}

int AndroidUtils::getRequestedOrientation() const
{
    const jint result = QtJniTypes::QtAndroidUtils::callStaticMethod<jint>("getRequestedOrientation");

    // Map Android constants back to our enum
    switch (result) {
    case 1:  return Portrait;
    case 0:  return Landscape;
    case 9:  return ReversePortrait;
    case 8:  return ReverseLandscape;
    default: return Unspecified;
    }
}

void AndroidUtils::toggleOrientation()
{
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("toggleOrientation");

    m_screenOrientation = getRequestedOrientation();
    emit screenOrientationChanged();
}

// ---- System Brightness ----

int AndroidUtils::systemBrightness() const
{
    return QtJniTypes::QtAndroidUtils::callStaticMethod<jint>("getSystemBrightness");
}

void AndroidUtils::setSystemBrightness(int brightness)
{
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("setSystemBrightness", brightness);
}

bool AndroidUtils::isAutoBrightness() const
{
    return QtJniTypes::QtAndroidUtils::callStaticMethod<jboolean>("isAutoBrightness");
}

void AndroidUtils::setAutoBrightness(bool enabled)
{
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("setAutoBrightness", enabled);
}

void AndroidUtils::applyWindowBrightness(int brightness)
{
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("applyWindowBrightness", brightness);
}

void AndroidUtils::moveTaskToBack()
{
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("moveTaskToBack");
}

void AndroidUtils::finishActivity()
{
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("finishActivity");
}

// ---- System Volume ----

int AndroidUtils::systemVolume(int streamType) const
{
    return QtJniTypes::QtAndroidUtils::callStaticMethod<jint>("getSystemVolume", streamType);
}

void AndroidUtils::setSystemVolume(int streamType, int volume)
{
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("setSystemVolume", streamType, volume);
}

int AndroidUtils::maxSystemVolume(int streamType) const
{
    return QtJniTypes::QtAndroidUtils::callStaticMethod<jint>("getMaxSystemVolume", streamType);
}

void AndroidUtils::adjustSystemVolume(int streamType, int direction)
{
    QtJniTypes::QtAndroidUtils::callStaticMethod<void>("adjustSystemVolume", streamType, direction);
}

// ---- Battery ----

int AndroidUtils::batteryLevel() const
{
    return QtJniTypes::QtAndroidUtils::callStaticMethod<jint>("getBatteryLevel");
}

bool AndroidUtils::isBatteryCharging() const
{
    return QtJniTypes::QtAndroidUtils::callStaticMethod<jboolean>("isBatteryCharging");
}

void AndroidUtils::notifyBatteryStatusChanged()
{
    emit batteryStatusChanged();
}

// ---- Picture in Picture ----

bool AndroidUtils::enterPictureInPicture(int aspectRatioWidth, int aspectRatioHeight)
{
    return QtJniTypes::QtAndroidUtils::callStaticMethod<jboolean>(
        "enterPictureInPicture", aspectRatioWidth, aspectRatioHeight);
}

bool AndroidUtils::isPictureInPicture() const
{
    return m_isPictureInPicture;
}

} // namespace qz

// JNI native method for PiP mode change notification
extern "C" JNIEXPORT void JNICALL
Java_qz_player_android_QtAndroidUtils_notifyPictureInPictureChanged(JNIEnv *, jclass, jboolean isInPiP)
{
    try {
        if (qz::g_pictureInPictureCallback) {
            qz::g_pictureInPictureCallback(static_cast<bool>(isInPiP));
        }
    } catch (...) {
        qWarning() << "Exception in PiP mode change callback";
    }
}
