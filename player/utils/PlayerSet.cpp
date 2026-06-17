#include "PlayerSet.hpp"

namespace qz {

PlayerSet* PlayerSet::instance()
{
    static PlayerSet set;
    return &set;
}

PlayerSet::PlayerSet(QObject* parent)
    : QObject(parent)
    , m_settings(QStringLiteral("qzplayer_set"))
{
    m_settings.init(QStringLiteral("skipEnabled"), false);
    m_settings.init(QStringLiteral("openingDuration"), 30.0);
    m_settings.init(QStringLiteral("endingDuration"), 30.0);
    m_settings.init(QStringLiteral("theme"), m_settings.enumToString(ThemeType::Type::System));
    m_theme = m_settings.stringToEnum<ThemeType::Type>(m_settings.getString(QStringLiteral("theme"), QString()), ThemeType::Type::System);
    m_settings.init(QStringLiteral("hardwareDecoderEnabled"), true);
    m_hardwareDecoderEnabled = m_settings.getBool(QStringLiteral("hardwareDecoderEnabled"), true);
    m_settings.init(QStringLiteral("hdrEnabled"), false);
    m_hdrEnabled = m_settings.getBool(QStringLiteral("hdrEnabled"), false);
    m_settings.init(QStringLiteral("zeroCopyEnabled"), true);
    m_zeroCopyEnabled = m_settings.getBool(QStringLiteral("zeroCopyEnabled"), true);
    m_settings.init(QStringLiteral("lowLatencyStreamingEnabled"), false);
    m_lowLatencyStreamingEnabled = m_settings.getBool(QStringLiteral("lowLatencyStreamingEnabled"), false);
#ifdef Q_OS_WINDOWS
    m_settings.init(QStringLiteral("decoderPriority"), QStringLiteral("2,1,0"));
    m_decoderPriority = m_settings.getString(QStringLiteral("decoderPriority"), QStringLiteral("2,1,0"));
#elif defined(Q_OS_ANDROID)
    m_settings.init(QStringLiteral("decoderPriority"), QStringLiteral("3,2,0"));
    m_decoderPriority = m_settings.getString(QStringLiteral("decoderPriority"), QStringLiteral("3,2,0"));
#else
    m_settings.init(QStringLiteral("decoderPriority"), QStringLiteral("2,0"));
    m_decoderPriority = m_settings.getString(QStringLiteral("decoderPriority"), QStringLiteral("2,0"));
#endif
}

bool PlayerSet::skipEnabled() const
{
    return m_skipEnabled;
}

void PlayerSet::setSkipEnabled(bool enabled)
{
    if (std::exchange(m_skipEnabled, enabled) != enabled) {
        m_settings.set(QStringLiteral("skipEnabled"), enabled);
        m_settings.flush();
        emit skipEnabledChanged(enabled);
    }
}

qreal PlayerSet::openingDuration() const
{
    return m_openingDuration;
}

void PlayerSet::setOpeningDuration(qreal seconds)
{
    if (std::exchange(m_openingDuration, seconds) != seconds) {
        m_settings.set(QStringLiteral("openingDuration"), seconds);
        m_settings.flush();
        emit openingDurationChanged(seconds);
    }
}

qreal PlayerSet::endingDuration() const
{
    return m_endingDuration;
}

void PlayerSet::setEndingDuration(qreal seconds)
{
    if (std::exchange(m_endingDuration, seconds) != seconds) {
        m_settings.set(QStringLiteral("endingDuration"), seconds);
        m_settings.flush();
        emit endingDurationChanged(seconds);
    }
}

ThemeType::Type PlayerSet::theme() const
{
    return m_theme;
}

void PlayerSet::setTheme(ThemeType::Type theme)
{
    if (std::exchange(m_theme, theme) != theme) {
        m_settings.set(QStringLiteral("theme"), m_settings.enumToString(theme));
        m_settings.flush();
        emit themeChanged(theme);
    }
}

bool PlayerSet::hardwareDecoderEnabled() const
{
    return m_hardwareDecoderEnabled;
}

void PlayerSet::setHardwareDecoderEnabled(bool enabled)
{
    if (std::exchange(m_hardwareDecoderEnabled, enabled) != enabled) {
        m_settings.set(QStringLiteral("hardwareDecoderEnabled"), enabled);
        m_settings.flush();
        emit hardwareDecoderEnabledChanged(enabled);
    }
}

bool PlayerSet::hdrEnabled() const
{
    return m_hdrEnabled;
}

void PlayerSet::setHdrEnabled(bool enabled)
{
    if (std::exchange(m_hdrEnabled, enabled) != enabled) {
        m_settings.set(QStringLiteral("hdrEnabled"), enabled);
        m_settings.flush();
        emit hdrEnabledChanged(enabled);
    }
}

bool PlayerSet::zeroCopyEnabled() const
{
    return m_zeroCopyEnabled;
}

void PlayerSet::setZeroCopyEnabled(bool enabled)
{
    if (std::exchange(m_zeroCopyEnabled, enabled) != enabled) {
        m_settings.set(QStringLiteral("zeroCopyEnabled"), enabled);
        m_settings.flush();
        emit zeroCopyEnabledChanged(enabled);
    }
}

bool PlayerSet::lowLatencyStreamingEnabled() const
{
    return m_lowLatencyStreamingEnabled;
}

void PlayerSet::setLowLatencyStreamingEnabled(bool enabled)
{
    if (std::exchange(m_lowLatencyStreamingEnabled, enabled) != enabled) {
        m_settings.set(QStringLiteral("lowLatencyStreamingEnabled"), enabled);
        m_settings.flush();
        emit lowLatencyStreamingEnabledChanged(enabled);
    }
}

QString PlayerSet::decoderPriority() const
{
    return m_decoderPriority;
}

void PlayerSet::setDecoderPriority(const QString &priority)
{
    if (std::exchange(m_decoderPriority, priority) != priority) {
        m_settings.set(QStringLiteral("decoderPriority"), priority);
        m_settings.flush();
        emit decoderPriorityChanged(priority);
    }
}

} // namespace qz
