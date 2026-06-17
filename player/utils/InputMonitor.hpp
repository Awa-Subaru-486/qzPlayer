#pragma once

#include <QObject>
#include <QTimer>
#include <QCursor>
#include <QElapsedTimer>
#include <QtQml/qqml.h>

#include "qzPlayer_export.hpp"

namespace qz {
    class QZ_PLAYER_EXPORT InputMonitor : public QObject {
        Q_OBJECT

    public:
        explicit InputMonitor(QObject* parent = nullptr);
        ~InputMonitor() override;

        Q_INVOKABLE void setRegion(int x, int y, int width, int height);
        Q_INVOKABLE void clearRegion();

    signals:
        void inRegionChanged(bool isIn);
        void activityInRegion();
        void horizontalSwipe(qreal dx);
        void horizontalSwipeReleased(qreal totalDx);
        void singleTapped(qreal x, qreal y);
        void doubleTapped(qreal x, qreal y);
        void longPressed(qreal x, qreal y);
        void longPressReleased();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private slots:
        void onTimerTimeout();
        void onSingleTapTimeout();

    private:
        void notifyActivity();
        void handleTouchEvent(QObject* watched, QEvent* event);
        void handleMouseEvent(QObject* watched, QEvent* event);

        bool isInTouchRegion(const QPointF& globalPos) const;
        void handleTouchBegin(const QPointF& globalPos);
        void handleTouchUpdate(const QPointF& globalPos);
        void handleTouchEnd(const QPointF& globalPos);

    private:
        QRect m_region;
        bool m_isInRegion{false};
        QTimer* m_timer{nullptr};
        QPoint m_lastPos;

        // Touch gesture state
        bool m_touchActive{false};
        bool m_isLongPress{false};
        bool m_isHorizontalSwipe{false};
        QPointF m_touchStartPos;
        QPointF m_touchLastPos;
        qreal m_accumulatedDx{0.0};
        QElapsedTimer m_touchTimer;
        QTimer* m_singleTapTimer{nullptr};
        int m_tapCount{0};
        QPointF m_tapPos;

        static constexpr int kLongPressThresholdMs = 500;
        static constexpr int kDoubleTapIntervalMs = 300;
        static constexpr int kHorizontalSwipeThreshold = 30;
    };
}
