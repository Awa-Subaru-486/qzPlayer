// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_ERRORINFO_P_H
#define QT_ERRORINFO_P_H

#include <qzMultimedia/MultimediaGlobal.h>
#include <QString>

// 错误信息：封装错误码和描述
template <typename ErrorCode, ErrorCode NoError = ErrorCode::NoError>
class ErrorInfo
{
public:
    ErrorInfo(ErrorCode error = NoError, QString description = {})
        : m_code(error), m_description(std::move(description))
    {
    }

    // 设置错误并通知
    template <typename Notifier>
    void setAndNotify(ErrorCode code, QString description, Notifier &notifier)
    {
        const bool changed = code != m_code || description != m_description;

        m_code = code;
        m_description = std::move(description);

        if (code != NoError)
            emit notifier.errorOccurred(m_code, m_description);

        if (changed)
            emit notifier.errorChanged();
    }

    ErrorCode code() const { return m_code; }
    QString description() const { return m_description; };

private:
    ErrorCode m_code;
    QString m_description;
};

#endif
