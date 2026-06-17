#ifndef MFDECODERSOURCEREADER_H
#define MFDECODERSOURCEREADER_H

#include <mfidl.h>
#include <mfreadwrite.h>

#include <QtCore/qobject.h>
#include "AudioFormat.h"
#include <QtCore/private/qcomptr_p.h>
#include "mfdecodersourcereadercallback_p.h"

namespace windows {

// MF 解码器源读取器，从媒体源读取解码后的音频样本
class MFDecoderSourceReader : public QObject
{
    Q_OBJECT
public:
    // 清除源
    void clearSource() { m_sourceReader.Reset(); }
    // 设置媒体源
    ComPtr<IMFMediaType> setSource(IMFMediaSource *source, AudioFormat::SampleFormat);

    // 读取下一个样本
    void readNextSample();

Q_SIGNALS:
    void newSample(ComPtr<IMFSample>);
    void finished();

private:
    ComPtr<IMFSourceReader> m_sourceReader;
    ComPtr<MFSourceReaderCallback> m_sourceReaderCallback;
};

}

#endif
