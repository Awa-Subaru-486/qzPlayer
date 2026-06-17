#ifndef MFMETADATACONTROL_H
#define MFMETADATACONTROL_H

#include <MediaMetadata.h>
#include "mfidl.h"

namespace windows {

// MF 元数据工具类，提供 IMFMediaSource 与 MediaMetaData 的双向转换
class MFMetaData
{
public:
    // 从原生媒体源创建元数据
    static MediaMetaData fromNative(IMFMediaSource* mediaSource);
    // 将元数据写入原生属性存储
    static void toNative(const MediaMetaData &metaData, IPropertyStore *content);
};

}

#endif
