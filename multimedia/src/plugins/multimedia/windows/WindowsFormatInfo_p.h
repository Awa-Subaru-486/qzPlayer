#ifndef WINDOWSFORMATINFO_P_H
#define WINDOWSFORMATINFO_P_H

#include <private/PlatformMediaFormatInfo_p.h>

namespace windows {

// Windows 平台格式信息，查询 Media Foundation 支持的格式
class FormatInfo : public PlatformMediaFormatInfo
{
public:
    FormatInfo();
    ~FormatInfo();
};

}

#endif
