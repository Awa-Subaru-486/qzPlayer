#ifndef PLAYBACKENGINE_FFMPEGVIDEORENDERER_P_H
#define PLAYBACKENGINE_FFMPEGVIDEORENDERER_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegRenderer_p.h>

#include <qzMultimedia/private/VideoTransformation_p.h>
#include <QtCore/qpointer.h>

QT_BEGIN_NAMESPACE

class VideoSink;

namespace ffmpeg {

// 视频渲染器，将解码后的视频帧渲染到视频输出
class VideoRenderer : public Renderer
{
    Q_OBJECT
public:
    VideoRenderer(const PlaybackEngineObjectID &id, const TimeController &tc, ::VideoSink *sink,
                  const VideoTransformation &transform);

    void setOutput(::VideoSink *sink, bool cleanPrevSink = false);

protected:
    RenderingResult renderInternal(Frame frame) override;

private:
    QPointer<::VideoSink> m_sink;
    VideoTransformation m_transform;
};

}

QT_END_NAMESPACE

#endif
