#include <QtCore/QtCore>
#include <qzMultimedia/QtMultimedia>

#ifdef Q_OS_WINDOWS
#  include <qt_windows.h>
#  include <D3d11.h>
#  include <dxgi1_2.h>
#  include <mfapi.h>
#  include <mfidl.h>
#  include <mferror.h>
#  include <mfreadwrite.h>
#endif

extern "C" {
#  include <libavformat/avformat.h>
#  include <libavcodec/avcodec.h>
#  include <libswresample/swresample.h>
#  include <libswscale/swscale.h>
#  include <libavutil/avutil.h>
}
