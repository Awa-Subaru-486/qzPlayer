/**
 * @file PlaylistModel.cpp
 * @brief 播放列表数据模型实现
 * @details 使用异步方式解析媒体元数据，支持 URL 去重
 */

#include "PlaylistModel.hpp"

#include <QMetaObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QDateTime>
#include <QCoreApplication>
#include <QThread>
#include <utility>
#include <qzMultimedia/MediaPlayer.h>
#include <qzMultimedia/MediaMetadata.h>
import qzLog;

namespace qz {

// ============================================================
// 封面缓存目录
// ============================================================

QString PlaylistModel::coverCacheDir()
{
    static QString cacheDir;
    if (cacheDir.isEmpty()) {
#ifdef Q_OS_ANDROID
        cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/covers");
#else
        cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/covers");
#endif
        (void)QDir().mkpath(cacheDir);
    }
    return cacheDir;
}

void PlaylistModel::clearCoverCache()
{
    const QString dir = coverCacheDir();
    QDir d(dir);
    if (!d.exists())
        return;

    const QStringList files = d.entryList(QDir::Files);
    for (const QString &file : files) {
        d.remove(file);
    }
}

// ============================================================
// PlaylistModel
// ============================================================

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

PlaylistModel::~PlaylistModel()
{
    // 清理所有活跃的解析器
    for (auto *parser : m_activeParsers) {
        if (parser) {
            parser->deleteLater();
        }
    }
    m_activeParsers.clear();
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};

    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case TitleRole:
        return entry.title;
    case UrlRole:
        return entry.url;
    case DurationRole:
        return entry.duration;
    case CoverUrlRole:
        return entry.coverUrl;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {UrlRole, "url"},
        {DurationRole, "duration"},
        {CoverUrlRole, "coverUrl"},
    };
}

bool PlaylistModel::addUrl(const QUrl &url)
{
    if (!url.isValid() || m_urlSet.contains(url))
        return false;

    const int row = static_cast<int>(m_entries.size());

    beginInsertRows(QModelIndex(), row, row);

    PlaylistEntry entry;
    entry.url = url;
    entry.title = url.fileName();
    m_entries.push_back(entry);
    m_urlSet.insert(url);

    endInsertRows();

    // 启动异步解析
    startAsyncParse({url}, row);

    return true;
}

int PlaylistModel::addUrls(const QList<QUrl> &urls)
{
    if (urls.isEmpty())
        return 0;

    // 过滤出新的 URL
    QList<QUrl> newUrls;
    newUrls.reserve(urls.size());

    for (const auto &url : urls) {
        if (url.isValid() && !m_urlSet.contains(url)) {
            newUrls.append(url);
        }
    }

    if (newUrls.isEmpty())
        return 0;

    const int startIndex = static_cast<int>(m_entries.size());
    const int endIndex = startIndex + static_cast<int>(newUrls.size()) - 1;

    beginInsertRows(QModelIndex(), startIndex, endIndex);

    for (const auto &url : newUrls) {
        PlaylistEntry entry;
        entry.url = url;
        entry.title = url.fileName();
        m_entries.push_back(entry);
        m_urlSet.insert(url);
    }

    endInsertRows();

    // 启动异步解析
    startAsyncParse(newUrls, startIndex);

    return static_cast<int>(newUrls.size());
}

void PlaylistModel::updateEntry(int row, const QString &title, qint64 duration, const QUrl &coverUrl)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size()))
        return;

    auto &entry = m_entries[row];
    bool changed = false;

    if (entry.title != title) {
        entry.title = title;
        changed = true;
    }
    if (entry.duration != duration) {
        entry.duration = duration;
        changed = true;
    }
    if (entry.coverUrl != coverUrl) {
        entry.coverUrl = coverUrl;
        changed = true;
    }

    if (!changed)
        return;

    // 确保在 GUI 线程中发出 dataChanged
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, [this, row, title, duration, coverUrl]() {
            updateEntry(row, title, duration, coverUrl);
        }, Qt::QueuedConnection);
        return;
    }

    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {TitleRole, DurationRole, CoverUrlRole});
}

void PlaylistModel::clear()
{
    beginResetModel();
    m_entries.clear();
    m_urlSet.clear();
    endResetModel();
}

bool PlaylistModel::containsUrl(const QUrl &url) const
{
    return m_urlSet.contains(url);
}

void PlaylistModel::startAsyncParse(const QList<QUrl> &urls, int startIndex)
{
    if (urls.isEmpty())
        return;

    // 为每个 URL 创建一个 MediaPlayer 进行异步解析
    for (int i = 0; i < urls.size(); ++i) {
        const int index = startIndex + i;
        const QUrl &url = urls[i];

        auto *player = new MediaPlayer(this);
        m_activeParsers.append(player);

        // 连接媒体状态变化信号
        connect(player, &MediaPlayer::mediaStatusChanged, this, [this, player, index, url](MediaPlayer::MediaStatus status) {
            if (status == MediaPlayer::LoadedMedia) {
                const auto md = player->metaData();

                // 标题
                QString title = md.stringValue(MediaMetaData::Title);
                if (title.isEmpty())
                    title = url.fileName();

                // 时长（毫秒）
                qint64 duration = 0;
                if (md.value(MediaMetaData::Duration).isValid())
                    duration = md.value(MediaMetaData::Duration).toLongLong();

                // 封面
                QUrl coverUrl;
                const QString cacheDir = coverCacheDir();
                const QString fileName = QString("%1.jpg")
                    .arg(QString::fromUtf8(QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Md5).toHex()));
                const QString filePath = cacheDir + "/" + fileName;
                if (QFile::exists(filePath)) {
                    coverUrl = QUrl::fromLocalFile(filePath);
                } else {
                    const QImage cover = player->getMediaCover({}, true);
                    if (!cover.isNull() && !cacheDir.isEmpty()) {
                        if (cover.save(filePath, "JPEG", 85)) {
                            coverUrl = QUrl::fromLocalFile(filePath);
                        } else {
                            Log::warn("封面保存失败: %1", filePath);
                        }
                    }
                }

                updateEntry(index, title, duration, coverUrl);

                // 清理
                m_activeParsers.removeOne(player);
                player->deleteLater();
            } else if (status == MediaPlayer::InvalidMedia || status == MediaPlayer::EndOfMedia) {
                // 解析失败，清理
                m_activeParsers.removeOne(player);
                player->deleteLater();
            }
        });

        // 连接错误信号
        connect(player, &MediaPlayer::errorChanged, this, [this, player]() {
            m_activeParsers.removeOne(player);
            player->deleteLater();
        });

        // 开始加载媒体
        player->setSource(url);
    }
}

} // namespace qz
