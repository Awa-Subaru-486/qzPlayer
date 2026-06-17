#include "HotkeyManager.hpp"
#include "NotificationManager.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QWindow>
#include <QKeySequence>

#include <qzMultimedia/MediaPlayer.h>
#include <qzMultimedia/QtAudio.h>
#include <qzMultimedia/AudioOutput.h>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace qz {

static const QHash<HotkeyManager::Action, QString> kActionKeys = {
    {HotkeyManager::Action::PlayPause,    QStringLiteral("hotkey/play_pause")},
    {HotkeyManager::Action::SeekForward,  QStringLiteral("hotkey/seek_forward")},
    {HotkeyManager::Action::SeekBackward, QStringLiteral("hotkey/seek_backward")},
    {HotkeyManager::Action::Stop,         QStringLiteral("hotkey/stop")},
    {HotkeyManager::Action::SpeedUp,      QStringLiteral("hotkey/speed_up")},
    {HotkeyManager::Action::SpeedDown,    QStringLiteral("hotkey/speed_down")},
    {HotkeyManager::Action::VolumeUp,     QStringLiteral("hotkey/volume_up")},
    {HotkeyManager::Action::VolumeDown,   QStringLiteral("hotkey/volume_down")},
    {HotkeyManager::Action::Mute,         QStringLiteral("hotkey/mute")},
    {HotkeyManager::Action::Fullscreen,   QStringLiteral("hotkey/fullscreen")},
};

static const QHash<HotkeyManager::Action, QString> kDefaultKeys = {
    {HotkeyManager::Action::PlayPause,    QStringLiteral("Space")},
    {HotkeyManager::Action::SeekForward,  QStringLiteral("Right")},
    {HotkeyManager::Action::SeekBackward, QStringLiteral("Left")},
    {HotkeyManager::Action::Stop,         QStringLiteral("S")},
    {HotkeyManager::Action::SpeedUp,      QStringLiteral("Up")},
    {HotkeyManager::Action::SpeedDown,    QStringLiteral("Down")},
    {HotkeyManager::Action::VolumeUp,     QStringLiteral("Volume Up")},
    {HotkeyManager::Action::VolumeDown,   QStringLiteral("Volume Down")},
    {HotkeyManager::Action::Mute,         QStringLiteral("M")},
    {HotkeyManager::Action::Fullscreen,   QStringLiteral("F")},
};

static QKeyCombination keyFromString(const QString& str)
{
    if (str.isEmpty()) return {};
    const QKeySequence seq(str);
    if (seq.count() <= 0) return {};
    return QKeyCombination(seq[0].keyboardModifiers(), Qt::Key(seq[0].key()));
}

static QString stringFromKey(const QKeyCombination& combo)
{
    if (combo.key() == Qt::Key_unknown) return {};
    return QKeySequence(combo.toCombined()).toString();
}

HotkeyManager::HotkeyManager(QObject* parent)
    : QObject(parent)
    , m_settings(QStringLiteral("qzplay_hotkeys"))
{
    loadSettings();
}

HotkeyManager::~HotkeyManager()
{
    deactivate();
}

int HotkeyManager::mode() const
{
    return m_mode;
}

void HotkeyManager::setMode(int mode)
{
    if (m_mode == mode) return;

    const bool wasActive = m_active;
    if (wasActive) deactivate();

    m_mode = mode;
    m_settings.set(QStringLiteral("hotkey/mode"), mode);
    m_settings.flush();
    emit modeChanged();

    if (wasActive) activate();
}

QObject* HotkeyManager::mediaPlayer() const
{
    return m_mediaPlayer;
}

void HotkeyManager::setMediaPlayer(QObject* player)
{
    if (m_mediaPlayer == player) return;
    m_mediaPlayer = player;
    emit mediaPlayerChanged();
}

QObject* HotkeyManager::window() const
{
    return m_window;
}

void HotkeyManager::setWindow(QObject* window)
{
    if (m_window == window) return;

    if (m_active && static_cast<Mode>(m_mode) == Mode::WindowFocus) {
        removeWindowFocusMode();
    }

    m_window = window;
    emit windowChanged();

    if (m_active && static_cast<Mode>(m_mode) == Mode::WindowFocus) {
        applyWindowFocusMode();
    }
}

void HotkeyManager::setHotkey(Action action, const QString& keySequence)
{
    const QKeyCombination combo = keyFromString(keySequence);

    // 如果新按键已被其他 action 占用，清除旧绑定并从 ini 中删除
    if (combo.key() != Qt::Key_unknown) {
        for (auto it = m_hotkeys.begin(); it != m_hotkeys.end(); ++it) {
            if (it.key() != action && it.value() == combo) {
                it.value() = QKeyCombination();
                m_settings.remove(actionKey(it.key()));
                emit hotkeyChanged(it.key());
                break;
            }
        }
    }

    m_hotkeys[action] = combo;
    if (combo.key() == Qt::Key_unknown) {
        m_settings.remove(actionKey(action));
    } else {
        m_settings.set(actionKey(action), keySequence);
    }
    m_settings.flush();
    emit hotkeyChanged(action);

    NotificationManager::instance()->show(QObject::tr("快捷键已更新"));

    // Re-register global hotkeys if active
    if (m_active && static_cast<Mode>(m_mode) == Mode::Global) {
        unregisterGlobalHotkeys();
        registerGlobalHotkeys();
    }
}

QString HotkeyManager::hotkey(Action action) const
{
    if (const auto it = m_hotkeys.constFind(action); it != m_hotkeys.constEnd()) {
        return stringFromKey(it.value());
    }
    return {};
}

QString HotkeyManager::defaultHotkey(Action action) const
{
    return kDefaultKeys.value(action);
}

void HotkeyManager::resetToDefaults()
{
    m_hotkeys.clear();
    for (auto it = kDefaultKeys.constBegin(); it != kDefaultKeys.constEnd(); ++it) {
        m_hotkeys[it.key()] = keyFromString(it.value());
        m_settings.set(actionKey(it.key()), it.value());
        emit hotkeyChanged(it.key());
    }
    m_settings.flush();

    if (m_active && static_cast<Mode>(m_mode) == Mode::Global) {
        unregisterGlobalHotkeys();
        registerGlobalHotkeys();
    }
}

void HotkeyManager::activate()
{
    if (m_active) return;
    m_active = true;

    switch (static_cast<Mode>(m_mode)) {
    case Mode::WindowFocus:
        applyWindowFocusMode();
        break;
    case Mode::Global:
        registerGlobalHotkeys();
        break;
    default:
        break;
    }
}

void HotkeyManager::deactivate()
{
    if (!m_active) return;
    m_active = false;

    removeWindowFocusMode();
    unregisterGlobalHotkeys();
}

bool HotkeyManager::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyRelease) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (!keyEvent->isAutoRepeat() && m_actionPressed) {
            // 加速/减速松开后回归默认速度
            if (m_pressedAction == Action::SpeedUp || m_pressedAction == Action::SpeedDown) {
                auto* player = resolveMediaPlayer();
                if (player) player->setPlaybackRate(m_speedBeforeHold);
            }
            // 只有可长按的 action 才立即关闭通知，其他 action 显示 3s 后自动关闭
            if (isRepeatableAction(m_pressedAction)) {
                closeActionNotification();
            }
            m_actionPressed = false;
        }
        return QObject::eventFilter(watched, event);
    }

    if (event->type() != QEvent::KeyPress) {
        return QObject::eventFilter(watched, event);
    }

    auto* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->isAutoRepeat()) {
        // 仅可长按的 action 在自动重复时继续执行
        if (m_actionPressed && isRepeatableAction(m_pressedAction)) {
            executeAction(m_pressedAction);
            showActionNotification(m_pressedAction);
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

    const QKeyCombination pressed(keyEvent->modifiers(), Qt::Key(keyEvent->key()));

    for (auto it = m_hotkeys.constBegin(); it != m_hotkeys.constEnd(); ++it) {
        if (it.value() == pressed) {
            m_pressedAction = it.key();
            m_actionPressed = true;
            // 记住加速/减速前的速度
            if (m_pressedAction == Action::SpeedUp || m_pressedAction == Action::SpeedDown) {
                auto* player = resolveMediaPlayer();
                m_speedBeforeHold = player ? player->playbackRate() : kDefaultSpeed;
            }
            executeAction(it.key());
            showActionNotification(it.key());
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)

#ifdef Q_OS_WIN
    auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        const int hotkeyId = static_cast<int>(msg->wParam);
        for (auto it = m_globalHotkeyIds.constBegin(); it != m_globalHotkeyIds.constEnd(); ++it) {
            if (it.value() == hotkeyId) {
                executeAction(it.key());
                showActionNotification(it.key());
                return true;
            }
        }
    }
#endif
    return false;
}

void HotkeyManager::registerGlobalHotkeys()
{
#ifdef Q_OS_WIN
    if (!m_window) return;

    auto* win = qobject_cast<QWindow*>(m_window.data());
    if (!win) return;

    const WId hwnd = win->winId();

    for (auto it = m_hotkeys.constBegin(); it != m_hotkeys.constEnd(); ++it) {
        const QKeyCombination combo = it.value();
        if (combo.key() == Qt::Key_unknown) continue;

        quint32 mod = MOD_NOREPEAT;
        if (combo.keyboardModifiers() & Qt::ControlModifier) mod |= MOD_CONTROL;
        if (combo.keyboardModifiers() & Qt::AltModifier)     mod |= MOD_ALT;
        if (combo.keyboardModifiers() & Qt::ShiftModifier)   mod |= MOD_SHIFT;

        const UINT vk = static_cast<UINT>(combo.key());

        const int id = m_nextHotkeyId++;
        if (::RegisterHotKey(reinterpret_cast<HWND>(hwnd), id, mod, vk)) {
            m_globalHotkeyIds[it.key()] = id;
        }
    }

    QCoreApplication::instance()->installNativeEventFilter(this);
#else
    // Global hotkeys are only supported on Windows currently
#endif
}

void HotkeyManager::unregisterGlobalHotkeys()
{
#ifdef Q_OS_WIN
    if (!m_globalHotkeyIds.isEmpty()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }

    if (!m_window) {
        m_globalHotkeyIds.clear();
        return;
    }

    auto* win = qobject_cast<QWindow*>(m_window.data());
    if (!win) {
        m_globalHotkeyIds.clear();
        return;
    }

    const WId hwnd = win->winId();

    for (auto it = m_globalHotkeyIds.constBegin(); it != m_globalHotkeyIds.constEnd(); ++it) {
        ::UnregisterHotKey(reinterpret_cast<HWND>(hwnd), it.value());
    }
    m_globalHotkeyIds.clear();
#endif
}

void HotkeyManager::applyWindowFocusMode()
{
    if (!m_window) return;
    m_window->installEventFilter(this);
}

void HotkeyManager::removeWindowFocusMode()
{
    if (m_window) {
        m_window->removeEventFilter(this);
    }
}

void HotkeyManager::executeAction(Action action)
{
    auto* player = resolveMediaPlayer();
    if (!player) return;

    switch (action) {
    case Action::PlayPause:
        if (player->isPlaying()) {
            player->pause();
        } else {
            if (player->mediaStatus() == MediaPlayer::EndOfMedia) {
                player->setPosition(0);
            }
            player->play();
        }
        break;
    case Action::SeekForward:
        player->setPosition(std::min(player->duration(), player->position() + kSeekIntervalMs));
        break;
    case Action::SeekBackward:
        player->setPosition(std::max(qint64(0), player->position() - kSeekIntervalMs));
        break;
    case Action::Stop:
        player->stop();
        break;
    case Action::SpeedUp:
        player->setPlaybackRate(kSpeedUpRate);
        break;
    case Action::SpeedDown:
        player->setPlaybackRate(kSpeedDownRate);
        break;
    case Action::VolumeUp: {
        auto* audio = player->audioOutput();
        if (audio) audio->setVolume(std::min(1.0, audio->volume() + kVolumeStep / 100.0));
        break;
    }
    case Action::VolumeDown: {
        auto* audio = player->audioOutput();
        if (audio) audio->setVolume(std::max(0.0, audio->volume() - kVolumeStep / 100.0));
        break;
    }
    case Action::Mute: {
        auto* audio = player->audioOutput();
        if (audio) audio->setMuted(!audio->isMuted());
        break;
    }
    case Action::Fullscreen:
        if (m_window) {
            auto* win = qobject_cast<QWindow*>(m_window.data());
            if (win) {
                if (win->visibility() == QWindow::FullScreen) {
                    win->showNormal();
                } else {
                    win->showFullScreen();
                }
            }
        }
        break;
    }
}

MediaPlayer* HotkeyManager::resolveMediaPlayer() const
{
    if (!m_mediaPlayer) return nullptr;
    return qobject_cast<MediaPlayer*>(m_mediaPlayer.data());
}

void HotkeyManager::showActionNotification(Action action)
{
    auto* player = resolveMediaPlayer();
    auto* nm = NotificationManager::instance();

    switch (action) {
    case Action::PlayPause: {
        if (player && player->isPlaying()) {
            nm->show(tr("播放"), 3, QStringLiteral("/res/icons/player/IconamoonPlayerPauseFill.svg"));
        } else {
            nm->show(tr("已暂停"), 3, QStringLiteral("/res/icons/player/IconamoonPlayerPlayFill.svg"));
        }
        break;
    }
    case Action::SeekForward:
        if (player) nm->show(tr("快进 %1s").arg(kSeekIntervalMs / 1000), 3);
        break;
    case Action::SeekBackward:
        if (player) nm->show(tr("快退 %1s").arg(kSeekIntervalMs / 1000), 3);
        break;
    case Action::Stop:
        nm->show(tr("停止"), 3, QStringLiteral("/res/icons/player/MaterialSymbolsStopRounded.svg"));
        break;
    case Action::SpeedUp:
        nm->show(tr("3x 加速"), 0);
        break;
    case Action::SpeedDown:
        nm->show(tr("0.5x 减速"), 0);
        break;
    case Action::VolumeUp: {
        auto* audio = player ? player->audioOutput() : nullptr;
        if (audio) nm->show(tr("音量 %1%").arg(qRound(audio->volume() * 100)), 0);
        break;
    }
    case Action::VolumeDown: {
        auto* audio = player ? player->audioOutput() : nullptr;
        if (audio) nm->show(tr("音量 %1%").arg(qRound(audio->volume() * 100)), 0);
        break;
    }
    case Action::Mute: {
        auto* audio = player ? player->audioOutput() : nullptr;
        if (audio) {
            if (audio->isMuted()) {
                nm->show(tr("已静音"), 3);
            } else {
                nm->show(tr("已取消静音"), 3);
            }
        }
        break;
    }
    case Action::Fullscreen: {
        if (m_window) {
            auto* win = qobject_cast<QWindow*>(m_window.data());
            if (win && win->visibility() == QWindow::FullScreen) {
                nm->show(tr("全屏"), 3);
            } else {
                nm->show(tr("退出全屏"), 3);
            }
        }
        break;
    }
    }
}

void HotkeyManager::closeActionNotification()
{
    NotificationManager::instance()->close();
}

void HotkeyManager::loadSettings()
{
    m_settings.init(QStringLiteral("hotkey/mode"), static_cast<int>(Mode::Disabled));
    m_mode = m_settings.getInt(QStringLiteral("hotkey/mode"), static_cast<int>(Mode::Disabled));

    for (auto it = kActionKeys.constBegin(); it != kActionKeys.constEnd(); ++it) {
        const QString defaultKey = kDefaultKeys.value(it.key());
        m_settings.init(it.value(), defaultKey);
        const QString keyStr = m_settings.getString(it.value(), defaultKey);
        m_hotkeys[it.key()] = keyFromString(keyStr);
    }
}

void HotkeyManager::saveSettings()
{
    for (auto it = m_hotkeys.constBegin(); it != m_hotkeys.constEnd(); ++it) {
        m_settings.set(actionKey(it.key()), stringFromKey(it.value()));
    }
    m_settings.flush();
}

QString HotkeyManager::actionKey(Action action)
{
    return kActionKeys.value(action);
}

QString HotkeyManager::actionDefaultKey(Action action)
{
    return kDefaultKeys.value(action);
}

bool HotkeyManager::isRepeatableAction(Action action)
{
    return action == Action::SpeedUp
        || action == Action::SpeedDown
        || action == Action::VolumeUp
        || action == Action::VolumeDown;
}

} // namespace qz
