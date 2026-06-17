/**
 * @file SvgRenderer.hpp
 * @brief SVG 渲染器组件，用于在 QML 中渲染 SVG 图像
 * @author AwaUtils
 * @date 2026-02-13
 */

#pragma once

#include <QQuickItem>
#include <QSGSimpleTextureNode>
#include <QImage>
#include <QFutureWatcher>
#include <atomic>
#include <QColor>
#include <QtQml/qqml.h>

#include "qzPlayer_export.hpp"

namespace qz {

    struct SvgAsyncResult {
        QImage masterCache;
        QImage renderCache;
        double docWidth = 0;
        double docHeight = 0;
        int generation = 0;
        bool success = false;
        bool loadFailed = false;
        bool renderFailed = false;
    };

    class QZ_PLAYER_EXPORT SvgRenderer : public QQuickItem
    {
        Q_OBJECT

        Q_PROPERTY(QString source READ source WRITE set_source NOTIFY sourceChanged)
        Q_PROPERTY(QColor color READ color WRITE set_color NOTIFY colorChanged)
        Q_PROPERTY(int paintedSize READ paintedSize WRITE set_paintedSize NOTIFY paintedSizeChanged)
        Q_PROPERTY(bool useOriginalColor READ useOriginalColor WRITE set_useOriginalColor NOTIFY useOriginalColorChanged)

    public:
        QString source() const;
        void set_source(const QString& in_source);

        QColor color() const;
        void set_color(const QColor& in_color);

        auto paintedSize() const -> int;
        auto set_paintedSize(int in_paintedSize) -> void;

        auto useOriginalColor() const -> bool;
        auto set_useOriginalColor(bool in_useOriginalColor) -> void;

    Q_SIGNALS:
        void sourceChanged();
        void colorChanged();
        void paintedSizeChanged();
        void useOriginalColorChanged();

    public:
        /**
         * @brief 构造函数
         * @param parent 父级 QQuickItem 对象，默认为 nullptr
         */
        explicit SvgRenderer(QQuickItem *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~SvgRenderer() override;

    protected:
        /**
         * @brief 监听几何尺寸变化
         * @param newGeometry 新的几何尺寸
         * @param oldGeometry 旧的几何尺寸
         */
        void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

        /**
         * @brief 更新绘制节点
         * @param oldNode 旧的场景图节点
         * @param updatePaintNodeData 更新绘制节点数据
         * @return 更新后的 QSGNode 指针
         */
        QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;

    private Q_SLOTS:
        void onAsyncRenderFinished();

    private:
        void updateRenderCache();

        /**
         * @brief 像素级染色（蒙版模式）
         * @param image 待染色的图像
         * @param color 染色颜色
         */
        static void applyColorToImage(QImage &image, const QColor &color);

        /**
         * @brief 更新 Item 的 implicit 尺寸
         */
        void updateImplicitSizeFromDocument();

    private:
        QString m_source{};             ///< SVG 文件源路径
        QColor m_color = Qt::black;     ///< 染色颜色，默认黑色

        QImage m_masterCache{};         ///< 存储"原始"光栅化结果（未染色）
        QImage m_renderCache{};         ///< 存储"染色后"的结果（用于显示）

        double m_docWidth = 0;          ///< SVG 文档原始宽度
        double m_docHeight = 0;         ///< SVG 文档原始高度
        std::atomic<int> m_generation{0}; ///< 异步渲染代数，用于丢弃过期结果
        QFutureWatcher<SvgAsyncResult> m_watcher; ///< 异步渲染结果监视器

        int m_paintedSize{};            ///< 绘制尺寸
        bool m_useOriginalColor{};      ///< 是否使用原始颜色
    };

} // namespace qz
