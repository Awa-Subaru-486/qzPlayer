/**
 * @file FileDropArea.hpp
 * @brief 文件拖放区域组件，支持从系统文件管理器拖入文件
 */

#pragma once

#include <QQuickItem>
#include <QUrl>

#include "qzPlayer_export.hpp"

namespace qz {

    class QZ_PLAYER_EXPORT FileDropArea : public QQuickItem
    {
        Q_OBJECT

        Q_PROPERTY(bool containsDrag READ containsDrag NOTIFY containsDragChanged)
        Q_PROPERTY(QStringList fileExtensions READ fileExtensions WRITE set_fileExtensions NOTIFY fileExtensionsChanged)

    public:
        explicit FileDropArea(QQuickItem *parent = nullptr);

        bool containsDrag() const;

        QStringList fileExtensions() const;
        void set_fileExtensions(const QStringList &extensions);

    Q_SIGNALS:
        void containsDragChanged();
        void fileExtensionsChanged();
        void filesDropped(const QStringList &filePaths);
        void entered();
        void exited();

    protected:
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dragMoveEvent(QDragMoveEvent *event) override;
        void dragLeaveEvent(QDragLeaveEvent *event) override;
        void dropEvent(QDropEvent *event) override;

    private:
        bool hasValidFiles(const class QMimeData *mimeData) const;
        QStringList extractFilePaths(const class QMimeData *mimeData) const;

        bool m_containsDrag = false;
        QStringList m_fileExtensions;
    };

} // namespace qz
