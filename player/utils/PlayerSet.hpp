#pragma once

#include <QObject>
#include <QtQml/qqml.h>

#include "Settings.hpp"
#include "ThemeTypes.hpp"
#include "qzPlayer_export.hpp"

namespace qz {

class QZ_PLAYER_EXPORT PlayerSet : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(PlayerSet)

    Q_PROPERTY(bool skipEnabled READ skipEnabled WRITE setSkipEnabled NOTIFY skipEnabledChanged FINAL)
    Q_PROPERTY(qreal openingDuration READ openingDuration WRITE setOpeningDuration NOTIFY openingDurationChanged FINAL)
    Q_PROPERTY(qreal endingDuration READ endingDuration WRITE setEndingDuration NOTIFY endingDurationChanged FINAL)
    Q_PROPERTY(ThemeType::Type theme READ theme WRITE setTheme NOTIFY themeChanged FINAL)
    Q_PROPERTY(bool hardwareDecoderEnabled READ hardwareDecoderEnabled WRITE setHardwareDecoderEnabled NOTIFY hardwareDecoderEnabledChanged FINAL)
    Q_PROPERTY(bool hdrEnabled READ hdrEnabled WRITE setHdrEnabled NOTIFY hdrEnabledChanged FINAL)
    Q_PROPERTY(bool zeroCopyEnabled READ zeroCopyEnabled WRITE setZeroCopyEnabled NOTIFY zeroCopyEnabledChanged FINAL)
    Q_PROPERTY(bool lowLatencyStreamingEnabled READ lowLatencyStreamingEnabled WRITE setLowLatencyStreamingEnabled NOTIFY lowLatencyStreamingEnabledChanged FINAL)
    Q_PROPERTY(QString decoderPriority READ decoderPriority WRITE setDecoderPriority NOTIFY decoderPriorityChanged FINAL)

public:
    static PlayerSet* instance();

    [[nodiscard]] bool skipEnabled() const;
    void setSkipEnabled(bool enabled);

    [[nodiscard]] qreal openingDuration() const;
    void setOpeningDuration(qreal seconds);

    [[nodiscard]] qreal endingDuration() const;
    void setEndingDuration(qreal seconds);

    [[nodiscard]] ThemeType::Type theme() const;
    void setTheme(ThemeType::Type theme);

    [[nodiscard]] bool hardwareDecoderEnabled() const;
    void setHardwareDecoderEnabled(bool enabled);

    [[nodiscard]] bool hdrEnabled() const;
    void setHdrEnabled(bool enabled);

    [[nodiscard]] bool zeroCopyEnabled() const;
    void setZeroCopyEnabled(bool enabled);

    [[nodiscard]] bool lowLatencyStreamingEnabled() const;
    void setLowLatencyStreamingEnabled(bool enabled);

    [[nodiscard]] QString decoderPriority() const;
    void setDecoderPriority(const QString &priority);

signals:
    void skipEnabledChanged(bool enabled);
    void openingDurationChanged(qreal seconds);
    void endingDurationChanged(qreal seconds);
    void themeChanged(ThemeType::Type theme);
    void hardwareDecoderEnabledChanged(bool enabled);
    void hdrEnabledChanged(bool enabled);
    void zeroCopyEnabledChanged(bool enabled);
    void lowLatencyStreamingEnabledChanged(bool enabled);
    void decoderPriorityChanged(const QString &priority);

private:
    explicit PlayerSet(QObject* parent = nullptr);

    Settings m_settings;
    bool m_skipEnabled = false;
    qreal m_openingDuration = 90.0;
    qreal m_endingDuration = 90.0;
    ThemeType::Type m_theme = ThemeType::Type::System;
    bool m_hardwareDecoderEnabled = true;
    bool m_hdrEnabled = false;
    bool m_zeroCopyEnabled = true;
    bool m_lowLatencyStreamingEnabled = false;
    QString m_decoderPriority;
};

} // namespace qz
