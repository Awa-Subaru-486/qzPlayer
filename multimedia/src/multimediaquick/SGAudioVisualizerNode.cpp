// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "SGAudioVisualizerNode_p.h"
#include <QtQuick/qsgflatcolormaterial.h>

QT_BEGIN_NAMESPACE

SGAudioVisualizerNode::SGAudioVisualizerNode()
    : m_geometry(QSGGeometry::defaultAttributes_Point2D(), 2)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawLines);
    m_geometry.setLineWidth(2.0f);
    // 初始化2个顶点避免0顶点问题
    m_geometry.vertexDataAsPoint2D()[0].set(0, 0);
    m_geometry.vertexDataAsPoint2D()[1].set(0, 0);
    setGeometry(&m_geometry);

    auto *mat = new QSGFlatColorMaterial;
    mat->setColor(m_color);
    setMaterial(mat);
    setFlag(OwnsMaterial);
}

SGAudioVisualizerNode::~SGAudioVisualizerNode() = default;

void SGAudioVisualizerNode::setSpectrumData(const float *leftData, int leftSize,
                                             const float *rightData, int rightSize)
{
    m_leftBuffer.resize(leftSize);
    memcpy(m_leftBuffer.data(), leftData, leftSize * sizeof(float));
    m_rightBuffer.resize(rightSize);
    memcpy(m_rightBuffer.data(), rightData, rightSize * sizeof(float));
    m_spectrumDirty = true;
    updateGeometry();
}

void SGAudioVisualizerNode::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    static_cast<QSGFlatColorMaterial *>(material())->setColor(m_color);
    markDirty(DirtyMaterial);
}

void SGAudioVisualizerNode::setRect(const QRectF &rect)
{
    if (m_rect == rect)
        return;
    m_rect = rect;
    m_rectDirty = true;
    updateGeometry();
}

void SGAudioVisualizerNode::updateGeometry()
{
    if (m_leftBuffer.isEmpty() || m_rect.isEmpty())
        return;

    const float x = m_rect.x();
    const float w = m_rect.width();
    const float y = m_rect.y();
    const float h = m_rect.height();
    const float upperCenter = y + h * 0.35f;
    const float lowerCenter = y + h * 0.65f;

    // DrawLines 模式：每条线段2个顶点
    // 左声道 N 个点 → N-1 条线段 → 2*(N-1) 个顶点
    // 右声道同理
    const int leftCount = m_leftBuffer.size();
    const int rightCount = m_rightBuffer.size();
    const int vertexCount = 2 * (leftCount - 1) + 2 * (rightCount - 1);
    m_geometry.allocate(vertexCount);

    auto *v = m_geometry.vertexDataAsPoint2D();
    int idx = 0;

    // 左声道：上方区域，从中线向上波动
    for (int i = 0; i < leftCount - 1; ++i) {
        float fx0 = x + (float(i) / (leftCount - 1)) * w;
        float fx1 = x + (float(i + 1) / (leftCount - 1)) * w;
        float fy0 = upperCenter - m_leftBuffer[i] * h * 0.3f;
        float fy1 = upperCenter - m_leftBuffer[i + 1] * h * 0.3f;
        v[idx++].set(fx0, fy0);
        v[idx++].set(fx1, fy1);
    }

    // 右声道：下方区域，从中线向下波动
    for (int i = 0; i < rightCount - 1; ++i) {
        float fx0 = x + (float(i) / (rightCount - 1)) * w;
        float fx1 = x + (float(i + 1) / (rightCount - 1)) * w;
        float fy0 = lowerCenter + m_rightBuffer[i] * h * 0.3f;
        float fy1 = lowerCenter + m_rightBuffer[i + 1] * h * 0.3f;
        v[idx++].set(fx0, fy0);
        v[idx++].set(fx1, fy1);
    }

    markDirty(DirtyGeometry);
    m_spectrumDirty = false;
    m_rectDirty = false;
}

QT_END_NAMESPACE
