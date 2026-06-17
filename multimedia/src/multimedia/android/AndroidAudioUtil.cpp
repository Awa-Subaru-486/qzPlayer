// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AndroidAudioUtil_p.h"
#include "AndroidAudioDevice_p.h"

#include <QtCore/QJniObject>
#include <QtCore/qjnitypes.h>
#include <QtGui/QGuiApplication>

QT_BEGIN_NAMESPACE

Q_DECLARE_JNI_CLASS(QtContentUriResolver, "qz/multimedia/android/QtContentUriResolver")

namespace AndroidAudioUtil {

bool supportsLowLatency()
{
    static bool result = [] {
        auto context = QNativeInterface::QAndroidApplication::context();
        QJniObject activity = context;
        auto packageManager = activity.callObjectMethod("getPackageManager",
                                                        "()Landroid/content/pm/PackageManager;");
        auto feature = QJniObject::fromString(QStringLiteral("android.hardware.audio.low_latency"));
        auto hasFeature = packageManager.callMethod<jboolean>("hasSystemFeature",
                                                               "(Ljava/lang/String;)Z",
                                                               feature.object<jstring>());
        return hasFeature;
    }();
    return result;
}

bool isDefaultBluetoothDevice(const AudioDevice &device)
{
    return device.isDefault()
            && AudioDevicePrivate::handle<AndroidAudioDevice>(device)->isBluetoothDevice();
}

QString getContentDisplayName(const QString &uriString)
{
    if (!uriString.startsWith(u"content://"))
        return {};

    static bool s_contextSet = false;
    if (!s_contextSet) {
        const auto context = QNativeInterface::QAndroidApplication::context();
        QtJniTypes::QtContentUriResolver::callStaticMethod<void>("setContext", context);
        s_contextSet = true;
    }

    const QJniObject result = QtJniTypes::QtContentUriResolver::callStaticMethod<jstring>(
        "getContentDisplayName", uriString);
    if (!result.isValid())
        return {};
    return result.toString();
}

} // namespace AndroidAudioUtil

QT_END_NAMESPACE
