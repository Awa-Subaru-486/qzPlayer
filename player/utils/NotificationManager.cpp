#include "NotificationManager.hpp"

namespace qz {

NotificationManager* NotificationManager::instance()
{
    static NotificationManager mgr;
    return &mgr;
}

NotificationManager::NotificationManager(QObject* parent)
    : QObject(parent)
{
}

void NotificationManager::show(const QString& text, int duration,
                               const QString& icon, const QColor& color)
{
    emit notificationShown(text, duration, icon, color);
}

void NotificationManager::close()
{
    emit notificationClosed();
}

} // namespace qz
