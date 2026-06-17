// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include <QtAudio.h>
#include <qmath.h>
#include <QDebug>

#define LOG100 4.60517018599

namespace QtAudio
{

float convertVolume(float volume, VolumeScale from, VolumeScale to)
{
    switch (from) {
    case LinearVolumeScale:
        volume = qMax(float(0), volume);
        switch (to) {
        case LinearVolumeScale:
            return volume;
        case CubicVolumeScale:
            return qPow(volume, float(1 / 3.0));
        case LogarithmicVolumeScale:
            return 1 - std::exp(-volume * LOG100);
        case DecibelVolumeScale:
            if (volume < 0.001)
                return float(-200);
            else
                return float(20.0) * std::log10(volume);
        }
        break;
    case CubicVolumeScale:
        volume = qMax(float(0), volume);
        switch (to) {
        case LinearVolumeScale:
            return volume * volume * volume;
        case CubicVolumeScale:
            return volume;
        case LogarithmicVolumeScale:
            return 1 - std::exp(-volume * volume * volume * LOG100);
        case DecibelVolumeScale:
            if (volume < 0.001)
                return float(-200);
            else
                return float(3.0 * 20.0) * std::log10(volume);
        }
        break;
    case LogarithmicVolumeScale:
        volume = qMax(float(0), volume);
        switch (to) {
        case LinearVolumeScale:
            if (volume > 0.99)
                return 1;
            else
                return -std::log(1 - volume) / LOG100;
        case CubicVolumeScale:
            if (volume > 0.99)
                return 1;
            else
                return qPow(-std::log(1 - volume) / LOG100, float(1 / 3.0));
        case LogarithmicVolumeScale:
            return volume;
        case DecibelVolumeScale:
            if (volume < 0.001)
                return float(-200);
            else if (volume > 0.99)
                return 0;
            else
                return float(20.0) * std::log10(-std::log(1 - volume) / LOG100);
        }
        break;
    case DecibelVolumeScale:
        switch (to) {
        case LinearVolumeScale:
            return qPow(float(10.0), volume / float(20.0));
        case CubicVolumeScale:
            return qPow(float(10.0), volume / float(3.0 * 20.0));
        case LogarithmicVolumeScale:
            if (qFuzzyIsNull(volume))
                return 1;
            else
                return 1 - std::exp(-qPow(float(10.0), volume / float(20.0)) * LOG100);
        case DecibelVolumeScale:
            return volume;
        }
        break;
    }

    return volume;
}

}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, Audio::Error error)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (error) {
        case Audio::NoError:
            dbg << "NoError";
            break;
        case Audio::OpenError:
            dbg << "OpenError";
            break;
        case Audio::IOError:
            dbg << "IOError";
            break;
        case Audio::UnderrunError:
            dbg << "UnderrunError";
            break;
        case Audio::FatalError:
            dbg << "FatalError";
            break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, Audio::State state)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (state) {
        case Audio::ActiveState:
            dbg << "ActiveState";
            break;
        case Audio::SuspendedState:
            dbg << "SuspendedState";
            break;
        case Audio::StoppedState:
            dbg << "StoppedState";
            break;
        case Audio::IdleState:
            dbg << "IdleState";
            break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, Audio::VolumeScale scale)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (scale) {
    case Audio::LinearVolumeScale:
        dbg << "LinearVolumeScale";
        break;
    case Audio::CubicVolumeScale:
        dbg << "CubicVolumeScale";
        break;
    case Audio::LogarithmicVolumeScale:
        dbg << "LogarithmicVolumeScale";
        break;
    case Audio::DecibelVolumeScale:
        dbg << "DecibelVolumeScale";
        break;
    }
    return dbg;
}

#endif

