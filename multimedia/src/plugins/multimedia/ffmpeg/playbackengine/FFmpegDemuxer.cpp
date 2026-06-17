

#include "playbackengine/FFmpegDemuxer_p.h"
import qzLog;
#include <chrono>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// ============================================================================
// 常量定义
// ============================================================================
// 最大缓冲时长: 4秒
// 当已缓冲的数据达到此时长，暂停读取以避免内存溢出
static constexpr TrackDuration MaxBufferedDurationUs{ 4'000'000 };

// 最大缓冲大小: 32MB
// 当已缓冲的数据达到此大小，暂停读取以避免内存溢出
static constexpr qint64 MaxBufferedSize = 32 * 1024 * 1024;

// 日志分类
static qz::Log::LogCategory qLcDemuxer("qz.multimedia.ffmpeg.demuxer");

// ============================================================================
// 辅助函数
// ============================================================================
// 计算数据包的结束位置
// 参数: packet - 数据包
//       stream - 数据包所属的流
//       context - AVFormatContext
// 返回: 数据包结束位置(轨道时间)
static TrackPosition packetEndPos(const Packet &packet, const AVStream *stream,
                                  const AVFormatContext *context)
{
    const AVPacket &avPacket = *packet.avPacket();

    // 结束位置 = 循环偏移 + 数据包结束时间
    return packet.loopOffset().loopStartTimeUs.asDuration()
            + toTrackPosition(AVStreamPosition(avPacket.pts + avPacket.duration), stream, context);
}

// 检查数据包是否在流的时长范围内
// 用于处理某些媒体文件时长估计不准确的情况
static bool isPacketWithinStreamDuration(const AVFormatContext *context, const Packet &packet)
{
    const AVPacket &avPacket = *packet.avPacket();
    const AVStream &avStream = *context->streams[avPacket.stream_index];
    const AVStreamDuration streamDuration(avStream.duration);

    // 如果流时长无效，或者时长估计方法不是从流获取，则认为有效
    if (streamDuration.get() <= 0
        || context->duration_estimation_method != AVFMT_DURATION_FROM_STREAM)
        return true;

    // 如果数据包的 PTS 无效，发出警告并认为有效
    if (avPacket.pts == AV_NOPTS_VALUE) {
        qz::Log::warn("ffmpeg::Demuxer received AVPacket with pts == AV_NOPTS_VALUE");
        return true;
    }

    // 如果流有起始时间，检查数据包是否在范围内
    if (avStream.start_time != AV_NOPTS_VALUE)
        return AVStreamDuration(avPacket.pts - avStream.start_time) <= streamDuration;

    // 否则使用轨道位置进行比较
    const TrackPosition trackPos = toTrackPosition(AVStreamPosition(avPacket.pts), &avStream, context);
    const TrackPosition trackPosOfStreamEnd = toTrackDuration(streamDuration, &avStream).asTimePoint();
    return trackPos <= trackPosOfStreamEnd;

}

Demuxer::Demuxer(const PlaybackEngineObjectID &id, AVFormatContext *context,
                 TrackPosition initialPosUs, bool seekPending, const LoopOffset &loopOffset,
                 const StreamIndexes &streamIndexes, int loops)
    : PlaybackEngineObject(id),
      m_context(context),

      // 如果没有待处理的跳转且初始位置为0，则认为已经定位
      m_seeked(!seekPending
               && initialPosUs == TrackPosition{ 0 }),
      m_posInLoopUs{ initialPosUs },
      m_loopOffset(loopOffset),
      m_loops(loops)
{
    // =========================================================================
    // 解复用器构造函数
    // 初始化解复用器，设置初始位置和活动流
    // =========================================================================
    qz::Log::cat_debug(qLcDemuxer, "Create demuxer. pos:{} loop offset:{} loop index:{} loops:{}", m_posInLoopUs.get(), m_loopOffset.loopStartTimeUs.get(), m_loopOffset.loopIndex, loops);

    Q_ASSERT(m_context);

    // 遍历所有轨道类型，激活需要解复的流
    for (auto i = 0; i < PlatformMediaPlayer::NTrackTypes; ++i) {
        if (streamIndexes[i] >= 0) {
            const auto trackType = static_cast<PlatformMediaPlayer::TrackType>(i);
            qz::Log::cat_debug(qLcDemuxer, "Activate demuxing stream {}, trackType:{}", i, static_cast<int>(trackType));

            // 将流索引映射到流数据
            m_streams[streamIndexes[i]] = { trackType };
        }
    }
}

void Demuxer::doNextStep()
{

    // =========================================================================
    // 解复用线程主循环 - 从媒体文件中读取数据包并分发到对应的解码器
    // =========================================================================
    // 步骤1: 确保已定位到正确的播放位置
    // 如果需要跳转(seek)，则执行跳转操作
    ensureSeeked();

    // 步骤2: 创建一个新的数据包对象
    // 分配 AVPacket 内存，用于存储从媒体文件读取的数据
    Packet packet(m_loopOffset, AVPacketUPtr{ av_packet_alloc() }, id());
    AVPacket &avPacket = *packet.avPacket();

    // 步骤3: 从媒体文件中读取一个数据包
    // av_read_frame 是 FFmpeg 核心函数，从 AVFormatContext 读取下一个数据包
    // 返回值: 0=成功, AVERROR_EOF=文件结束, 负值=错误
    const int demuxStatus = av_read_frame(m_context, &avPacket);

    // 步骤4: 检查是否收到退出信号
    // AVERROR_EXIT 表示播放被中断，直接返回
    if (demuxStatus == AVERROR_EXIT)
        return;

    // 步骤5: 确定数据包所属的流是否是我们关心的流
    // 一个媒体文件可能包含多个流(视频、音频、字幕等)
    // 我们只处理被选中的流
    const int streamIndex = avPacket.stream_index;
    auto streamIterator = m_streams.find(streamIndex);
    const bool streamIsRelevant = streamIterator != m_streams.end();

    // 诊断日志：字幕流包被读取但不在活动流中
    if (!streamIsRelevant && demuxStatus >= 0) {
        const auto *stream = m_context->streams[streamIndex];
        if (stream && stream->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            qz::Log::cat_debug(qLcDemuxer, "Subtitle packet read but stream not active: streamIndex={} pts={} "
                                "duration={} size={})",
                                streamIndex, avPacket.pts, avPacket.duration, avPacket.size);
        }
    }

    // =========================================================================
    // 步骤6: 处理文件结束或循环播放逻辑
    // =========================================================================
    if (demuxStatus == AVERROR_EOF
        || (streamIsRelevant && !isPacketWithinStreamDuration(m_context, packet))) {
        // 递增循环索引
        ++m_loopOffset.loopIndex;

        // 获取循环次数设置
        const auto loops = m_loops.loadAcquire();

        // 情况6a: 循环次数已达到上限，播放结束
        if (loops >= 0 && m_loopOffset.loopIndex >= loops) {

            qz::Log::cat_debug(qLcDemuxer, "finish demuxing");

            // 标记缓冲完成，通知上层
            if (!std::exchange(m_buffered, true))
                emit packetsBuffered();

            // 设置结束标志
            setAtEnd(true);
        } else {

            // 情况6b: 继续循环播放
            // 重置状态，从头开始播放
            m_seeked = false;
            m_posInLoopUs = TrackPosition(0);
            m_loopOffset.loopStartTimeUs = m_maxPacketsEndPos;
            m_maxPacketsEndPos = TrackPosition(0);

            // 重新定位到文件开头
            ensureSeeked();

            qz::Log::cat_debug(qLcDemuxer, "Demuxer loops changed. Index:{} Offset:{}", m_loopOffset.loopIndex, m_loopOffset.loopStartTimeUs.get());

            // 调度下一次读取
            scheduleNextStep();
        }

        return;
    }

    // =========================================================================
    // 步骤7: 处理读取错误
    // =========================================================================
    if (demuxStatus < 0) {
        qz::Log::cat_warn(qLcDemuxer, "Demuxing failed {} {}", demuxStatus, static_cast<int>(AVError(demuxStatus)));

        // 情况7a: EAGAIN 错误，可以重试
        // EAGAIN 表示资源暂时不可用，稍后重试
        if (demuxStatus == AVERROR(EAGAIN) && m_demuxerRetryCount != s_maxDemuxerRetries) {
            m_failTimePoint = std::chrono::steady_clock::now();
            ++m_demuxerRetryCount;

            qz::Log::cat_debug(qLcDemuxer, "Retrying");
            scheduleNextStep();
        } else {

            // 情况7b: 其他错误，报告失败
            emit error(::MediaPlayer::ResourceError,
                       QLatin1StringView("Demuxing failed"));
        }

        return;
    }

    // =========================================================================
    // 步骤8: 成功读取数据包，更新状态
    // =========================================================================
    // 重置重试计数器
    m_demuxerRetryCount = 0;
    m_failTimePoint.reset();

    // =========================================================================
    // 步骤9: 处理有效的数据包
    // =========================================================================
    if (streamIsRelevant) {
        auto &streamData = streamIterator->second;
        const AVStream *stream = m_context->streams[streamIndex];

        // 计算数据包结束位置
        const TrackPosition endPos = packetEndPos(packet, stream, m_context);

        // 内核级片头跳过：如果数据包结束位置 <= opening，跳过不解码
        if (m_openingUs > TrackPosition(0) && endPos <= m_openingUs + m_loopOffset.loopStartTimeUs.asDuration()) {
            scheduleNextStep();
            return;
        }

        // 内核级片尾跳过：如果数据包位置 >= ending，视为文件结束
        if (m_endingUs > TrackPosition(0) && endPos > m_endingUs + m_loopOffset.loopStartTimeUs.asDuration()) {
            ++m_loopOffset.loopIndex;
            const auto loops = m_loops.loadAcquire();
            if (loops >= 0 && m_loopOffset.loopIndex >= loops) {
                qz::Log::cat_debug(qLcDemuxer, "Ending reached, finish demuxing at {}", endPos.get());
                if (!std::exchange(m_buffered, true))
                    emit packetsBuffered();
                setAtEnd(true);
                return;
            }
            scheduleNextStep();
            return;
        }
        // 更新最大数据包结束位置(用于循环播放)
        m_maxPacketsEndPos = qMax(m_maxPacketsEndPos, endPos);

        // 更新流的缓冲统计信息
        // bufferedDuration: 已缓冲的时长
        // bufferedSize: 已缓冲的数据大小
        // maxSentPacketsPos: 已发送数据包的最大位置
        streamData.bufferedDuration += toTrackDuration(AVStreamDuration(avPacket.duration), stream);
        streamData.bufferedSize += avPacket.size;
        streamData.maxSentPacketsPos = qMax(streamData.maxSentPacketsPos, endPos);
        // 检查是否达到缓冲上限
        updateStreamDataLimitFlag(streamData);

        // 检查是否首次发现数据包
        // 首次发现时通知上层，用于确定播放起始位置
        if (!m_buffered && streamData.isDataLimitReached) {
            m_buffered = true;
            emit packetsBuffered();
        }

        // 通知上层缓冲进度变化
        // 取所有活动流中 maxSentPacketsPos 的最小值作为已缓冲的最远位置
        qint64 minBufferedEnd = std::numeric_limits<qint64>::max();
        for (const auto &entry : m_streams) {
            const auto &sd = entry.second;
            minBufferedEnd = std::min(minBufferedEnd, sd.maxSentPacketsPos.get());
        }
        if (minBufferedEnd != std::numeric_limits<qint64>::max())
            emit bufferProgressChanged(minBufferedEnd);

        // 首次找到数据包时发送信号
        if (!m_firstPacketFound) {
            m_firstPacketFound = true;
            emit firstPacketFound(id(), m_posInLoopUs + m_loopOffset.loopStartTimeUs.asDuration());
        }

        // =========================================================================
        // 步骤10: 根据流类型分发数据包到对应的解码器
        // =========================================================================
        // 通过流类型获取对应的信号(视频/音频/字幕)
        const auto signal = signalByTrackType(streamData.trackType);

        // 发送信号，将数据包传递给解码器
        emit (this->*signal)(std::move(packet));
    }

    // =========================================================================
    // 步骤11: 调度下一次读取
    // =========================================================================
    // 继续读取下一个数据包
    scheduleNextStep();
}

void Demuxer::onPacketProcessed(Packet packet)
{
    // =========================================================================
    // 数据包处理完成回调
    // 当解码器处理完一个数据包后调用此函数，用于更新缓冲统计信息
    // =========================================================================
    Q_ASSERT(packet.isValid());

    // 检查数据包是否属于当前解复用器实例
    // 如果是旧实例的数据包，忽略它
    if (!checkID(packet.sourceID()))
        return;

    auto &avPacket = *packet.avPacket();

    // 获取数据包所属流的信息
    const auto streamIndex = avPacket.stream_index;
    const auto stream = m_context->streams[streamIndex];

    // 更新流的缓冲统计信息
    if (m_streams.contains(streamIndex)) {
        auto &streamData = m_streams.at(streamIndex);

        // 减少已缓冲时长和数据大小
        // 因为数据包已被处理，不再占用缓冲区
        streamData.bufferedDuration -= toTrackDuration(AVStreamDuration(avPacket.duration), stream);
        streamData.bufferedSize -= avPacket.size;
        // 更新已处理数据包的最大位置
        streamData.maxProcessedPacketPos =
                qMax(streamData.maxProcessedPacketPos, packetEndPos(packet, stream, m_context));

        // 断言确保统计信息有效
        Q_ASSERT(streamData.bufferedDuration >= TrackDuration(0));
        Q_ASSERT(streamData.bufferedSize >= 0);

        // 更新数据限制标志，可能触发继续读取
        updateStreamDataLimitFlag(streamData);
    }

    // 通知上层缓冲进度变化（缓冲数据被消费后进度可能回退）
    qint64 minBufferedEnd = std::numeric_limits<qint64>::max();
    for (const auto& val : m_streams | std::views::values) {
        const auto &sd = val;
        minBufferedEnd = std::min(minBufferedEnd, sd.maxSentPacketsPos.get());
    }
    if (minBufferedEnd != std::numeric_limits<qint64>::max())
        emit bufferProgressChanged(minBufferedEnd);

    // 调度下一次读取操作
    scheduleNextStep();
}

Demuxer::TimePoint Demuxer::nextTimePoint() const
{
    // =========================================================================
    // 计算下一次执行的时间点
    // 如果正在重试，则返回重试时间；否则返回默认时间点
    // =========================================================================
    Q_ASSERT(m_failTimePoint.has_value() == !!m_demuxerRetryCount);

    // 如果有失败时间点，则等待一段时间后重试
    // 否则使用父类的默认时间点
    return m_failTimePoint ? *m_failTimePoint + s_demuxerRetryInterval
                           : PlaybackEngineObject::nextTimePoint();
}

bool Demuxer::canDoNextStep() const
{
    // =========================================================================
    // 检查是否可以执行下一步
    // 条件: 
    //   1. 父类允许执行
    //   2. 未到达文件末尾
    //   3. 有活动的流
    //   4. 所有流都未达到缓冲上限
    // =========================================================================
    // 检查流是否达到数据限制的 lambda 函数
    auto isDataLimitReached = [](const auto &streamIndexToData) {
        return streamIndexToData.second.isDataLimitReached;
    };

    // 综合检查所有条件
    return PlaybackEngineObject::canDoNextStep() && !isAtEnd() && !m_streams.empty()
            && std::ranges::none_of(m_streams, isDataLimitReached);
}

void Demuxer::ensureSeeked()
{
    // =========================================================================
    // 确保已定位到正确的播放位置
    // 如果尚未定位，则执行 seek 操作跳转到目标位置
    // =========================================================================

    // 如果已经定位过，直接返回
    if (std::exchange(m_seeked, true))
        return;

    // 检查媒体是否支持跳转
    // AVFMTCTX_UNSEEKABLE 标志表示媒体不可跳转(如某些网络流)
    if ((m_context->ctx_flags & AVFMTCTX_UNSEEKABLE) == 0) {

        // 将轨道位置转换为 AVFormatContext 的位置
        const AVContextPosition seekPos = toContextPosition(m_posInLoopUs, m_context);

        qz::Log::cat_debug(qLcDemuxer, "Seeking to offset {}us from media start.", m_posInLoopUs.get());

        // 执行跳转操作
        // 参数: -1 表示自动选择流，AVSEEK_FLAG_BACKWARD 表示向后找最近的关键帧
        auto err = av_seek_frame(m_context, -1, seekPos.get(), AVSEEK_FLAG_BACKWARD);

        // 处理跳转失败
        if (err < 0) {
            qz::Log::cat_warn(qLcDemuxer, "Failed to seek, pos {}", seekPos.get());

            // 如果不是跳转到开头，或者媒体有有效时长，则报告错误
            if (m_posInLoopUs != TrackPosition{ 0 } || m_context->duration > 0)
                emit error(::MediaPlayer::ResourceError,
                           QLatin1StringView("Failed to seek: ") + err2str(err));
        }
    }

    // 清除结束标志，因为已经重新定位
    setAtEnd(false);
}

Demuxer::RequestingSignal Demuxer::signalByTrackType(PlatformMediaPlayer::TrackType trackType)
{
    // =========================================================================
    // 根据轨道类型获取对应的信号
    // 用于将数据包分发到正确的解码器
    // =========================================================================
    switch (trackType) {
    case PlatformMediaPlayer::TrackType::VideoStream:
        return &Demuxer::requestProcessVideoPacket;
    case PlatformMediaPlayer::TrackType::AudioStream:
        return &Demuxer::requestProcessAudioPacket;
    case PlatformMediaPlayer::TrackType::SubtitleStream:
        return &Demuxer::requestProcessSubtitlePacket;
    default:
        Q_ASSERT(!"Unknown track type");
    }

    return nullptr;
}

void Demuxer::setLoops(int loopsCount)
{
    // =========================================================================
    // 设置循环播放次数
    // 使用原子操作，确保线程安全
    // =========================================================================
    qz::Log::cat_debug(qLcDemuxer, "setLoops to demuxer {}", loopsCount);
    m_loops.storeRelease(loopsCount);
}

void Demuxer::setOpening(TrackPosition pos)
{
    // =========================================================================
    // 设置片头起始位置
    // 解复用器将跳过该位置之前的数据包，不进行解码
    // =========================================================================
    m_openingUs = pos;
}

void Demuxer::setEnding(TrackPosition pos)
{
    // =========================================================================
    // 设置片尾结束位置
    // 当解复用器读取到该位置的数据包时，将视为文件结束
    // =========================================================================
    m_endingUs = pos;
}

void Demuxer::updateStreamDataLimitFlag(StreamData &streamData)
{

    // =========================================================================
    // 更新流的数据限制标志
    // 当缓冲达到上限时，暂停读取以避免内存溢出
    // 
    // 触发条件(满足任一):
    //   1. 已缓冲时长 >= 最大缓冲时长(4秒)
    //   2. 已缓冲时长为0 且 (已发送位置 - 已处理位置) >= 最大缓冲时长
    //   3. 已缓冲数据大小 >= 最大缓冲大小(32MB)
    // =========================================================================

    // 计算已发送和已处理数据包的位置差
    const TrackDuration packetsPosDiff =
            streamData.maxSentPacketsPos - streamData.maxProcessedPacketPos;

    // 检查是否达到任一限制条件
    streamData.isDataLimitReached = streamData.bufferedDuration >= MaxBufferedDurationUs
            || (streamData.bufferedDuration == TrackDuration(0)
                && packetsPosDiff >= MaxBufferedDurationUs)
            || streamData.bufferedSize >= MaxBufferedSize;
}

}

QT_END_NAMESPACE

#include "moc_FFmpegDemuxer_p.cpp"
