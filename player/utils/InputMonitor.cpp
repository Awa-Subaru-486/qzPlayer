#include "InputMonitor.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QTouchEvent>
#include <QMouseEvent>

namespace qz {

InputMonitor::InputMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &InputMonitor::onTimerTimeout);
    m_timer->start(100);

    m_singleTapTimer = new QTimer(this);
    m_singleTapTimer->setSingleShot(true);
    m_singleTapTimer->setInterval(kDoubleTapIntervalMs);
    connect(m_singleTapTimer, &QTimer::timeout, this, &InputMonitor::onSingleTapTimeout);

    QCoreApplication::instance()->installEventFilter(this);
}

InputMonitor::~InputMonitor()
{
    if (m_timer) {
        m_timer->stop();
    }

    auto app = QCoreApplication::instance();
    if (app) {
        app->removeEventFilter(this);
    }
}

void InputMonitor::setRegion(int x, int y, int width, int height)
{
    m_region = QRect(x, y, width, height);
    m_isInRegion = false;
    m_lastPos = QPoint();
}

void InputMonitor::clearRegion()
{
    m_region = QRect();
    m_isInRegion = false;
    m_lastPos = QPoint();
}

bool InputMonitor::eventFilter(QObject* watched, QEvent* event)
{
    if (m_region.isNull()) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::KeyPress:
    case QEvent::Shortcut:
    case QEvent::ShortcutOverride:
        notifyActivity();
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
        handleMouseEvent(watched, event);
        break;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        handleTouchEvent(watched, event);
        break;
    default:
        break;
    }

    // Always pass events through - never consume them
    return QObject::eventFilter(watched, event);
}

void InputMonitor::notifyActivity()
{
    if (m_region.isNull()) {
        return;
    }

    const QPoint currentPos = QCursor::pos();
    QPoint checkPos = currentPos.isNull() ? m_lastPos : currentPos;

    if (m_region.contains(checkPos)) {
        m_lastPos = checkPos;
        m_isInRegion = true;
        emit activityInRegion();
        emit inRegionChanged(true);
    }
}

bool InputMonitor::isInTouchRegion(const QPointF& globalPos) const
{
    return m_region.contains(globalPos.toPoint());
}

void InputMonitor::handleTouchBegin(const QPointF& globalPos)
{
    m_touchActive = true;
    m_isLongPress = false;
    m_isHorizontalSwipe = false;
    m_touchStartPos = globalPos;
    m_touchLastPos = globalPos;
    m_accumulatedDx = 0.0;
    m_touchTimer.start();
}

void InputMonitor::handleTouchUpdate(const QPointF& globalPos)
{
    if (!m_touchActive) return;

    qreal dx = globalPos.x() - m_touchLastPos.x();
    m_accumulatedDx = globalPos.x() - m_touchStartPos.x();

    // Check for horizontal swipe
    if (!m_isHorizontalSwipe && !m_isLongPress) {
        qreal totalAbsDx = qAbs(m_accumulatedDx);
        qreal totalAbsDy = qAbs(globalPos.y() - m_touchStartPos.y());
        if (totalAbsDx > kHorizontalSwipeThreshold && totalAbsDx > totalAbsDy) {
            m_isHorizontalSwipe = true;
        }
    }

    // Emit horizontal swipe displacement
    if (m_isHorizontalSwipe && !qFuzzyIsNull(dx)) {
        emit horizontalSwipe(dx);
    }

    m_touchLastPos = globalPos;
}

void InputMonitor::handleTouchEnd(const QPointF& globalPos)
{
    if (!m_touchActive) return;

    m_touchActive = false;

    if (m_isHorizontalSwipe) {
        emit horizontalSwipeReleased(m_accumulatedDx);
    } else if (m_isLongPress) {
        emit longPressReleased();
    } else {
        // It's a tap - determine single or double
        m_tapCount++;
        m_tapPos = globalPos;

        if (m_tapCount == 1) {
            m_singleTapTimer->start();
        } else if (m_tapCount >= 2) {
            m_singleTapTimer->stop();
            m_tapCount = 0;
            emit doubleTapped(globalPos.x(), globalPos.y());
        }
    }

    m_isLongPress = false;
    m_isHorizontalSwipe = false;
}

void InputMonitor::handleTouchEvent(QObject* watched, QEvent* event)
{
    auto* touchEvent = static_cast<QTouchEvent*>(event);
    const auto& touchPoints = touchEvent->points();

    if (touchPoints.isEmpty()) return;

    const QPointF globalPos = touchPoints.first().globalPosition();

    // Original activity tracking
    m_lastPos = globalPos.toPoint();
    if (m_region.contains(m_lastPos)) {
        m_isInRegion = true;
        emit activityInRegion();
        emit inRegionChanged(true);
    } else {
        if (m_isInRegion) {
            m_isInRegion = false;
            emit inRegionChanged(false);
        }
    }

    // Only process gesture events if touch is in region
    if (!isInTouchRegion(globalPos)) return;

    switch (event->type()) {
    case QEvent::TouchBegin:
        handleTouchBegin(globalPos);
        break;
    case QEvent::TouchUpdate:
        // Check for long press timeout
        if (m_touchActive && !m_isLongPress && !m_isHorizontalSwipe) {
            if (m_touchTimer.elapsed() >= kLongPressThresholdMs) {
                qreal totalAbsDx = qAbs(globalPos.x() - m_touchStartPos.x());
                qreal totalAbsDy = qAbs(globalPos.y() - m_touchStartPos.y());
                if (totalAbsDx < kHorizontalSwipeThreshold && totalAbsDy < kHorizontalSwipeThreshold) {
                    m_isLongPress = true;
                    emit longPressed(globalPos.x(), globalPos.y());
                }
            }
        }
        handleTouchUpdate(globalPos);
        break;
    case QEvent::TouchEnd:
        handleTouchEnd(globalPos);
        break;
    default:
        break;
    }
}

void InputMonitor::onSingleTapTimeout()
{
    if (m_tapCount == 1) {
        emit singleTapped(m_tapPos.x(), m_tapPos.y());
    }
    m_tapCount = 0;
}

void InputMonitor::onTimerTimeout()
{
    if (m_region.isNull()) {
        return;
    }

    const QPoint currentPos = QCursor::pos();

    if (currentPos.isNull()) {
        return;
    }

    if (m_region.contains(currentPos)) {
        if (currentPos != m_lastPos) {
            m_lastPos = currentPos;
            m_isInRegion = true;
            emit activityInRegion();
            emit inRegionChanged(true);
        }
    } else {
        if (m_isInRegion) {
            m_isInRegion = false;
            emit inRegionChanged(false);
        }
        m_lastPos = QPoint();
    }
}

void InputMonitor::handleMouseEvent(QObject* watched, QEvent* event)
{
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    const QPointF globalPos = mouseEvent->globalPosition();

    // Update activity tracking
    m_lastPos = globalPos.toPoint();
    if (m_region.contains(m_lastPos)) {
        m_isInRegion = true;
        emit activityInRegion();
        emit inRegionChanged(true);
    } else {
        if (m_isInRegion) {
            m_isInRegion = false;
            emit inRegionChanged(false);
        }
    }

    // Only process double click events if mouse is in region
    if (!m_region.contains(globalPos.toPoint())) return;

    // Double click event from system - emit directly
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (mouseEvent->button() == Qt::LeftButton) {
            emit doubleTapped(globalPos.x(), globalPos.y());
        }
    }
}

}
