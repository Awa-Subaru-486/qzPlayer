#ifndef QZPLAYER_EXPORT_HPP
#define QZPLAYER_EXPORT_HPP

#include <QtCore/qtconfigmacros.h>

#if defined(QT_SHARED) || !defined(QT_STATIC)
#  if defined(QZ_PLAYER_BUILD_LIB)
#    define QZ_PLAYER_EXPORT Q_DECL_EXPORT
#  else
#    define QZ_PLAYER_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define QZ_PLAYER_EXPORT
#endif

#endif
