#ifndef QZTHEME_EXPORT_HPP
#define QZTHEME_EXPORT_HPP

#include <QtCore/qtconfigmacros.h>

#if defined(QT_SHARED) || !defined(QT_STATIC)
#  if defined(QZTHEME_BUILD_LIB)
#    define QZTHEME_EXPORT Q_DECL_EXPORT
#  else
#    define QZTHEME_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define QZTHEME_EXPORT
#endif

#endif
