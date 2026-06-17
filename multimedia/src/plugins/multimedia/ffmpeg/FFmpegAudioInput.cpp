#include "FFmpegAudioInput_p.h"

#include <QtCore/qatomic.h>
#include <QtCore/qdebug.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qmetaobject.h>
#include <qzMultimedia/AudioBuffer.h>
#include <qzMultimedia/AudioSource.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

class AudioSourceIO : public QIODevice
{
    Q_OBJECT
public:
    AudioSourceIO(AudioInput *audioInput) : m_input(audioInput)
    {
        m_muted = m_input->muted;
        m_volume = m_input->volume;
        updateVolume();
        open(QIODevice::WriteOnly);
    }

    ~AudioSourceIO() override
    {
        if (m_audioSource)
            m_audioSource->reset();
    }

    void setDevice(const ::AudioDevice &device)
    {
        Q_ASSERT(!thread()->isCurrentThread());
        QMutexLocker locker(&m_mutex);
        if (m_device == device)
            return;
        m_device = device;
        QMetaObject::invokeMethod(this, [this] {
            QMutexLocker locker(&m_mutex);
            updateSource(locker);
        });
    }
    void setBufferSize(int bufferSize)
    {
        m_bufferSize.storeRelease((bufferSize > 0 && m_format.isValid())
                                          ? m_format.bytesForFrames(bufferSize)
                                          : DefaultAudioInputBufferSize);
    }
    void setRunning(bool r) {
        Q_ASSERT(!thread()->isCurrentThread());
        QMutexLocker locker(&m_mutex);
        if (m_running == r)
            return;
        m_running = r;
        QMetaObject::invokeMethod(this, &AudioSourceIO::updateRunning);
    }

    void setVolume(float vol) {
        Q_ASSERT(!thread()->isCurrentThread());
        QMutexLocker locker(&m_mutex);
        m_volume = vol;
        QMetaObject::invokeMethod(this, &AudioSourceIO::updateVolume);
    }
    void setMuted(bool muted) {
        Q_ASSERT(!thread()->isCurrentThread());
        QMutexLocker locker(&m_mutex);
        m_muted = muted;
        QMetaObject::invokeMethod(this, &AudioSourceIO::updateVolume);
    }

    int bufferSize() const { return m_bufferSize.loadAcquire(); }

protected:
    qint64 readData(char *, qint64) override
    {
        return 0;
    }
    qint64 writeData(const char *data, qint64 len) override
    {
        Q_ASSERT(m_audioSource);

        int l = len;
        while (len > 0) {
            const auto bufferSize = m_bufferSize.loadAcquire();

            while (m_pcm.size() > bufferSize) {
                sendBuffer(m_pcm.first(bufferSize));
                m_pcm.remove(0, bufferSize);
            }

            int toAppend = qMin(len, bufferSize - m_pcm.size());
            m_pcm.append(data, toAppend);
            data += toAppend;
            len -= toAppend;
            if (m_pcm.size() == bufferSize) {
                sendBuffer(m_pcm);
                m_pcm.clear();
            }
        }

        return l;
    }

private Q_SLOTS:
    void updateVolume()
    {
        if (m_audioSource)
            m_audioSource->setVolume(m_muted ? 0. : m_volume);
    }
    void updateRunning()
    {
        QMutexLocker locker(&m_mutex);
        if (m_running) {
            if (!m_audioSource)
                updateSource(locker);
            else
                m_audioSource->start(this);
        } else {
            m_audioSource->stop();
        }
    }

private:
    void updateSource(const QMutexLocker<QMutex> &guard)
    {
        Q_ASSERT(guard.mutex() == &m_mutex);
        m_format = m_device.preferredFormat();
        if (std::exchange(m_audioSource, nullptr))
            m_pcm.clear();

        m_audioSource = std::make_unique<::AudioSource>(m_device, m_format);
        updateVolume();
        if (m_running)
            m_audioSource->start(this);
    }

    void sendBuffer(const QByteArray &pcmData)
    {
        Q_UNUSED(pcmData);
    }

    QMutex m_mutex;
    ::AudioDevice m_device;
    float m_volume = 1.;
    bool m_muted = false;
    bool m_running = false;

    AudioInput *m_input = nullptr;
    std::unique_ptr<::AudioSource> m_audioSource;
    ::AudioFormat m_format;
    QAtomicInt m_bufferSize = DefaultAudioInputBufferSize;
    qint64 m_processed = 0;
    QByteArray m_pcm;
};

}

namespace ffmpeg {

AudioInput::AudioInput(::AudioInput *qq)
    : PlatformAudioInput(qq)
{
    qRegisterMetaType<::AudioBuffer>();

    m_inputThread = std::make_unique<QThread>();
    m_inputThread->setObjectName(QStringLiteral("FFmpegAudioInputThread"));
    m_audioIO = new AudioSourceIO(this);
    m_audioIO->moveToThread(m_inputThread.get());
    m_inputThread->start();
}

AudioInput::~AudioInput()
{
    m_audioIO->deleteLater();
    m_inputThread->exit();
    m_inputThread->wait();
}

void AudioInput::setAudioDevice(const ::AudioDevice &device)
{
    m_audioIO->setDevice(device);
}

void AudioInput::setMuted(bool muted)
{
    m_audioIO->setMuted(muted);
}

void AudioInput::setVolume(float volume)
{
    m_audioIO->setVolume(volume);
}

void AudioInput::setBufferSize(int bufferSize)
{
    m_audioIO->setBufferSize(bufferSize);
}

int AudioInput::bufferSize() const
{
    return m_audioIO->bufferSize();
}

}

QT_END_NAMESPACE

#include "moc_FFmpegAudioInput_p.cpp"

#include "FFmpegAudioInput.moc"
