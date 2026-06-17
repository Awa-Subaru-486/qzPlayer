/**
 * @file FileDropArea.cpp
 * @brief 文件拖放区域组件实现
 */

#include "FileDropArea.hpp"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <qfileinfo.h>
#include <QMimeData>

namespace qz {

FileDropArea::FileDropArea(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemAcceptsDrops);
}

bool FileDropArea::containsDrag() const
{
    return m_containsDrag;
}

QStringList FileDropArea::fileExtensions() const
{
    return m_fileExtensions;
}

void FileDropArea::set_fileExtensions(const QStringList &extensions)
{
    if (m_fileExtensions == extensions)
        return;
    m_fileExtensions = extensions;
    emit fileExtensionsChanged();
}

void FileDropArea::dragEnterEvent(QDragEnterEvent *event)
{
    if (hasValidFiles(event->mimeData()))
    {
        event->acceptProposedAction();
        if (!m_containsDrag)
        {
            m_containsDrag = true;
            emit containsDragChanged();
            emit entered();
        }
    }
    else
    {
        event->ignore();
    }
}

void FileDropArea::dragMoveEvent(QDragMoveEvent *event)
{
    if (hasValidFiles(event->mimeData()))
        event->acceptProposedAction();
    else
        event->ignore();
}

void FileDropArea::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    if (m_containsDrag)
    {
        m_containsDrag = false;
        emit containsDragChanged();
        emit exited();
    }
}

void FileDropArea::dropEvent(QDropEvent *event)
{
    if (!hasValidFiles(event->mimeData()))
    {
        event->ignore();
        return;
    }

    event->acceptProposedAction();

    const QStringList paths = extractFilePaths(event->mimeData());
    if (!paths.isEmpty())
        emit filesDropped(paths);

    if (m_containsDrag)
    {
        m_containsDrag = false;
        emit containsDragChanged();
        emit exited();
    }
}

bool FileDropArea::hasValidFiles(const QMimeData *mimeData) const
{
    if (!mimeData || !mimeData->hasUrls())
        return false;

    const auto urls = mimeData->urls();
    for (const auto &url : urls)
    {
        if (!url.isLocalFile())
            continue;

        if (m_fileExtensions.isEmpty())
            return true;

        const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        for (const auto &ext : m_fileExtensions)
        {
            if (suffix == ext.toLower())
                return true;
        }
    }

    return false;
}

QStringList FileDropArea::extractFilePaths(const QMimeData *mimeData) const
{
    QStringList paths;
    const auto urls = mimeData->urls();
    for (const auto &url : urls)
    {
        if (!url.isLocalFile())
            continue;

        const QString path = url.toLocalFile();
        if (m_fileExtensions.isEmpty())
        {
            paths.append(path);
            continue;
        }

        const QString suffix = QFileInfo(path).suffix().toLower();
        for (const auto &ext : m_fileExtensions)
        {
            if (suffix == ext.toLower())
            {
                paths.append(path);
                break;
            }
        }
    }
    return paths;
}

} // namespace qz
