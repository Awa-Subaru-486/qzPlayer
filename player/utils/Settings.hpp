#pragma once

#include <QMetaEnum>
#include <QSettings>
#include <QVariant>
#include <mutex>
#include <unordered_map>

#include "qzPlayer_export.hpp"

namespace qz {

using SettingsMap = std::unordered_map<QString, QVariant>;

class QZ_PLAYER_EXPORT Settings
{
    Q_GADGET

public:
    enum class Option : int
    {
        Disable = 0,
        Auto = 1,
        Force = 2
    };
    Q_ENUM(Option)

public:
    explicit Settings(const QString& name);
    virtual ~Settings();

    auto init(const QString &key, const QVariant &val) -> void;
    auto set(const QString &key, const QVariant &val) -> void;
    auto remove(const QString &key) -> void;
    virtual auto flush() -> void;

    [[nodiscard]] auto get(const QString &key, const QVariant &def) const -> QVariant;
    [[nodiscard]] auto getOption(const QString &key) const -> Option;

    [[nodiscard]] auto getBool(const QString &key, const QVariant &def) const -> bool
    {
        return get(key, def).toBool();
    }

    [[nodiscard]] auto getInt(const QString &key, const QVariant &def) const -> int
    {
        return get(key, def).toInt();
    }

    [[nodiscard]] auto getString(const QString &key, const QVariant &def) const -> QString
    {
        return get(key, def).toString();
    }

    template <typename Enum>
    auto enumToString(Enum value) const -> QString;

    template <typename Enum>
    auto stringToEnum(const QString &str, Enum defaultValue) const -> Enum;

protected:
    explicit Settings(const QString& name, bool);

    auto flushCache() -> void;

private:
    QSettings m_settings;
    mutable std::mutex m_mutex;
    SettingsMap m_cache;
};

template <typename Enum>
auto Settings::enumToString(Enum value) const -> QString
{
    static_assert(std::is_enum_v<Enum>, "Template argument must be an enum type.");

    const QMetaEnum metaEnum = QMetaEnum::fromType<Enum>();
    const char* key = metaEnum.valueToKey(static_cast<std::underlying_type_t<Enum>>(value));
    return key ? QString::fromUtf8(key) : QString();
}

template <typename Enum>
auto Settings::stringToEnum(const QString &str, Enum defaultValue) const -> Enum
{
    static_assert(std::is_enum_v<Enum>, "Template argument must be an enum type.");

    const QMetaEnum metaEnum = QMetaEnum::fromType<Enum>();
    bool ok = false;
    const int value = metaEnum.keyToValue(str.toUtf8().constData(), &ok);
    return ok ? static_cast<Enum>(value) : defaultValue;
}

} // namespace qz
