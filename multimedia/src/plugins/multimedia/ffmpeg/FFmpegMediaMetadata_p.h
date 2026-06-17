#ifndef FFMPEGMEDIAMETADATA_P_H
#define FFMPEGMEDIAMETADATA_P_H

#include <MediaMetadata.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
// 媒体元数据处理，提供 AVDictionary 与 MediaMetaData 的双向转换
class MetaData : public ::MediaMetaData
{
public:
    // 添加元数据条目
    static void addEntry(::MediaMetaData &metaData, AVDictionaryEntry *entry);
    // 从 AVDictionary 创建元数据
    static ::MediaMetaData fromAVMetaData(const AVDictionary *tags);

    // 获取元数据值
    static QByteArray value(const ::MediaMetaData &metaData, ::MediaMetaData::Key key);
    // 转换为 AVDictionary
    static AVDictionary *toAVMetaData(const ::MediaMetaData &metaData);
};
}
QT_END_NAMESPACE

#endif
