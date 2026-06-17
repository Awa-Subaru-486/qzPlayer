#pragma once

#include <QObject>
#include <QtQml/qqml.h>
#include <QtCore/qjnitypes.h>

#include "qzPlayer_export.hpp"

namespace qz {

class QZ_PLAYER_EXPORT AndroidUtils : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int screenOrientation READ screenOrientation WRITE set_screenOrientation NOTIFY screenOrientationChanged)

public:
    explicit AndroidUtils(QObject* parent = nullptr);
    ~AndroidUtils() override;

    enum ScreenOrientation {
        Unspecified = 0,
        Portrait = 1,
        Landscape = 2,
        ReversePortrait = 3,
        ReverseLandscape = 4
    };
    Q_ENUM(ScreenOrientation)

    enum AudioStreamType {
        StreamVoiceCall = 0,
        StreamSystem = 1,
        StreamRing = 2,
        StreamMusic = 3,
        StreamAlarm = 4,
        StreamNotification = 5
    };
    Q_ENUM(AudioStreamType)

    enum VolumeAdjustDirection {
        AdjustLower = -1,
        AdjustSame = 0,
        AdjustRaise = 1
    };
    Q_ENUM(VolumeAdjustDirection)

    int screenOrientation() const;
    void set_screenOrientation(int orientation);

    Q_INVOKABLE void setRequestedOrientation(int orientation);
    Q_INVOKABLE int getRequestedOrientation() const;
    Q_INVOKABLE void toggleOrientation();

    // System Brightness
    Q_INVOKABLE int systemBrightness() const;
    Q_INVOKABLE void setSystemBrightness(int brightness);
    Q_INVOKABLE bool isAutoBrightness() const;
    Q_INVOKABLE void setAutoBrightness(bool enabled);
    Q_INVOKABLE void applyWindowBrightness(int brightness);

    // Activity
    Q_INVOKABLE void moveTaskToBack();
    Q_INVOKABLE void finishActivity();

    // Picture in Picture
    Q_INVOKABLE bool enterPictureInPicture(int aspectRatioWidth = 16, int aspectRatioHeight = 9);
    Q_INVOKABLE bool isPictureInPicture() const;

    // System Volume
    Q_INVOKABLE int systemVolume(int streamType) const;
    Q_INVOKABLE void setSystemVolume(int streamType, int volume);
    Q_INVOKABLE int maxSystemVolume(int streamType) const;
    Q_INVOKABLE void adjustSystemVolume(int streamType, int direction);

    // Battery
    Q_INVOKABLE int batteryLevel() const;
    Q_INVOKABLE bool isBatteryCharging() const;

    // Internal method called from Java to notify battery status change
    Q_INVOKABLE void notifyBatteryStatusChanged();

Q_SIGNALS:
    void screenOrientationChanged();
    void batteryStatusChanged();
    void backPressed();
    void pictureInPictureChanged(bool isInPiP);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    int m_screenOrientation{Unspecified};
    bool m_isPictureInPicture{false};
};

} // namespace qz
