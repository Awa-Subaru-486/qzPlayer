// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MULTIMEDIAENUMTOSTRINGCONVERTER_P_H
#define QT_MULTIMEDIAENUMTOSTRINGCONVERTER_P_H

#include <QtCore/private/qglobal_p.h>
#include <QtCore/qstring.h>

#include <optional>

#define QT_MM_CAT(x, y) QT_MM_IMPL_CAT(x, y)
#define QT_MM_IMPL_CAT(x, y) x##y

#define QT_MM_IMPL_GEN_CASE_MAP_ENUM_TO_STRING(SYMBOL, STRING) \
    case SYMBOL:                                               \
        return QStringLiteral(STRING);

#define QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING(seq)               \
    QT_MM_CAT(QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING_1 seq, _END) \
    static_assert(true, "force semicolon")
#define QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING_1(x, y) \
    QT_MM_IMPL_GEN_CASE_MAP_ENUM_TO_STRING(x, y) QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING_2
#define QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING_2(x, y) \
    QT_MM_IMPL_GEN_CASE_MAP_ENUM_TO_STRING(x, y) QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING_1
#define QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING_1_END
#define QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING_2_END

namespace QtMultimediaPrivate {

struct EnumName
{
};

template <typename Enum, typename Role = EnumName>
struct StringResolver
{
    static std::optional<QString> toQString(Enum);
};

}

#define QT_MM_MAKE_STRING_RESOLVER(Enum, EnumName, ...)        \
    template <>                                                \
    struct QtMultimediaPrivate::StringResolver<Enum, EnumName> \
    {                                                          \
        static std::optional<QString> toQString(Enum arg)      \
        {                                                      \
            switch (arg) {                                     \
            QT_MM_IMPL_GEN_CASES_ENUM_TO_STRING(__VA_ARGS__);  \
            default:                                           \
                return std::nullopt;                           \
            }                                                  \
        }                                                      \
    };                                                         \
    static_assert(true, "force semicolon")

#define QT_MM_DEFINE_QDEBUG_ENUM(EnumType)                                     \
    QDebug operator<<(QDebug dbg, EnumType arg)                                \
    {                                                                          \
        QDebugStateSaver saver(dbg);                                           \
        dbg.noquote();                                                         \
        std::optional<QString> resolved =                                      \
                QtMultimediaPrivate::StringResolver<EnumType>::toQString(arg); \
        if (resolved)                                                          \
            dbg << *resolved;                                                  \
        else                                                                   \
            dbg << "Unknown Enum value";                                       \
        return dbg;                                                            \
    }                                                                          \
    static_assert(true, "force semicolon")

#endif
