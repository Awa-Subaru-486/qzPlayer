// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MEDIATIMERANGE_H
#define QT_MEDIATIMERANGE_H
#include <qzMultimedia/MultimediaGlobal.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>

class MediaTimeRangePrivate;

QT_DECLARE_QESDP_SPECIALIZATION_DTOR_WITH_EXPORT(MediaTimeRangePrivate, QZ_MULTIMEDIA_EXPORT)

// 媒体时间范围：表示一组时间间隔的集合
class QZ_MULTIMEDIA_EXPORT MediaTimeRange
{
public:
    // 时间间隔结构
    struct Interval
    {
        constexpr Interval() noexcept = default;
        explicit constexpr Interval(qint64 start, qint64 end) noexcept
        : s(start), e(end)
        {}

        constexpr qint64 start() const noexcept { return s; }
        constexpr qint64 end() const noexcept { return e; }

        // 检查是否包含指定时间
        constexpr bool contains(qint64 time) const noexcept
        {
            return isNormal() ? (s <= time && time <= e)
                : (e <= time && time <= s);
        }

        constexpr bool isNormal() const noexcept { return s <= e; }
        // 规范化间隔
        constexpr Interval normalized() const
        {
            return s > e ? Interval(e, s) : *this;
        }
        // 平移间隔
        constexpr Interval translated(qint64 offset) const
        {
            return Interval(s + offset, e + offset);
        }

        friend constexpr bool operator==(Interval lhs, Interval rhs) noexcept
        {
            return lhs.start() == rhs.start() && lhs.end() == rhs.end();
        }
        friend constexpr bool operator!=(Interval lhs, Interval rhs) noexcept
        {
            return lhs.start() != rhs.start() || lhs.end() != rhs.end();
        }

    private:
        friend class MediaTimeRangePrivate;
        qint64 s = 0;
        qint64 e = 0;
    };

    MediaTimeRange();
    explicit MediaTimeRange(qint64 start, qint64 end);
    MediaTimeRange(const Interval&);
    MediaTimeRange(const MediaTimeRange &range) noexcept;
    ~MediaTimeRange();

    MediaTimeRange &operator=(const MediaTimeRange&) noexcept;

    MediaTimeRange(MediaTimeRange &&other) noexcept = default;
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(MediaTimeRange)
    void swap(MediaTimeRange &other) noexcept
    { d.swap(other.d); }
    void detach();

    MediaTimeRange &operator=(const Interval&);

    // 最早和最晚时间
    qint64 earliestTime() const;
    qint64 latestTime() const;

    // 获取所有间隔
    QList<MediaTimeRange::Interval> intervals() const;
    bool isEmpty() const;
    bool isContinuous() const;

    // 检查是否包含指定时间
    bool contains(qint64 time) const;

    // 添加间隔
    void addInterval(qint64 start, qint64 end);
    void addInterval(const Interval &interval);
    void addTimeRange(const MediaTimeRange&);

    // 移除间隔
    void removeInterval(qint64 start, qint64 end);
    void removeInterval(const Interval &interval);
    void removeTimeRange(const MediaTimeRange&);

    MediaTimeRange& operator+=(const MediaTimeRange&);
    MediaTimeRange& operator+=(const Interval&);
    MediaTimeRange& operator-=(const MediaTimeRange&);
    MediaTimeRange& operator-=(const Interval&);

    void clear();

    friend inline bool operator==(const MediaTimeRange &lhs, const MediaTimeRange &rhs)
    { return lhs.intervals() == rhs.intervals(); }
    friend inline bool operator!=(const MediaTimeRange &lhs, const MediaTimeRange &rhs)
    { return lhs.intervals() != rhs.intervals(); }

private:
    QExplicitlySharedDataPointer<MediaTimeRangePrivate> d;
};

#ifndef QT_NO_DEBUG_STREAM
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, const MediaTimeRange::Interval &);
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, const MediaTimeRange &);
#endif

inline MediaTimeRange operator+(const MediaTimeRange &r1, const MediaTimeRange &r2)
{ return (MediaTimeRange(r1) += r2); }
inline MediaTimeRange operator-(const MediaTimeRange &r1, const MediaTimeRange &r2)
{ return (MediaTimeRange(r1) -= r2); }

Q_DECLARE_SHARED(MediaTimeRange)

Q_DECLARE_METATYPE(MediaTimeRange)
Q_DECLARE_METATYPE(MediaTimeRange::Interval)

#endif
