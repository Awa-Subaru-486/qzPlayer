module;

#include <QtQml/qqmlengine.h>
#include <qqml.h>
#include "qzPlayer_export.hpp"
#include "CoverArtItem.hpp"
#include "NotificationManager.hpp"
#ifndef Q_OS_ANDROID
#include "FileDropArea.hpp"
#include "HotkeyManager.hpp"
#endif
#include "InputMonitor.hpp"
#include "PlaylistModel.hpp"
#include "PlayerSet.hpp"
#include "SvgRenderer.hpp"
#include "VulkanWindowInitializer.hpp"

#ifdef Q_OS_ANDROID
#include "AndroidUtils.hpp"
#endif

export module qml_register_types_qzPlayer;

// import Test1Item;
// import Test2Item;

namespace qz {
    constexpr auto qz_player_uri = "qz.player";
    constexpr int version_major = 1;
    constexpr int version_minor = 0;
#define qz_player_url_ma_mi qz_player_uri, version_major, version_minor
}

export namespace qz
{
    QZ_PLAYER_EXPORT auto qml_register_types_qz_player(QQmlEngine *) -> void
    {
        qmlRegisterType<CoverArtItem>(qz_player_url_ma_mi, "CoverArtItem");
        qmlRegisterType<SvgRenderer>(qz_player_url_ma_mi, "SvgRenderer");
        qmlRegisterType<InputMonitor>(qz_player_url_ma_mi, "InputMonitor");
#ifndef Q_OS_ANDROID
        qmlRegisterType<FileDropArea>(qz_player_url_ma_mi, "FileDropArea");
        qmlRegisterType<HotkeyManager>(qz_player_url_ma_mi, "HotkeyManager");
#endif
        qmlRegisterType<PlaylistModel>(qz_player_url_ma_mi, "PlaylistModel");

        // qmlRegisterType<Test1Item>(qz_player_url_ma_mi, "Test1Item");
        // qmlRegisterType<Test2Item>(qz_player_url_ma_mi, "Test2Item");

#ifdef Q_OS_ANDROID
        qmlRegisterType<AndroidUtils>(qz_player_url_ma_mi, "AndroidUtils");
#endif

        qmlRegisterSingletonInstance<NotificationManager>(qz_player_url_ma_mi, "NotificationManager", NotificationManager::instance());
        qmlRegisterSingletonInstance<PlayerSet>(qz_player_url_ma_mi, "PlayerSet", PlayerSet::instance());
        qmlRegisterSingletonInstance<VulkanWindowInitializer>(qz_player_url_ma_mi, "VulkanWindowInitializer", VulkanWindowInitializer::instance());
    }
}
