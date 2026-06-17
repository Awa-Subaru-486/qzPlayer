// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AndroidAudioDevices_p.h"

#include <qzMultimedia/private/AndroidAudioDevice_p.h>
#include <qzMultimedia/private/AndroidAudioJniTypes_p.h>
#include <qzMultimedia/private/AndroidAudioSink_p.h>
#include <qzMultimedia/private/AndroidAudioSource_p.h>

#include <QtCore/qjnitypes.h>
#include <QtGui/QGuiApplication>
#include <qzMultimedia/private/AudioDeviceMonitor_p.h>

QT_BEGIN_NAMESPACE

using namespace QtJniTypes;

namespace {

AudioFormat preferredFormatForDevice(const QtJniTypes::AudioDeviceInfo &deviceInfo)
{
    AudioFormat preferredFormat;

    // Set preferred channel count based on what device reports, with default set to stereo (2)
    QJniArray<jint> channelCounts = deviceInfo.callMethod<QJniArray<jint>>("getChannelCounts");
    if (channelCounts.isEmpty()) {
        preferredFormat.setChannelConfig(AudioFormat::ChannelConfigStereo);
    } else {
        const auto [minIt, maxIt] = std::minmax_element(channelCounts.begin(), channelCounts.end());
        const int channelCount = std::clamp(2, *minIt, *maxIt);
        preferredFormat.setChannelConfig(
                AudioFormat::defaultChannelConfigForChannelCount(channelCount));
    }

    // Get optimal sample rate from AudioManager
    preferredFormat.setSampleRate(
            QtAudioDeviceManager::callStaticMethod<jint>("getDefaultSampleRate"));

    // Using Float avoids conversions for processing, so we should prefer that instead of whatever
    // the device uses natively
    preferredFormat.setSampleFormat(AudioFormat::Float);

    return preferredFormat;
};

QList<AudioDevice> availableDevices(AudioDevice::Mode mode)
{
    if (mode == AudioDevice::Null)
        return {};

    QList<AudioDevice> devices;
    const char *getMethod =
            mode == AudioDevice::Input ? "getAudioInputDevices" : "getAudioOutputDevices";
    auto deviceInfos =
            QtAudioDeviceManager::callStaticMethod<QJniArray<AudioDeviceInfo>>(getMethod);

    if (!deviceInfos.isValid())
        return {};

    for (int i = 0; i < deviceInfos.size(); ++i) {
        AudioDeviceInfo deviceInfo = deviceInfos.at(i);
        int id = deviceInfo.callMethod<jint>("getId");
        jint deviceType = deviceInfo.callMethod<jint>("getType");
        auto description = QtAudioDeviceManager::callStaticMethod<QString>(
                "audioDeviceTypeToString", deviceType);
        bool isBluetoothDevice =
                QtAudioDeviceManager::callStaticMethod<jboolean>("isBluetoothDevice", deviceInfo);
        devices << AudioDevicePrivate::createQAudioDevice(std::make_unique<AndroidAudioDevice>(
                QString::number(id).toUtf8(), description, mode,
                preferredFormatForDevice(deviceInfo), isBluetoothDevice, i == 0));
    }

    return devices;
}

} // namespace

// Called by any C++ thread
AndroidAudioDevices::AndroidAudioDevices() : PlatformAudioDevices()
{
    QtAudioDeviceManager::callStaticMethod<void>(
        "qAndroidAudioDevicesConstructed",
        static_cast<jlong>(reinterpret_cast<size_t>(this)));
}

AndroidAudioDevices::~AndroidAudioDevices()
{
    // Performs a blocking call to unregister AndroidAudioDevices from receiving
    // any more callbacks, and flushes remaining callbacks.
    QtAudioDeviceManager::callStaticMethod<void>("qAndroidAudioDevicesDestroyed");
}

// Called by any C++ thread
QList<AudioDevice> AndroidAudioDevices::findAudioInputs() const
{
    return availableDevices(AudioDevice::Input);
}

// Called by any C++ thread
QList<AudioDevice> AndroidAudioDevices::findAudioOutputs() const
{
    return availableDevices(AudioDevice::Output);
}

// Called by any C++ thread
PlatformAudioSource *AndroidAudioDevices::createAudioSource(const AudioDevice &deviceInfo,
                                                              const AudioFormat &fmt,
                                                              QObject *parent)
{
    return new QtAAudio::AndroidAudioSource(deviceInfo, fmt, parent);
}

// Called by any C++ thread
PlatformAudioSink *AndroidAudioDevices::createAudioSink(const AudioDevice &deviceInfo,
                                                          const AudioFormat &fmt, QObject *parent)
{
    return new QtAAudio::AndroidAudioSink(deviceInfo, fmt, parent);
}

// Invoked by background Java Handler thread
static void onAudioInputDevicesUpdated(
    JNIEnv * /*env*/,
    jobject /*thiz*/,
    jlong nativePtr)
{
    auto *audioDevices = reinterpret_cast<AndroidAudioDevices*>(static_cast<size_t>(nativePtr));
    Q_ASSERT(!audioDevices->thread()->isCurrentThread());
    audioDevices->onAudioInputsChanged();
}
Q_DECLARE_JNI_NATIVE_METHOD(onAudioInputDevicesUpdated)

// Invoked by background Java Handler thread
static void onAudioOutputDevicesUpdated(
    JNIEnv * /*env*/,
    jobject /*thiz*/,
    jlong nativePtr)
{
    auto *audioDevices = reinterpret_cast<AndroidAudioDevices*>(static_cast<size_t>(nativePtr));
    Q_ASSERT(!audioDevices->thread()->isCurrentThread());
    audioDevices->onAudioOutputsChanged();
}
Q_DECLARE_JNI_NATIVE_METHOD(onAudioOutputDevicesUpdated)

// Invoked by Java Handler thread when the default output device changes
static void onDefaultOutputDeviceChanged(
    JNIEnv *,
    jobject)
{
    // Notify AudioDeviceMonitor to update the audio output device
    // and pause the MediaPlayer
    AudioDeviceMonitor::notifyOutputDeviceChanged();
}
Q_DECLARE_JNI_NATIVE_METHOD(onDefaultOutputDeviceChanged)

bool AndroidAudioDevices::registerNativeMethods()
{
    static const bool registered = []{
        const auto context = QNativeInterface::QAndroidApplication::context();
        QtAudioDeviceManager::callStaticMethod<void>("setContext", context);

        return QtJniTypes::QtAudioDeviceManager::registerNativeMethods({
            Q_JNI_NATIVE_METHOD(onAudioInputDevicesUpdated),
            Q_JNI_NATIVE_METHOD(onAudioOutputDevicesUpdated),
            Q_JNI_NATIVE_METHOD(onDefaultOutputDeviceChanged),
        });
    }();
    return registered;
}

QT_END_NAMESPACE

extern "C" Q_DECL_EXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /*reserved*/)
{
    static bool initialized = false;
    if (initialized)
        return JNI_VERSION_1_6;
    initialized = true;

    QT_USE_NAMESPACE
    typedef union {
        JNIEnv *nativeEnvironment;
        void *venv;
    } UnionJNIEnvToVoid;

    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;

    if (!AndroidAudioDevices::registerNativeMethods())
        return JNI_ERR;

    return JNI_VERSION_1_6;
}
