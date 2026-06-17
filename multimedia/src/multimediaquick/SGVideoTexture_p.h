// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef SGVIDEOTEXTURE_P_H
#define SGVIDEOTEXTURE_P_H

#include <QtQuick/qsgtexture.h>
#include <QtGui/qimage.h>
#include <QtGui/rhi/qrhi.h>
#include "qzmultimediaquickexports.h"

#include <memory>

QT_BEGIN_NAMESPACE

class QSGVideoTexturePrivate;
// 场景图视频纹理：封装 QRhiTexture 用于 QSG 渲染
class QZ_MULTIMEDIAQUICK_EXPORT QSGVideoTexture : public QSGTexture
{
    Q_DECLARE_PRIVATE(QSGVideoTexture)
public:
    QSGVideoTexture();
    ~QSGVideoTexture() override;

    qint64 comparisonKey() const override;
    QRhiTexture *rhiTexture() const override;
    QSize textureSize() const override;
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;
    // 设置 RHI 纹理
    void setRhiTexture(QRhiTexture *texture);
    // 设置纹理数据
    void setData(QRhiTexture::Format f, const QSize &s, const uchar *data, int bytes);

protected:
    std::unique_ptr<QSGVideoTexturePrivate> d_ptr;
};

QT_END_NAMESPACE

#endif
