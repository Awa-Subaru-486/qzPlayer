

#include "playbackengine/FFmpegAudioRenderer_p.h"

#include <qzMultimedia/AudioSink.h>
#include <qzMultimedia/AudioOutput.h>
#include <qzMultimedia/AudioBufferOutput.h>
#include <qzMultimedia/private/AudioBufferSupport_p.h>
#include <qzMultimedia/private/PlatformAudioOutput_p.h>

import qzLog;

#include "FFmpegAudioFrameConverter_p.h"
#include "FFmpegMediaFormatInfo_p.h"
#include "FFmpegResampler_p.h"

QT_BEGIN_NAMESPACE

// 日志分类
static qz::Log::LogCategory qLcAudioRenderer("qz.multimedia.ffmpeg.audiorenderer");

namespace ffmpeg {

using namespace std::chrono_literals;
using namespace std::chrono;

namespace {

// ============================================================================
// 音频缓冲时间常量定义
// 这些常量控制音频缓冲区的大小和行为，影响延迟和流畅度
// ============================================================================
// 目标音频缓冲时长: 110ms
// 提供足够的缓冲来处理时序变化，同时不会造成过大的延迟
constexpr auto DesiredBufferTime = 110000us;

// 最小可接受缓冲时长: 22ms
// 低于此阈值可能导致音频欠载，产生爆音
constexpr auto MinDesiredBufferTime = 22000us;

// 最大可接受缓冲时长: 64ms
// 高于此阈值，音频延迟会变得明显
constexpr auto MaxDesiredBufferTime = 64000us;

// 最小空闲缓冲时长: 10ms
// 确保总有空间写入新的音频数据
constexpr auto MinDesiredFreeBufferTime = 10000us;

// 缓冲加载状态测量时间窗口: 400ms
// 用于确定是否需要同步调整
constexpr auto BufferLoadingMeasureTime = 400ms;

// 时序计算的小偏移量: 2ms
// 用于补偿处理开销
constexpr auto DurationBias = 2ms;

// 从帧获取音频格式的辅助函数
::AudioFormat audioFormatFromFrame(const Frame &frame)
{
    auto format = MediaFormatInfo::audioFormatFromCodecParameters(
            *frame.codecContext()->stream()->codecpar);

    if (format.sampleFormat() == ::AudioFormat::Unknown && frame.avFrame())
        format.setSampleFormat(
                MediaFormatInfo::sampleFormat(AVSampleFormat(frame.avFrame()->format)));

    return format;
}

}

AudioRenderer::AudioRenderer(const PlaybackEngineObjectID &id, const TimeController &tc,
                             ::AudioOutput *output, ::AudioBufferOutput *bufferOutput,
                             bool pitchCompensation)
    : Renderer(id, tc),
      m_output(output),
      m_bufferOutput(bufferOutput),
      m_pitchCompensation(pitchCompensation)
{
    // =========================================================================
    // 音频渲染器构造函数
    // 初始化音频输出设备，连接信号槽
    // =========================================================================
    if (output) {

        // 连接音频输出设备的信号
        connect(output, &::AudioOutput::deviceChanged, this, &AudioRenderer::onDeviceChanged);
        connect(output, &::AudioOutput::volumeChanged, this, &AudioRenderer::updateVolume);
        connect(output, &::AudioOutput::mutedChanged, this, &AudioRenderer::updateVolume);
    }
}

AudioRenderer::~AudioRenderer()
{
    // =========================================================================
    // 析构函数
    // 释放音频输出资源
    // =========================================================================
    freeOutput();
}

void AudioRenderer::setOutput(::AudioOutput *output)
{
    // 设置音频输出设备
    setOutputInternal(m_output, output, [this](::AudioOutput *) { onDeviceChanged(); });
}

void AudioRenderer::setOutput(::AudioBufferOutput *bufferOutput)
{
    // 设置音频缓冲输出
    setOutputInternal(m_bufferOutput, bufferOutput,
                      [this](::AudioBufferOutput *) { m_bufferOutputChanged = true; });
}

void AudioRenderer::setPitchCompensation(bool enabled)
{
    // =========================================================================
    // 设置音高补偿
    // 当播放速度改变时，是否保持原始音高
    // =========================================================================
    invokePriorityMethod([this, enabled] {
        if (m_pitchCompensation == enabled)
            return;

        m_pitchCompensation = enabled;
        // 重置音频帧转换器，下次使用时重新创建
        m_audioFrameConverter.reset();
    });
}

void AudioRenderer::updateVolume()
{
    // =========================================================================
    // 更新音量
    // 根据输出设备的音量和静音状态设置音频接收器的音量
    // =========================================================================
    if (m_sink)
        m_sink->setVolume(m_output->isMuted() ? 0.f : m_output->volume());
}

void AudioRenderer::onDeviceChanged()
{
    // 设备改变标志，触发重新初始化
    m_deviceChanged = true;
}

Renderer::RenderingResult AudioRenderer::renderInternal(Frame frame)
{
    if (frame.isValid()) {
        updateOutputs(frame);

        const auto audioPtsUs = frame.absolutePts().get();
        const auto videoPtsUs = s_lastVideoPtsUs.loadAcquire();
        const auto diffUs = audioPtsUs - videoPtsUs;

        qz::Log::cat_debug(qLcAudioRenderer, "[A/V Sync] audio_pts:{}ms video_pts:{}ms diff:{}ms", audioPtsUs / 1000.0, videoPtsUs / 1000.0, diffUs / 1000.0);
    }

    const RenderingResult result = pushFrameToOutput(frame);

    if (m_lastFramePushDone)
        pushFrameToBufferOutput(frame);

    m_lastFramePushDone = result.done;

    return result;
}

AudioRenderer::RenderingResult AudioRenderer::pushFrameToOutput(const Frame &frame)
{
    // =========================================================================
    // 将音频帧推送到输出设备
    // 
    // 流程:
    //   1. 检查输出设备是否就绪
    //   2. 获取同步时间戳
    //   3. 如果没有缓冲数据，转换帧并缓冲
    //   4. 将缓冲数据写入输出设备
    //   5. 更新同步状态
    // =========================================================================
    // 检查输出设备是否就绪
    if (!m_ioDevice || !m_audioFrameConverter)
        return {};

    Q_ASSERT(m_sink);

    // 确保首次帧标志在函数结束时被清除
    auto firstFrameFlagGuard = qScopeGuard([&]() { m_firstFrameToSink = false; });

    // =========================================================================
    // 步骤1: 获取同步时间戳
    // =========================================================================
    // 记录当前音频接收器状态、空闲字节数、缓冲偏移和时间点
    const SynchronizationStamp syncStamp{ m_sink->state(), m_sink->bytesFree(),
                                          m_bufferedData.offset, SteadyClock::now() };

    // =========================================================================
    // 步骤2: 处理无效帧(刷新/排空)
    // =========================================================================
    if (!m_bufferedData.isValid()) {
        if (!frame.isValid()) {

            // 如果已经排空过，返回空结果
            if (std::exchange(m_drained, true))
                return {};

            // 计算缓冲加载时间
            const auto time = bufferLoadingTime(syncStamp);

            qz::Log::cat_debug(qLcAudioRenderer, "Draining AudioRenderer, time:{}", time);

            // 返回排空状态
            return { time.count() == 0, time };
        }

        // =========================================================================
        // 步骤3: 转换音频帧并缓冲
        // =========================================================================
        // 使用音频帧转换器将 AVFrame 转换为音频缓冲
        m_bufferedData = {
            m_audioFrameConverter->convert(frame.avFrame()),
        };
    }

    // =========================================================================
    // 步骤4: 将缓冲数据写入输出设备
    // =========================================================================
    if (m_bufferedData.isValid()) {

        // 确保同步状态在函数结束时被更新
        auto syncGuard = qScopeGuard([&]() { updateSynchronization(syncStamp, frame); });

        // 写入数据到输出设备
        const auto bytesWritten = m_ioDevice->write(m_bufferedData.data(), m_bufferedData.size());

        // 更新缓冲偏移
        m_bufferedData.offset += bytesWritten;

        // 如果缓冲数据已全部写入，清空缓冲
        if (m_bufferedData.size() <= 0) {
            m_bufferedData = {};
            return {};
        }

        // 计算剩余数据的时长
        const auto remainingDuration = durationForBytes(m_bufferedData.size());

        // 返回渲染结果，包含剩余时长
        return { false,
                 std::min(remainingDuration + DurationBias, m_timings.actualBufferDuration / 2) };
    }

    return {};
}

void AudioRenderer::pushFrameToBufferOutput(const Frame &frame)
{
    // =========================================================================
    // 将音频帧推送到缓冲输出
    // 用于外部获取音频数据(如音频分析、可视化等)
    // =========================================================================
    if (!m_bufferOutput)
        return;

    if (frame.isValid()) {
        Q_ASSERT(m_bufferOutputResampler);

        // 重采样并发射音频缓冲信号
        ::AudioBuffer buffer = m_bufferOutputResampler->resample(frame.avFrame());
        emit m_bufferOutput->audioBufferReceived(buffer);
    } else {

        // 发射空缓冲信号表示结束
        emit m_bufferOutput->audioBufferReceived({});
    }
}

void AudioRenderer::onPlaybackRateChanged()
{
    // 播放速率改变时，重置音频帧转换器
    m_audioFrameConverter.reset();
}

AudioRenderer::TimePoint AudioRenderer::nextTimePoint() const
{
    // =========================================================================
    // 计算下一次执行的时间点
    // 处理首次帧和空闲状态的特殊情况
    // =========================================================================
    const TimePoint timePoint = Renderer::nextTimePoint();

    // 如果是首次帧，使用默认时间点
    if (m_firstFrameToSink)
        return timePoint;

    // 如果音频接收器不在空闲状态，使用默认时间点
    if (!m_sink || m_sink->state() != ::Audio::IdleState)
        return timePoint;

    // 如果时间间隔过大，使用默认时间点
    constexpr auto MaxFixableInterval = 50ms;
    if (timePoint == TimePoint::min() ||
        timePoint - std::chrono::steady_clock::now() > MaxFixableInterval)
        return timePoint;

    // 返回最小时间点，立即执行
    return TimePoint::min();
}

void AudioRenderer::onPauseChanged()
{
    // 暂停状态改变时，重置首次帧标志
    m_firstFrameToSink = true;
    Renderer::onPauseChanged();
}

void AudioRenderer::initAudioFrameConverter(const Frame &frame)
{
    // =========================================================================
    // 初始化音频帧转换器
    // 根据是否需要音高补偿选择不同的转换器
    // =========================================================================
    if (!m_pitchCompensation || qFuzzyCompare(playbackRate(), 1.0f)) {

        // 不需要音高补偿，使用简单转换器
        m_audioFrameConverter = makeTrivialAudioFrameConverter(frame, m_sinkFormat, playbackRate());
    } else {

        // 需要音高补偿，使用变调转换器
        m_audioFrameConverter =
                makePitchShiftingAudioFrameConverter(frame, m_sinkFormat, playbackRate());
    }
}

void AudioRenderer::freeOutput()
{
    // =========================================================================
    // 释放音频输出资源
    // 清理音频接收器和相关状态
    // =========================================================================
    qz::Log::cat_debug(qLcAudioRenderer, "Free audio output");
    if (m_sink) {
        m_sink->reset();
        m_sink.reset();
    }

    m_ioDevice = nullptr;
    m_bufferedData = {};
    m_deviceChanged = false;
    m_sinkFormat = {};
    m_timings = {};
    m_bufferLoadingInfo = {};
}

void AudioRenderer::updateOutputs(const Frame &frame)
{
    // =========================================================================
    // 更新输出设备状态
    // 处理设备变化，初始化音频接收器和转换器
    // =========================================================================

    // 处理设备变化
    if (m_deviceChanged) {
        freeOutput();
        m_audioFrameConverter.reset();
    }

    // 处理缓冲输出
    if (m_bufferOutput) {
        if (m_bufferOutputChanged) {
            m_bufferOutputChanged = false;
            m_bufferOutputResampler.reset();
        }

        // 创建缓冲输出重采样器
        if (!m_bufferOutputResampler) {
            ::AudioFormat outputFormat = m_bufferOutput->format();
            if (!outputFormat.isValid())
                outputFormat = audioFormatFromFrame(frame);
            m_bufferOutputResampler = createResampler(frame, outputFormat);
        }
    }

    // 如果没有音频输出设备，直接返回
    if (!m_output)
        return;

    // 初始化音频格式
    if (!m_sinkFormat.isValid()) {
        m_sinkFormat = audioFormatFromFrame(frame);

        auto deviceChannelConfig = m_output->device().channelConfiguration();
        if (deviceChannelConfig != ::AudioFormat::ChannelConfigUnknown) {
            m_sinkFormat.setChannelConfig(deviceChannelConfig);
        } else {
            int maxCh = m_output->device().maximumChannelCount();
            m_sinkFormat.setChannelConfig(
                    ::AudioFormat::defaultChannelConfigForChannelCount(
                            qMin(m_sinkFormat.channelCount(), maxCh)));
        }
    }

    // 创建音频接收器
    if (!m_sink) {
        m_sink = std::make_unique<::AudioSink>(m_output->device(), m_sinkFormat);
        updateVolume();
        // 设置缓冲区大小
        m_sink->setBufferSize(m_sinkFormat.bytesForDuration(DesiredBufferTime.count()));
        // 启动音频接收器
        m_ioDevice = m_sink->start();
        m_firstFrameToSink = true;

        // 连接状态变化信号
        connect(m_sink.get(), &::AudioSink::stateChanged, this,
                &AudioRenderer::onAudioSinkStateChanged);

        // 计算时间参数
        m_timings.actualBufferDuration = durationForBytes(m_sink->bufferSize());
        m_timings.maxSoundDelay = qMin(MaxDesiredBufferTime,
                                       m_timings.actualBufferDuration - MinDesiredFreeBufferTime);
        m_timings.minSoundDelay = MinDesiredBufferTime;

        Q_ASSERT(DurationBias < m_timings.minSoundDelay
                 && m_timings.maxSoundDelay < m_timings.actualBufferDuration);
    }

    // 初始化音频帧转换器
    if (!m_audioFrameConverter)
        initAudioFrameConverter(frame);
}

// 音视频同步核心函数。
// 音频驱动整个播放时序，这是音视频同步的关键所在。
//
// 同步算法如下:
// 1. 计算 soundDelay = frameDelay + bufferLoadingTime - writtenTime
//    - frameDelay: 当前帧相对于时间线的提前/延迟量
//    - bufferLoadingTime: 当前缓冲的音频时长
//    - writtenTime: 刚写入的音频时长
//
// 2. 将缓冲状态分类为 High/Moderate/Low:
//    - High: 缓冲过多，播放速度过慢
//    - Low: 缓冲不足，播放速度过快
//    - Moderate: 最佳范围，无需调整
//
// 3. 如果缓冲超出最佳范围超过 BufferLoadingMeasureTime:
//    - 调用 changeRendererTime() 调整时间线
//    - 这会影响音频和视频渲染器
//    - 视频会自动跟随调整后的时间线
void AudioRenderer::updateSynchronization(const SynchronizationStamp &stamp, const Frame &frame)
{
    if (!frame.isValid())
        return;

    Q_ASSERT(m_sink);

    // 计算各项时间参数
    const auto bufferLoadingTime = this->bufferLoadingTime(stamp);
    const auto currentFrameDelay = frameDelay(frame, stamp.timePoint);
    const auto writtenTime = durationForBytes(stamp.bufferBytesWritten);
    // 音频延迟 = 帧延迟 + 缓冲加载时间 - 已写入时间
    const auto soundDelay = currentFrameDelay + bufferLoadingTime - writtenTime;

    // 同步函数: 调整渲染时间
    auto synchronize = [&](microseconds fixedDelay, microseconds targetSoundDelay) {
        // 调整时间线
        changeRendererTime(fixedDelay - targetSoundDelay);
        if (qLcAudioRenderer.is_enabled()) {
            qz::Log::cat_debug(qLcAudioRenderer, "Change rendering time:\n  First frame:{}\n  Delay (frame+buffer-written):{}+{}-{}={}\n  Fixed delay:{}\n  Target delay:{}\n  Buffer durations (min/max/limit):{} {} {}\n  Audio sink state:{}", m_firstFrameToSink, currentFrameDelay, bufferLoadingTime, writtenTime, soundDelay, fixedDelay, targetSoundDelay, m_timings.minSoundDelay, m_timings.maxSoundDelay, m_timings.actualBufferDuration, static_cast<int>(stamp.audioSinkState));
        }
    };

    // 根据音频延迟确定缓冲加载类型
    const auto loadingType = soundDelay > m_timings.maxSoundDelay ? BufferLoadingInfo::High
                           : soundDelay < m_timings.minSoundDelay ? BufferLoadingInfo::Low
                                                                  : BufferLoadingInfo::Moderate;

    if (loadingType != m_bufferLoadingInfo.type) {
        m_bufferLoadingInfo = { loadingType, stamp.timePoint, soundDelay };
    }

    // 如果不在最佳范围，进行同步调整
    if (loadingType != BufferLoadingInfo::Moderate) {
        const auto isHigh = loadingType == BufferLoadingInfo::High;
        const auto shouldHandleIdle = stamp.audioSinkState == ::Audio::IdleState && !isHigh;

        auto &fixedDelay = m_bufferLoadingInfo.delay;

        // 更新固定延迟
        fixedDelay = shouldHandleIdle ? soundDelay
                   : isHigh           ? qMin(soundDelay, fixedDelay)
                                      : qMax(soundDelay, fixedDelay);

        // 检查是否需要同步
        if (stamp.timePoint - m_bufferLoadingInfo.timePoint > BufferLoadingMeasureTime
            || (m_firstFrameToSink && isHigh) || shouldHandleIdle) {
            // 计算目标延迟
            const auto targetDelay = isHigh
                    ? (m_timings.maxSoundDelay + m_timings.minSoundDelay) / 2
                    : m_timings.minSoundDelay + DurationBias;

            // 执行同步
            synchronize(fixedDelay, targetDelay);
            // 重置缓冲加载信息
            m_bufferLoadingInfo = { BufferLoadingInfo::Moderate, stamp.timePoint, targetDelay };
        }
    }
}

// 计算当前缓冲中的音频数据时长。
// 这是同步的关键 - 告诉我们有多少音频正在排队等待播放，
// 这会影响时序计算。
// 如果音频接收器空闲(未播放)，返回 0。
microseconds AudioRenderer::bufferLoadingTime(const SynchronizationStamp &syncStamp) const
{
    // =========================================================================
    // 计算当前缓冲中的音频时长
    // 这是同步的关键 - 告诉我们有多少音频正在排队等待播放
    // 如果音频接收器空闲(未播放)，返回 0
    // =========================================================================
    Q_ASSERT(m_sink);

    // 如果音频接收器空闲，返回 0
    if (syncStamp.audioSinkState == ::Audio::IdleState)
        return microseconds(0);

    // 计算缓冲中的字节数
    const auto bytes = qMax(m_sink->bufferSize() - syncStamp.audioSinkBytesFree, 0);

    // 将字节数转换为时长
    return durationForBytes(bytes);
}

void AudioRenderer::onAudioSinkStateChanged(::Audio::State state)
{
    // 音频接收器状态变化回调
    // 当状态变为空闲且不是首次帧且设备未改变时，调度下一步
    if (state == ::Audio::IdleState && !m_firstFrameToSink && !m_deviceChanged) {
        scheduleNextStep();
    }
}

microseconds AudioRenderer::durationForBytes(qsizetype bytes) const
{
    // 将字节数转换为时长(微秒)
    return microseconds(m_sinkFormat.durationForBytes(static_cast<qint32>(bytes)));
}

}

QT_END_NAMESPACE

#include "moc_FFmpegAudioRenderer_p.cpp"
