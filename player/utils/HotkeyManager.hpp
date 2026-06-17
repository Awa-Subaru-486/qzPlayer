#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeyCombination>
#include <QHash>
#include <QtQml/qqml.h>

#include "qzPlayer_export.hpp"
#include "Settings.hpp"

class QWindow;
class MediaPlayer;
class NotificationManager;

namespace qz {

class QZ_PLAYER_EXPORT HotkeyManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QObject* mediaPlayer READ mediaPlayer WRITE setMediaPlayer NOTIFY mediaPlayerChanged)
    Q_PROPERTY(QObject* window READ window WRITE setWindow NOTIFY windowChanged)

public:
    enum class Action {
        PlayPause,
        SeekForward,
        SeekBackward,
        Stop,
        SpeedUp,
        SpeedDown,
        VolumeUp,
        VolumeDown,
        Mute,
        Fullscreen,
    };
    Q_ENUM(Action)

    enum class Mode {
        Disabled = 0,
        WindowFocus = 1,
        Global = 2,
    };
    Q_ENUM(Mode)

    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    int mode() const;
    void setMode(int mode);

    QObject* mediaPlayer() const;
    void setMediaPlayer(QObject* player);

    QObject* window() const;
    void setWindow(QObject* window);

    Q_INVOKABLE void setHotkey(Action action, const QString& keySequence);  // 设置快捷键绑定
    Q_INVOKABLE QString hotkey(Action action) const;  // 获取当前快捷键
    Q_INVOKABLE QString defaultHotkey(Action action) const;  // 获取默认快捷键
    Q_INVOKABLE void resetToDefaults();  // 重置所有快捷键为默认值

    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();

signals:
    void modeChanged();
    void mediaPlayerChanged();
    void windowChanged();
    void hotkeyChanged(Action action);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void registerGlobalHotkeys();
    void unregisterGlobalHotkeys();
    void applyWindowFocusMode();
    void removeWindowFocusMode();

    void executeAction(Action action);
    void showActionNotification(Action action);
    void closeActionNotification();
    MediaPlayer* resolveMediaPlayer() const;

    void loadSettings();
    void saveSettings();

    static QString actionKey(Action action);
    static QString actionDefaultKey(Action action);

    int m_mode{static_cast<int>(Mode::Disabled)};
    QPointer<QObject> m_mediaPlayer;
    QPointer<QObject> m_window;

    QHash<Action, QKeyCombination> m_hotkeys;
    Settings m_settings;

    bool m_active{false};

    // 长按追踪：仅加速/减速/音量增减支持长按
    Action m_pressedAction{Action::PlayPause};
    bool m_actionPressed{false};
    qreal m_speedBeforeHold{1.0};

    static bool isRepeatableAction(Action action);

#ifdef Q_OS_WIN
    QHash<Action, int> m_globalHotkeyIds;
    int m_nextHotkeyId{1};
#endif

    static constexpr int kSeekIntervalMs = 10000;
    static constexpr qreal kSpeedUpRate = 3.0;
    static constexpr qreal kSpeedDownRate = 0.5;
    static constexpr qreal kDefaultSpeed = 1.0;
    static constexpr int kVolumeStep = 5;
};

} // namespace qz
