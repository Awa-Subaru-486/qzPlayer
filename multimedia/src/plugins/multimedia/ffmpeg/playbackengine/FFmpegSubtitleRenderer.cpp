#include "playbackengine/FFmpegSubtitleRenderer_p.h"

#include "VideoSink.h"
import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

static qz::Log::LogCategory qLcSubtitleRenderer("qz.multimedia.ffmpeg.subtitlerenderer");

SubtitleRenderer::SubtitleRenderer(const PlaybackEngineObjectID &id, const TimeController &tc,
                                   ::VideoSink *sink)
    : Renderer(id, tc), m_sink(sink)
{
    // =========================================================================
    // 字幕渲染器构造函数
    // 初始化视频输出接收器(字幕显示在视频上)
    // =========================================================================
}

void SubtitleRenderer::setOutput(::VideoSink *sink, bool cleanPrevSink)
{
    // =========================================================================
    // 设置视频输出接收器
    // 参数: sink - 新的视频接收器
    //       cleanPrevSink - 是否清除旧接收器的字幕
    // =========================================================================
    setOutputInternal(m_sink, sink, [=](::VideoSink *prev) {
        if (!prev)
            return;

        // if (sink)
        //     sink->setSubtitleText(prev->subtitleText());

        // 如果需要清除旧接收器，清空其所有字幕数据
        if (cleanPrevSink) {
            prev->setSubtitleText({});
            prev->setSubtitleImage({}, {});
            prev->setSubtitleBitmapData({});
        }
    });
}

SubtitleRenderer::~SubtitleRenderer()
{
    // =========================================================================
    // 析构函数
    // 清空视频接收器的所有字幕数据(文本/位图/GPU调色板位图)
    // =========================================================================
    if (m_sink) {
        m_sink->setSubtitleText({});
        m_sink->setSubtitleImage({}, {});
        m_sink->setSubtitleBitmapData({});
    }
}

Renderer::RenderingResult SubtitleRenderer::renderInternal(Frame frame)
{
    if (!m_sink)
        return {};

    m_sink->setSubtitleStyle(m_subtitleStyle);

    if (frame.isValid()) {
        const auto currentPos = currentTimelinePosition();

        // 条件 3: 字幕到期 — 当前播放位置超过字幕结束时间
        //   仅当 hasDuration==true 时判断 (end_display_time != UINT32_MAX)
        //   hasDuration==false 的字幕(如PGS)持续显示直到被下一帧替换或清除事件清除
        if (frame.hasDuration() && frame.absoluteEnd() <= currentPos) {
            m_sink->setSubtitleText({});
            m_sink->setSubtitleImage({}, {});
            m_sink->setSubtitleBitmapData({});
            return { true, std::chrono::microseconds(0) };
        }

        // 渲染字幕
        if (frame.hasSubtitleBitmapData()) {
            m_sink->setSubtitleBitmapData(frame.subtitleBitmapData());
            m_sink->setSubtitleImage({}, {});
            m_sink->setSubtitleText({});
        } else if (frame.hasSubtitleImage()) {
            m_sink->setSubtitleImage(frame.subtitleImage(), frame.subtitleRect());
            m_sink->setSubtitleBitmapData({});
            m_sink->setSubtitleText({});
        } else {
            m_sink->setSubtitleText(frame.text());
            m_sink->setSubtitleImage({}, {});
            m_sink->setSubtitleBitmapData({});
        }

        // hasDuration==false: 无明确结束时间(PGS等)，由下一帧替换或清除事件清除
        if (!frame.hasDuration())
            return { true, std::chrono::microseconds(0) };

        // hasDuration==true: 有明确结束时间，帧保留在队列中直到结束时间到达时自动清除
        const auto endTimePoint = timeFromPosition(frame.absoluteEnd());
        const auto now = SteadyClock::now();
        if (endTimePoint > now) {
            auto recheckInterval = std::chrono::duration_cast<std::chrono::microseconds>(
                    endTimePoint - now);
            return { false, recheckInterval };
        }

        // endTimePoint <= now 但 currentPos < absoluteEnd，时间控制器精度问题，短重检
        return { false, std::chrono::milliseconds(1) };
    } else {
        m_sink->setSubtitleText({});
        m_sink->setSubtitleImage({}, {});
        m_sink->setSubtitleBitmapData({});
    }

    return { true, std::chrono::microseconds(0) };
}

void SubtitleRenderer::setSubtitleStyle(const SubtitleStyle &style)
{
    m_subtitleStyle = style;
}

}

QT_END_NAMESPACE

#include "moc_FFmpegSubtitleRenderer_p.cpp"
