// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#include <QtCore/qsystemdetection.h>

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtConcurrent/QtConcurrent>

#ifdef Q_OS_WINDOWS
#  include <QtCore/qt_windows.h>

#  ifndef Q_CC_MINGW
#    include <audioclient.h>
#    include <ks.h>
#    include <ksmedia.h>
#    include <mfapi.h>
#    include <mferror.h>
#    include <mfidl.h>
#    include <mfobjects.h>
#    include <mfreadwrite.h>
#    include <mftransform.h>
#    include <mmdeviceapi.h>
#    include <mmreg.h>
#    include <propsys.h>
#    include <propvarutil.h>
#    include <wmcodecdsp.h>
#  endif
