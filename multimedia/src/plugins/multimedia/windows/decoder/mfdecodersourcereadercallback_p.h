#ifndef MFDECODERSOURCEREADERCALLBACK_H
#define MFDECODERSOURCEREADERCALLBACK_H

#include <mfidl.h>
#include <mfreadwrite.h>

#include <QtCore/qobject.h>
#include <QtCore/private/qcomptr_p.h>
#include <QtCore/private/qcomobject_p.h>

namespace windows {

// MF 源读取器回调，处理异步读取样本的回调
class MFSourceReaderCallback : public QObject, public QComObject<IMFSourceReaderCallback>
{
    Q_OBJECT
public:
    // 样本读取完成回调
    STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
                              LONGLONG llTimestamp, IMFSample *pSample) override;
    STDMETHODIMP OnFlush(DWORD) override { return S_OK; }
    STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *) override { return S_OK; }

Q_SIGNALS:
    void newSample(ComPtr<IMFSample>);
    void finished();
};

}

#endif
