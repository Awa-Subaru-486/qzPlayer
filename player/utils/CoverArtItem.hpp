/**
 * @file CoverArtItem.hpp
 * @brief 封面图像显示组件，异步获取 MediaPlayer 封面并提取强调色
 * @details 使用自定义 QSGMaterial + 圆角着色器，零拷贝纹理上传
 */

#pragma once

#include <QQuickItem>
#include <QImage>
#include <QFutureWatcher>
#include <QColor>
#include <atomic>
#include <optional>

#include "qzPlayer_export.hpp"

namespace qz {

    class QZ_PLAYER_EXPORT CoverArtItem : public QQuickItem
    {
        Q_OBJECT

        Q_PROPERTY(QObject* mediaPlayer READ mediaPlayer WRITE set_mediaPlayer NOTIFY mediaPlayerChanged)
        Q_PROPERTY(qreal radius READ radius WRITE set_radius NOTIFY radiusChanged)
        Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY isEmptyChanged)
        Q_PROPERTY(qreal coverAspectRatio READ coverAspectRatio NOTIFY coverAspectRatioChanged)
        Q_PROPERTY(qreal maxDim READ maxDim WRITE set_maxDim NOTIFY maxDimChanged)

    public:
        explicit CoverArtItem(QQuickItem *parent = nullptr);
        ~CoverArtItem() override;

        QObject* mediaPlayer() const;
        void set_mediaPlayer(QObject *player);

        qreal radius() const;
        void set_radius(qreal r);

        bool isEmpty() const;
        qreal coverAspectRatio() const;

        qreal maxDim() const;
        void set_maxDim(qreal dim);

        const QImage &coverImage() const;
        int generation() const;

    Q_SIGNALS:
        void mediaPlayerChanged();
        void radiusChanged();
        void isEmptyChanged();
        void coverAspectRatioChanged();
        void maxDimChanged();

    protected:
        QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

    private Q_SLOTS:
        void onCoverFinished();
        void onColorExtracted();
        void requestCover();

    private:
        void extractColor(const QImage &image);
        void updateImplicitSize();

        QObject *m_mediaPlayer = nullptr;
        QImage m_coverImage;
        qreal m_radius = 12.0;
        qreal m_coverAspectRatio = 0.0;
        qreal m_maxDim = 120.0;
        bool m_coverNeeded = false;
        std::atomic<int> m_generation{0};

        QFutureWatcher<QImage> m_coverWatcher;
        QFutureWatcher<std::optional<QColor>> m_colorWatcher;
    };

} // namespace qz
