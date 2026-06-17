#include "Settings.hpp"

#include <QStandardPaths>
#include <QDir>

namespace qz {

static QString getAppDataPath()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return path;
}

Settings::Settings(const QString& name)
    : m_settings(getAppDataPath() + QDir::separator() + name + QStringLiteral(".ini"),
                 QSettings::IniFormat)
{
    qRegisterMetaType<Option>("Option");
}

Settings::Settings(const QString& name, bool)
    : m_settings(getAppDataPath() + QDir::separator() + name + QStringLiteral(".ini"),
                 QSettings::IniFormat)
{
}

Settings::~Settings()
{
    std::lock_guard lock(m_mutex);
    flushCache();
}

auto Settings::init(const QString &key, const QVariant &val) -> void
{
    std::lock_guard lock(m_mutex);
    if (!m_cache.contains(key) && !m_settings.contains(key))
        m_cache[key] = val;
}

auto Settings::set(const QString &key, const QVariant &val) -> void
{
    std::lock_guard lock(m_mutex);
    if (!m_cache.contains(key) && !m_settings.contains(key))
        return;

    m_cache[key] = val;
}

auto Settings::remove(const QString &key) -> void
{
    std::lock_guard lock(m_mutex);
    m_cache.erase(key);
    m_settings.remove(key);
}

auto Settings::get(const QString &key, const QVariant &def) const -> QVariant
{
    std::lock_guard lock(m_mutex);
    if (const auto it = m_cache.find(key); it != m_cache.end())
        return it->second;
    return m_settings.value(key, def);
}

auto Settings::flush() -> void
{
    std::lock_guard lock(m_mutex);
    flushCache();
    m_settings.sync();
}

auto Settings::getOption(const QString &key) const -> Option
{
    if (const auto v = get(key, QVariant::fromValue(Option::Auto));
        v.canConvert<Option>())
    {
        return v.value<Option>();
    }
    return Option::Auto;
}

auto Settings::flushCache() -> void
{
    for (const auto& [key, value] : m_cache) {
        m_settings.setValue(key, value);
    }
    m_cache.clear();
}

} // namespace qz
