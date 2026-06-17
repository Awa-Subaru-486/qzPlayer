// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include <QtCore/qdebug.h>

#include "MediaTimeRange.h"

class MediaTimeRangePrivate : public QSharedData
{
public:
    MediaTimeRangePrivate() = default;
    MediaTimeRangePrivate(const MediaTimeRange::Interval &interval);

    QList<MediaTimeRange::Interval> intervals;

    void addInterval(const MediaTimeRange::Interval &interval);
    void removeInterval(const MediaTimeRange::Interval &interval);
};

QT_DEFINE_QESDP_SPECIALIZATION_DTOR(MediaTimeRangePrivate);

MediaTimeRangePrivate::MediaTimeRangePrivate(const MediaTimeRange::Interval &interval)
{
    if (interval.isNormal())
        intervals << interval;
}

void MediaTimeRangePrivate::addInterval(const MediaTimeRange::Interval &interval)
{

    if (!interval.isNormal())
        return;

    int i;
    for (i = 0; i < intervals.size(); i++) {

        if(interval.s < intervals[i].s) {
            intervals.insert(i, interval);
            break;
        }
    }

    if (i == intervals.size())
        intervals.append(interval);

    if (i > 0 && intervals[i - 1].e >= interval.s - 1)
        i--;

    while (i < intervals.size() - 1
          && intervals[i].e >= intervals[i + 1].s - 1) {
        intervals[i].e = qMax(intervals[i].e, intervals[i + 1].e);
        intervals.removeAt(i + 1);
    }
}

void MediaTimeRangePrivate::removeInterval(const MediaTimeRange::Interval &interval)
{

    if (!interval.isNormal())
        return;

    for (int i = 0; i < intervals.size(); i++) {
        const MediaTimeRange::Interval r = intervals.at(i);

        if (r.e < interval.s) {

            continue;
        } else if (interval.e < r.s) {

            break;
        } else if (r.s < interval.s && interval.e < r.e) {

            intervals[i].e = interval.s -1;
            addInterval(MediaTimeRange::Interval(interval.e + 1, r.e));
            break;
        } else if (r.s < interval.s) {

            intervals[i].e = interval.s - 1;
        } else if (interval.e < r.e) {

            intervals[i].s = interval.e + 1;
            break;
        } else {

            intervals.removeAt(i);
            --i;
        }
    }
}

MediaTimeRange::MediaTimeRange()
    : d(new MediaTimeRangePrivate)
{

}

MediaTimeRange::MediaTimeRange(qint64 start, qint64 end)
    : MediaTimeRange(Interval(start, end))
{
}

MediaTimeRange::MediaTimeRange(const MediaTimeRange::Interval &interval)
    : d(new MediaTimeRangePrivate(interval))
{
}

MediaTimeRange::MediaTimeRange(const MediaTimeRange &range) noexcept = default;

MediaTimeRange::~MediaTimeRange() = default;

MediaTimeRange &MediaTimeRange::operator=(const MediaTimeRange &other) noexcept = default;

MediaTimeRange &MediaTimeRange::operator=(const MediaTimeRange::Interval &interval)
{
    d = new MediaTimeRangePrivate(interval);
    return *this;
}

qint64 MediaTimeRange::earliestTime() const
{
    if (!d->intervals.empty())
        return d->intervals[0].start();

    return 0;
}

qint64 MediaTimeRange::latestTime() const
{
    if (!d->intervals.empty())
        return d->intervals[d->intervals.size() - 1].end();

    return 0;
}

void MediaTimeRange::addInterval(qint64 start, qint64 end)
{
    detach();
    d->addInterval(Interval(start, end));
}

void MediaTimeRange::addInterval(const MediaTimeRange::Interval &interval)
{
    detach();
    d->addInterval(interval);
}

void MediaTimeRange::addTimeRange(const MediaTimeRange &range)
{
    detach();
    const auto intervals = range.intervals();
    for (const Interval &i : intervals) {
        d->addInterval(i);
    }
}

void MediaTimeRange::removeInterval(qint64 start, qint64 end)
{
    detach();
    d->removeInterval(Interval(start, end));
}

void MediaTimeRange::removeInterval(const MediaTimeRange::Interval &interval)
{
    detach();
    d->removeInterval(interval);
}

void MediaTimeRange::removeTimeRange(const MediaTimeRange &range)
{
    detach();
    const auto intervals = range.intervals();
    for (const Interval &i : intervals) {
        d->removeInterval(i);
    }
}

MediaTimeRange& MediaTimeRange::operator+=(const MediaTimeRange &other)
{
    addTimeRange(other);
    return *this;
}

MediaTimeRange& MediaTimeRange::operator+=(const MediaTimeRange::Interval &interval)
{
    addInterval(interval);
    return *this;
}

MediaTimeRange& MediaTimeRange::operator-=(const MediaTimeRange &other)
{
    removeTimeRange(other);
    return *this;
}

MediaTimeRange& MediaTimeRange::operator-=(const MediaTimeRange::Interval &interval)
{
    removeInterval(interval);
    return *this;
}

void MediaTimeRange::clear()
{
    detach();
    d->intervals.clear();
}

void MediaTimeRange::detach()
{
    d.detach();
}

QList<MediaTimeRange::Interval> MediaTimeRange::intervals() const
{
    return d->intervals;
}

bool MediaTimeRange::isEmpty() const
{
    return d->intervals.empty();
}

bool MediaTimeRange::isContinuous() const
{
    return (d->intervals.size() <= 1);
}

bool MediaTimeRange::contains(qint64 time) const
{
    for (int i = 0; i < d->intervals.size(); i++) {
        if (d->intervals[i].contains(time))
            return true;

        if (time < d->intervals[i].start())
            break;
    }

    return false;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const MediaTimeRange::Interval &interval)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "MediaTimeRange::Interval( " << interval.start() << ", " << interval.end() << " )";
    return dbg;
}

QDebug operator<<(QDebug dbg, const MediaTimeRange &range)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "MediaTimeRange( ";
    const auto intervals = range.intervals();
    for (const auto &interval : intervals)
        dbg << '(' <<  interval.start() << ", " << interval.end() << ") ";
    dbg.space();
    dbg << ')';
    return dbg;
}
#endif

