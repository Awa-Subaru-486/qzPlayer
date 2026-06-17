// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef SGAUDIOVISUALIZERNODE_P_H
#define SGAUDIOVISUALIZERNODE_P_H

#include <QtQuick/qsgnode.h>
#include <QtQuick/qsggeometry.h>
#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

// 音频可视化场景图节点：双声道波形线（左声道向上，右声道向下）
class SGAudioVisualizerNode : public QSGGeometryNode
{
public:
    SGAudioVisualizerNode();
    ~SGAudioVisualizerNode() override;

    void setSpectrumData(const float *leftData, int leftSize,
                         const float *rightData, int rightSize);
    void setColor(const QColor &color);
    void setRect(const QRectF &rect);

private:
    void updateGeometry();

    QSGGeometry m_geometry;
    QColor m_color;
    QRectF m_rect;

    QVector<float> m_leftBuffer;
    QVector<float> m_rightBuffer;
    bool m_spectrumDirty = false;
    bool m_rectDirty = true;
};

QT_END_NAMESPACE

#endif
