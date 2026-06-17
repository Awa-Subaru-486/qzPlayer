#ifndef PLAYBACKENGINE_FFMPEGSUBTITLERENDERER_P_H
#define PLAYBACKENGINE_FFMPEGSUBTITLERENDERER_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegRenderer_p.h>

#include <QtCore/qpointer.h>
#include <SubtitleStyle.h>

QT_BEGIN_NAMESPACE

class VideoSink;

namespace ffmpeg {

// 字幕渲染器，将字幕文本/图片输出到视频接收器
class SubtitleRenderer : public Renderer
{
    Q_OBJECT
public:
    SubtitleRenderer(const PlaybackEngineObjectID &id, const TimeController &tc, ::VideoSink *sink);

    ~SubtitleRenderer() override;

    void setOutput(::VideoSink *sink, bool cleanPrevSink = false);

    void setSubtitleStyle(const SubtitleStyle &style);

protected:
    RenderingResult renderInternal(Frame frame) override;

private:
    QPointer<::VideoSink> m_sink;
    SubtitleStyle m_subtitleStyle;
};

}

QT_END_NAMESPACE

#endif
