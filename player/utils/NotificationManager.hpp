#pragma once

#include <QObject>
#include <QColor>
#include <QUrl>
#include <QtQml/qqml.h>

#include "qzPlayer_export.hpp"

namespace qz {

class QZ_PLAYER_EXPORT NotificationManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(NotificationManager)

public:
    static NotificationManager* instance();

    Q_INVOKABLE void show(const QString& text, int duration = 3,
                          const QString& icon = QString(),
                          const QColor& color = QColor());
    Q_INVOKABLE void close();

signals:
    void notificationShown(const QString& text, int duration,
                           const QString& icon, const QColor& color);
    void notificationClosed();

private:
    explicit NotificationManager(QObject* parent = nullptr);
};

} // namespace qz
