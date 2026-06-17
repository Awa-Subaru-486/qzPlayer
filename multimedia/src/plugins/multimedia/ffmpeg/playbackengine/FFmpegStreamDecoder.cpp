#include "playbackengine/FFmpegStreamDecoder_p.h"
#include "playbackengine/FFmpegMediaDataHolder_p.h"
import qzLog;
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtCore/qbytearray.h>
#include <cstring>

QT_BEGIN_NAMESPACE

// 日志分类
static qz::Log::LogCategory qLcStreamDecoder("qz.multimedia.ffmpeg.streamdecoder");

namespace ffmpeg {
// ============================================================================
// 构造函数与析构函数
// ============================================================================

StreamDecoder::StreamDecoder(const PlaybackEngineObjectID &id, const CodecContext &codecContext,
                             TrackPosition absSeekPos)
    : PlaybackEngineObject(id),
      m_codecContext(codecContext),
      m_absSeekPos(absSeekPos),
      // 根据编解码器类型确定轨道类型(音频/视频/字幕)
      m_trackType(MediaDataHolder::trackTypeFromMediaType(codecContext.context()->codec_type))
{
    // =========================================================================
    // 流解码器构造函数
    // 初始化解码器，设置编解码上下文和跳转位置
    // =========================================================================
    qz::Log::cat_debug(qLcStreamDecoder, "Create stream decoder, trackType {} absSeekPos:{}", static_cast<int>(m_trackType), absSeekPos.get());
    Q_ASSERT(m_trackType != PlatformMediaPlayer::NTrackTypes);
}

StreamDecoder::~StreamDecoder()
{
    // =========================================================================
    // 析构函数
    // 清空编解码器缓冲区，释放未解码的数据
    // =========================================================================
    avcodec_flush_buffers(m_codecContext.context());
}

// ============================================================================
// 数据包接收与处理
// ============================================================================
void StreamDecoder::onFinalPacketReceived(PlaybackEngineObjectID sourceID)
{
    // =========================================================================
    // 最终数据包接收回调
    // 当解复用器发送完所有数据包后调用，触发解码器刷新
    // =========================================================================
    // 检查会话ID是否匹配
    if (checkSessionID(sourceID.sessionID))
        decode({});  // 发送空数据包触发刷新
}

void StreamDecoder::decode(Packet packet)
{
    // =========================================================================
    // 解码入口函数
    // 接收来自解复用器的数据包，加入队列等待解码
    // =========================================================================

    // 检查数据包的会话ID是否过期
    if (packet.isValid() && !checkSessionID(packet.sourceID().sessionID)) {
        qz::Log::cat_debug(qLcStreamDecoder, "Packet session outdated. Source id:[session:{},object:{}] current id:[session:{},object:{}]", packet.sourceID().sessionID, packet.sourceID().objectID, id().sessionID, id().objectID);
        return;
    }

    // 将数据包加入解码队列
    m_packets.enqueue(std::move(packet));

    // 调度下一步解码操作
    scheduleNextStep();
}

void StreamDecoder::doNextStep()
{
    // =========================================================================
    // 解码线程主循环 - 从队列取出数据包并执行解码
    // =========================================================================

    // 步骤1: 从队列取出待解码的数据包
    Packet packet = m_packets.dequeue();

    // 步骤2: 定义解码函数
    // 根据轨道类型选择不同的解码方法
    auto decodePacket = [this](const Packet &packet) {
        if (trackType() == PlatformMediaPlayer::SubtitleStream)
            decodeSubtitle(packet);  // 字幕解码
        else
            decodeMedia(packet);      // 音频/视频解码
    };

    // =========================================================================
    // 步骤3: 处理循环播放的循环索引变化
    // =========================================================================
    // 当数据包的循环索引与当前索引不同时，需要刷新解码器缓冲区
    if (packet.isValid() && packet.loopOffset().loopIndex != m_offset.loopIndex) {

        // 先刷新解码器(发送空数据包)
        decodePacket({});

        qz::Log::cat_debug(qLcStreamDecoder, "flush buffers due to new loop:{}", packet.loopOffset().loopIndex);

        // 清空 FFmpeg 编解码器缓冲区
        avcodec_flush_buffers(m_codecContext.context());
        // 更新当前循环偏移
        m_offset = packet.loopOffset();
    }

    // =========================================================================
    // 步骤4: 执行解码
    // =========================================================================
    decodePacket(packet);

    // =========================================================================
    // 步骤5: 更新状态并通知上层
    // =========================================================================
    // 设置结束标志(如果数据包无效表示已到达末尾)
    setAtEnd(!packet.isValid());

    // 通知解复用器数据包已处理完成
    if (packet.isValid())
        emit packetProcessed(std::move(packet));

    // 调度下一次解码
    scheduleNextStep();
}

PlatformMediaPlayer::TrackType StreamDecoder::trackType() const
{
    // 获取当前轨道类型
    return m_trackType;
}

qint32 StreamDecoder::maxQueueSize(PlatformMediaPlayer::TrackType type)
{
    // =========================================================================
    // 获取指定轨道类型的最大队列大小
    // 队列大小决定了可以同时等待处理的帧数
    // 
    // 视频队列较小(3): 因为视频帧较大，处理时间长
    // 音频队列较大(9): 因为音频帧较小，需要更多缓冲保证流畅
    // 字幕队列中等(6): 平衡内存占用和流畅度
    // =========================================================================
    switch (type) {
    case PlatformMediaPlayer::VideoStream:
        return 3;   // 视频最大队列大小
    case PlatformMediaPlayer::AudioStream:
        return 9;   // 音频最大队列大小
    case PlatformMediaPlayer::SubtitleStream:
        return 6;   // 字幕最大队列大小
    default:
        Q_UNREACHABLE_RETURN(-1);
    }
}

void StreamDecoder::onFrameProcessed(Frame frame)
{
    // =========================================================================
    // 帧处理完成回调
    // 当渲染器处理完一个帧后调用，用于更新待处理帧计数
    // =========================================================================

    // 检查帧是否属于当前解码器实例
    if (!checkID(frame.sourceID()))
        return;

    // 减少待处理帧计数
    --m_pendingFramesCount;
    Q_ASSERT(m_pendingFramesCount >= 0);

    // 调度下一次解码
    scheduleNextStep();
}

bool StreamDecoder::canDoNextStep() const
{
    // =========================================================================
    // 检查是否可以执行下一步解码
    // 条件:
    //   1. 数据包队列不为空
    //   2. 待处理帧数未超过最大队列大小
    //   3. 父类允许执行
    // =========================================================================
    const qint32 maxCount = maxQueueSize(m_trackType);

    return !m_packets.empty() && m_pendingFramesCount < maxCount
            && PlaybackEngineObject::canDoNextStep();
}

void StreamDecoder::onFrameFound(Frame frame)
{
    // =========================================================================
    // 帧发现回调
    // 当解码出一个新帧后调用，用于通知上层处理
    // =========================================================================

    // 如果帧结束位置在跳转位置之前，丢弃该帧(跳转后不需要的帧)
    if (frame.isValid() && frame.absoluteEnd() < m_absSeekPos)
        return;

    // 增加待处理帧计数
    Q_ASSERT(m_pendingFramesCount >= 0);
    ++m_pendingFramesCount;

    // 发送信号，请求处理该帧
    emit requestHandleFrame(frame);
}

void StreamDecoder::decodeMedia(const Packet &packet)
{

    // =========================================================================
    // 音频/视频解码函数
    // 使用 FFmpeg 的 avcodec_send_packet / avcodec_receive_frame 模式
    // 
    // 解码流程:
    //   1. 发送数据包到解码器 (avcodec_send_packet)
    //   2. 从解码器接收解码后的帧 (avcodec_receive_frame)
    // =========================================================================

    // 步骤1: 发送数据包到解码器
    auto sendPacketResult = sendAVPacket(packet);

    // =========================================================================
    // 处理 EAGAIN 错误
    // EAGAIN 表示解码器缓冲区已满，需要先取出帧再发送
    // =========================================================================
    if (sendPacketResult == AVERROR(EAGAIN)) {

        // 先接收已解码的帧，腾出空间
        receiveAVFrames();
        // 再次尝试发送数据包
        sendPacketResult = sendAVPacket(packet);

        // 如果第二次发送成功，说明 FFmpeg 行为异常(理论上应该还是 EAGAIN)
        if (sendPacketResult != AVERROR(EAGAIN))
            qz::Log::warn("Unexpected FFmpeg behavior");
    }

    // =========================================================================
    // 步骤2: 接收解码后的帧
    // =========================================================================
    // 如果数据包发送成功，接收所有解码后的帧
    // flushPacket 参数表示是否为刷新数据包(文件末尾)
    if (sendPacketResult == 0)
        receiveAVFrames(!packet.isValid());
}

int StreamDecoder::sendAVPacket(const Packet &packet)
{
    // =========================================================================
    // 发送数据包到 FFmpeg 解码器
    // 
    // 参数: packet - 数据包(无效时发送 nullptr 触发解码器刷新)
    // 返回: FFmpeg 错误码
    // =========================================================================

    return avcodec_send_packet(m_codecContext.context(),
                               packet.isValid() ? packet.avPacket() : nullptr);
}

void StreamDecoder::receiveAVFrames(bool flushPacket)
{
    // =========================================================================
    // 从 FFmpeg 解码器接收解码后的帧
    // 
    // 循环接收直到:
    //   - 解码器返回 EAGAIN (需要更多数据)
    //   - 解码器返回 EOF (已刷新完成)
    //   - 解码器返回错误
    // 
    // 参数: flushPacket - 是否为刷新模式(文件末尾)
    // =========================================================================
    while (true) {

        // 分配 AVFrame 用于存储解码后的数据
        auto avFrame = makeAVFrame();

        // 从解码器接收一帧
        const auto receiveFrameResult = avcodec_receive_frame(m_codecContext.context(), avFrame.get());

        // =========================================================================
        // 处理接收结果
        // =========================================================================
        if (receiveFrameResult == AVERROR_EOF || receiveFrameResult == AVERROR(EAGAIN)) {

            // 特殊情况: 刷新模式下收到 EAGAIN 是异常的
            // 这表示解码器还有数据但无法输出，继续尝试
            if (flushPacket && receiveFrameResult == AVERROR(EAGAIN)) {
                qz::Log::warn("Unexpected FFmpeg behavior: EAGAIN state for "
                              "avcodec_receive_frame at end of the stream");
                flushPacket = false;
                continue;
            }
            // 正常结束: EOF 或需要更多数据
            m_consecutiveErrors = 0;
            break;
        }

        // 处理其他错误：对非致命错误容忍，避免偶发错误终止播放
        if (receiveFrameResult < 0) {
            ++m_consecutiveErrors;

            // 非致命错误容忍策略（参考 QMplay2）
            if ((receiveFrameResult == AVERROR_INVALIDDATA
                 || receiveFrameResult == AVERROR_EXTERNAL
                 || receiveFrameResult == AVERROR_EXIT)
                && m_consecutiveErrors < maxConsecutiveErrors) {
                qz::Log::cat_warn(qLcStreamDecoder,
                                  "Tolerating decode error ({}/{}): {}",
                                  m_consecutiveErrors, maxConsecutiveErrors,
                                  err2str(receiveFrameResult));
                break; // 跳过当前帧，继续解码下一个包
            }

            emit error(::MediaPlayer::FormatError, err2str(receiveFrameResult));
            break;
        }

        // =========================================================================
        // 成功接收到一帧
        // =========================================================================
        m_consecutiveErrors = 0;
        // 如果是视频流，可能需要从硬件缓冲区复制数据
        if (m_trackType == PlatformMediaPlayer::VideoStream)
            avFrame = copyFromHwPool(std::move(avFrame));

        // 通知上层发现新帧
        onFrameFound({ m_offset, std::move(avFrame), m_codecContext, id() });
    }
}

void StreamDecoder::decodeSubtitle(const Packet &packet)
{
    // =========================================================================
    // 字幕解码函数
    // 使用 FFmpeg 的 avcodec_decode_subtitle2 解码字幕
    //
    // 字幕解码与音频/视频不同，使用专门的字幕解码 API
    // =========================================================================
    // 忽略无效数据包(字幕解码不支持刷新)
    if (!packet.isValid())
        return;

    // 准备字幕结构体
    AVSubtitle subtitle = {};
    int gotSubtitle = 0;

    // =========================================================================
    // 步骤1: 解码字幕
    // =========================================================================
    const auto *avPkt = packet.avPacket();

    // MKV 容器可能对字幕数据使用 zlib 压缩（Content Compression），
    // FFmpeg 的 av_read_frame 对字幕流可能不会自动解压。
    // 检测 zlib 压缩头 (0x78 + 0x01/0x5E/0x9C/0xDA) 并解压。
    AVPacketUPtr decompressedPacket;
    const AVPacket *decodePkt = avPkt;

    if (avPkt->size >= 2 && avPkt->data) {
        const uint8_t *d = avPkt->data;
        if (d[0] == 0x78 && (d[1] == 0x01 || d[1] == 0x5E || d[1] == 0x9C || d[1] == 0xDA)) {
            // 检测到 zlib 压缩，使用 qUncompress 解压
            // qUncompress 需要前 4 字节为未压缩大小 (big-endian)
            QByteArray decompressedData;
            bool success = false;

            for (int multiplier : {20, 50, 100}) {
                quint32 estSize = static_cast<quint32>(avPkt->size) * multiplier;
                QByteArray wrappedData(sizeof(quint32), Qt::Uninitialized);
                wrappedData[0] = static_cast<char>((estSize >> 24) & 0xFF);
                wrappedData[1] = static_cast<char>((estSize >> 16) & 0xFF);
                wrappedData[2] = static_cast<char>((estSize >> 8) & 0xFF);
                wrappedData[3] = static_cast<char>(estSize & 0xFF);
                wrappedData.append(reinterpret_cast<const char *>(d), avPkt->size);

                decompressedData = qUncompress(wrappedData);
                if (!decompressedData.isEmpty()) {
                    success = true;
                    break;
                }
            }

            if (success) {
                // 创建新的 AVPacket 存放解压后的数据
                decompressedPacket.reset(av_packet_alloc());
                const int destLen = decompressedData.size();
                av_new_packet(decompressedPacket.get(), destLen);
                memcpy(decompressedPacket->data, decompressedData.constData(), destLen);
                decompressedPacket->pts = avPkt->pts;
                decompressedPacket->dts = avPkt->dts;
                decompressedPacket->duration = avPkt->duration;
                decompressedPacket->flags = avPkt->flags;
                decompressedPacket->stream_index = avPkt->stream_index;
                decodePkt = decompressedPacket.get();
            } else {
                qz::Log::cat_warn(qLcStreamDecoder,
                                   "Subtitle packet zlib decompression failed");
            }
        }
    }

    const int res =
            avcodec_decode_subtitle2(m_codecContext.context(), &subtitle, &gotSubtitle,
                                      const_cast<AVPacket *>(decodePkt));

    // 检查解码结果
    if (res < 0) {
        qz::Log::cat_warn(qLcStreamDecoder, "avcodec_decode_subtitle2 failed: error={} ({})",
                           res, err2str(res));
        return;
    }
    if (!gotSubtitle)
        return;

    // =========================================================================
    // 步骤2: 计算字幕时间范围
    // =========================================================================
    //
    // 核心公式:
    //   绝对开始时间 = start_display_time / 1000.0 + packet.pts × time_base
    //   显示时长:
    //     if (end_display_time != UINT32_MAX && end_display_time != start_display_time)
    //       duration = (end_display_time - start_display_time) / 1000.0
    //     else
    //       hasDuration = false (无明确结束时间, 持续显示直到被替换或清除)
    TrackPosition start = 0;
    TrackDuration duration = 0;
    bool hasDuration = false;

    // 计算 basePos (包 PTS 对应的轨道位置)
    TrackPosition basePos = 0;
    bool hasBasePos = false;

    if (packet.avPacket()->pts != AV_NOPTS_VALUE) {
        basePos = m_codecContext.toTrackPosition(AVStreamPosition(packet.avPacket()->pts));
        hasBasePos = true;
    } else if (subtitle.pts != AV_NOPTS_VALUE) {
        // 数据包 PTS 无效，尝试将 subtitle.pts 视为流时间基准
        basePos = m_codecContext.toTrackPosition(AVStreamPosition(subtitle.pts));
        hasBasePos = true;
    }

    if (hasBasePos) {
        // 绝对开始时间 = basePos + start_display_time偏移
        start = basePos + TrackDuration(static_cast<qint64>(subtitle.start_display_time) * 1000);

        // 时长计算:
        // end_display_time == UINT32_MAX: 无明确结束时间 (PGS/DVB等)
        // end_display_time == start_display_time: 无实际显示时长
        if (subtitle.end_display_time != UINT32_MAX
            && subtitle.end_display_time != subtitle.start_display_time) {
            duration = TrackDuration(
                    static_cast<qint64>(subtitle.end_display_time - subtitle.start_display_time) * 1000);
            hasDuration = true;
        }
    } else {
        // 没有任何有效的 PTS 信息，使用包的 duration
        start = m_codecContext.toTrackPosition(AVStreamPosition(packet.avPacket()->pts));
        duration = m_codecContext.toTrackDuration(AVStreamDuration(packet.avPacket()->duration));
        hasDuration = (duration > TrackDuration(0));
    }

    // =========================================================================
    // 步骤3: 提取字幕内容(文本或位图)
    // =========================================================================
    QString text;
    QImage bitmapImage;
    QRect bitmapRect;
    bool hasBitmap = false;
    SubtitleBitmapData bitmapData;
    bool hasBitmapData = false;

    for (uint i = 0; i < subtitle.num_rects; ++i) {
        const auto *r = subtitle.rects[i];

        if (r->type == SUBTITLE_BITMAP) {
            // 位图字幕 (PGS/DVD/PNG 等)
            if (r->w > 0 && r->h > 0 && r->data[0]) {
                QImage image;

                if (r->data[1] && r->nb_colors > 0) {
                    // 有调色板的索引位图 (PGS/DVD 字幕)：GPU 路径
                    // 收集所有 rect，合并调色板
                    if (!hasBitmapData) {
                        bitmapData.nbColors = r->nb_colors;
                        auto paletteBuf = std::make_shared<QVector<uint32_t>>(r->nb_colors);
                        memcpy(paletteBuf->data(), r->data[1],
                               r->nb_colors * sizeof(uint32_t));
                        bitmapData.palette = std::move(paletteBuf);
                        hasBitmapData = true;
                    }

                    // 添加 rect 到列表
                    SubtitleBitmapData::Rect rect;
                    rect.x = r->x;
                    rect.y = r->y;
                    rect.w = r->w;
                    rect.h = r->h;

                    auto indexBuf = std::make_shared<QVector<uint8_t>>(r->w * r->h);
                    for (int row = 0; row < r->h; ++row) {
                        memcpy(indexBuf->data() + row * r->w,
                               r->data[0] + row * r->linesize[0], r->w);
                    }
                    rect.indexData = std::move(indexBuf);
                    bitmapData.rects.append(rect);

                    continue;
                }

                // 无调色板：CPU 端处理为 QImage
                if (r->linesize[0] >= r->w * 4) {
                    image = QImage(r->w, r->h, QImage::Format_ARGB32_Premultiplied);
                    for (int y = 0; y < r->h; ++y) {
                        const uint8_t *src = r->data[0] + y * r->linesize[0];
                        memcpy(image.scanLine(y), src, r->w * 4);
                    }
                    image = image.convertedTo(QImage::Format_ARGB32_Premultiplied);
                } else if (r->linesize[0] > 0) {
                    image = QImage(reinterpret_cast<const uchar *>(r->data[0]),
                                   r->w, r->h, r->linesize[0],
                                   QImage::Format_ARGB32);
                    if (!image.isNull()) {
                        image = image.convertedTo(QImage::Format_ARGB32_Premultiplied);
                    } else {
                        int dataSize = r->linesize[0] * r->h;
                        image = QImage::fromData(r->data[0], dataSize);
                        if (!image.isNull())
                            image = image.convertedTo(QImage::Format_ARGB32_Premultiplied);
                    }
                }

                if (!image.isNull()) {
                    if (hasBitmap) {
                        const QRect newRect(r->x, r->y, r->w, r->h);
                        const QRect unitedRect = bitmapRect.united(newRect);
                        QImage combined(unitedRect.size(), QImage::Format_ARGB32_Premultiplied);
                        combined.fill(Qt::transparent);
                        QPainter p(&combined);
                        p.setCompositionMode(QPainter::CompositionMode_Source);
                        p.drawImage(QPoint(bitmapRect.topLeft() - unitedRect.topLeft()),
                                    bitmapImage);
                        p.drawImage(QPoint(newRect.topLeft() - unitedRect.topLeft()),
                                    image);
                        p.end();
                        bitmapImage = std::move(combined);
                        bitmapRect = unitedRect;
                    } else {
                        bitmapImage = std::move(image);
                        bitmapRect = QRect(r->x, r->y, r->w, r->h);
                        hasBitmap = true;
                    }
                }
            }
        } else {
            if (i)
                text += QLatin1Char('\n');

            if (r->text) {
                text += QString::fromUtf8(static_cast<const char*>(r->text));
            } else if (r->ass) {
                const char *ass = r->ass;
                int nCommas = 0;
                while (*ass) {
                    if (nCommas == 8)
                        break;
                    if (*ass == ',')
                        ++nCommas;
                    ++ass;
                }
                text += QString::fromUtf8(ass);
            }
        }
    }

    // =========================================================================
    // 步骤4: 规范化字幕文本
    // =========================================================================
    text.replace(QLatin1String("\\N"), QLatin1String("\n"));
    text.replace(QLatin1String("\\n"), QLatin1String("\n"));
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (text.endsWith(QLatin1Char('\n')))
        text.chop(1);

    // =========================================================================
    // 步骤5: 发送字幕帧
    // =========================================================================
    // PGS 解码器在收到 "hide" 段时会发出 num_rects==0 的字幕事件，
    // 这表示当前字幕应该被清除
    if (subtitle.num_rects == 0) {
        onFrameFound({ m_offset, SubtitleBitmapData{}, start, TrackDuration(0), id() });
        onFrameFound({ m_offset, QImage(), QRect(), start, TrackDuration(0), id() });
        onFrameFound({ m_offset, QString(), start, TrackDuration(0), id() });
        return;
    }

    // 有明确结束时间的字幕（如 ASS/SRT/DVD Sub），在结束时间点生成清除帧。
    // 无明确结束时间的字幕（如 PGS 的 end_display_time=UINT32_MAX），
    // hasDuration=false，不生成清除帧，由下一帧自动替换或清除事件清除。
    const TrackPosition endTime = start + duration;

    if (hasBitmapData) {
        onFrameFound({ m_offset, bitmapData, start, duration, id(), hasDuration });
        if (hasDuration) {
            onFrameFound({ m_offset, SubtitleBitmapData{}, endTime, TrackDuration(0), id() });
        }
    } else if (hasBitmap) {
        onFrameFound({ m_offset, bitmapImage, bitmapRect, start, duration, id(), hasDuration });
        if (hasDuration) {
            onFrameFound({ m_offset, QImage(), QRect(), endTime, TrackDuration(0), id() });
        }
    } else if (!text.isEmpty()) {
        onFrameFound({ m_offset, text, start, duration, id(), hasDuration });
        if (hasDuration) {
            onFrameFound({ m_offset, QString(), endTime, TrackDuration(0), id() });
        }
    }
}

}

QT_END_NAMESPACE

#include "moc_FFmpegStreamDecoder_p.cpp"
