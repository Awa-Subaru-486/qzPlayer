// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_SUBTITLESTYLE_H
#define QT_VIDEO_SUBTITLESTYLE_H

#include <qzMultimedia/MultimediaGlobal.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>

class QZ_MULTIMEDIA_EXPORT SubtitleStyle
{
    Q_GADGET
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily)
    Q_PROPERTY(qreal fontSize READ fontSize WRITE setFontSize)
    Q_PROPERTY(QColor fontColor READ fontColor WRITE setFontColor)
    Q_PROPERTY(bool bold READ bold WRITE setBold)
    Q_PROPERTY(bool italic READ italic WRITE setItalic)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(qreal backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity)
    Q_PROPERTY(qreal cornerRadius READ cornerRadius WRITE setCornerRadius)
    Q_PROPERTY(qreal topMargin READ topMargin WRITE setTopMargin)
    Q_PROPERTY(qreal bottomMargin READ bottomMargin WRITE setBottomMargin)
    Q_PROPERTY(qreal leftMargin READ leftMargin WRITE setLeftMargin)
    Q_PROPERTY(qreal rightMargin READ rightMargin WRITE setRightMargin)
public:
    SubtitleStyle() = default;

    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family) { m_fontFamily = family; }

    qreal fontSize() const { return m_fontSize; }
    void setFontSize(qreal size) { m_fontSize = size; }

    QColor fontColor() const { return m_fontColor; }
    void setFontColor(const QColor &color) { m_fontColor = color; }

    bool bold() const { return m_bold; }
    void setBold(bool bold) { m_bold = bold; }

    bool italic() const { return m_italic; }
    void setItalic(bool italic) { m_italic = italic; }

    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor &color) { m_backgroundColor = color; }

    qreal backgroundOpacity() const { return m_backgroundOpacity; }
    void setBackgroundOpacity(qreal opacity) { m_backgroundOpacity = opacity; }

    qreal cornerRadius() const { return m_cornerRadius; }
    void setCornerRadius(qreal radius) { m_cornerRadius = radius; }

    qreal topMargin() const { return m_topMargin; }
    void setTopMargin(qreal margin) { m_topMargin = margin; }

    qreal bottomMargin() const { return m_bottomMargin; }
    void setBottomMargin(qreal margin) { m_bottomMargin = margin; }

    qreal leftMargin() const { return m_leftMargin; }
    void setLeftMargin(qreal margin) { m_leftMargin = margin; }

    qreal rightMargin() const { return m_rightMargin; }
    void setRightMargin(qreal margin) { m_rightMargin = margin; }

    bool operator==(const SubtitleStyle &other) const = default;
    bool operator!=(const SubtitleStyle &other) const = default;

private:
    QString m_fontFamily;
    qreal m_fontSize = 18;
    QColor m_fontColor = Qt::white;
    bool m_bold = false;
    bool m_italic = false;
    QColor m_backgroundColor = Qt::black;
    qreal m_backgroundOpacity = 0.5;
    qreal m_cornerRadius = 0.1;
    qreal m_topMargin = 0.05;
    qreal m_bottomMargin = 0.05;
    qreal m_leftMargin = 0.05;
    qreal m_rightMargin = 0.05;
};

Q_DECLARE_METATYPE(SubtitleStyle)

#endif
