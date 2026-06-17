/**
 * @file PlaylistModel.hpp
 * @brief 播放列表数据模型，提供媒体标题、URL、时长和封面给 QML 视图
 * @details 使用异步方式解析媒体元数据，支持 URL 去重
 */

#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QList>
#include <QSet>
#include <vector>

#include "qzPlayer_export.hpp"

namespace qz {

/**
 * @brief 播放列表条目数据结构
 */
struct PlaylistEntry
{
    QUrl url;
    QString title;
    qint64 duration = 0;      // 毫秒
    QUrl coverUrl;
};

class QZ_PLAYER_EXPORT PlaylistModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        UrlRole = Qt::UserRole + 1,
        DurationRole,
        CoverUrlRole,
    };

    explicit PlaylistModel(QObject *parent = nullptr);
    ~PlaylistModel() override;

    // QAbstractListModel interface
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 添加单个 URL 到播放列表
     * @param url 媒体文件 URL
     * @return true 如果成功添加，false 如果 URL 已存在
     */
    Q_INVOKABLE bool addUrl(const QUrl &url);

    /**
     * @brief 添加多个 URL 到播放列表
     * @param urls 媒体文件 URL 列表
     * @return 实际添加的 URL 数量（已存在的 URL 会被跳过）
     */
    Q_INVOKABLE int addUrls(const QList<QUrl> &urls);

    /**
     * @brief 更新指定行的媒体信息（线程安全）
     * @param row 行号
     * @param title 标题
     * @param duration 时长（毫秒）
     * @param coverUrl 封面 URL
     */
    Q_INVOKABLE void updateEntry(int row, const QString &title, qint64 duration, const QUrl &coverUrl);

    /**
     * @brief 清空播放列表
     */
    Q_INVOKABLE void clear();

    /**
     * @brief 检查 URL 是否已存在于播放列表中
     * @param url 要检查的 URL
     * @return true 如果已存在，false 如果不存在
     */
    Q_INVOKABLE bool containsUrl(const QUrl &url) const;

    // 封面缓存管理
    static QString coverCacheDir();
    static void clearCoverCache();

private:
    /**
     * @brief 启动异步解析媒体文件元数据
     * @param urls 要解析的 URL 列表
     * @param startIndex 这些 URL 在播放列表中的起始索引
     */
    void startAsyncParse(const QList<QUrl> &urls, int startIndex);

    // 单个媒体文件的解析结果
    struct ParseResult {
        int index;
        QString title;
        qint64 duration = 0;
        QUrl coverUrl;
        bool success = false;
    };

    // 数据存储
    std::vector<PlaylistEntry> m_entries;

    // URL 集合，用于快速去重检查（数据代理）
    QSet<QUrl> m_urlSet;

    // 活跃的解析器列表
    QList<QObject*> m_activeParsers;
};

} // namespace qz
