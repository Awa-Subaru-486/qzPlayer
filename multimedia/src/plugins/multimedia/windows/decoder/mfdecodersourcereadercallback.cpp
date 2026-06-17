#include "mfdecodersourcereadercallback_p.h"

namespace windows {

STDMETHODIMP MFSourceReaderCallback::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
                                                  DWORD dwStreamFlags, LONGLONG llTimestamp,
                                                  IMFSample *pSample)
{
    Q_UNUSED(hrStatus);
    Q_UNUSED(dwStreamIndex);
    Q_UNUSED(llTimestamp);
    if (pSample) {
        emit newSample(ComPtr<IMFSample>{ pSample });
    } else if ((dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM) == MF_SOURCE_READERF_ENDOFSTREAM) {
        emit finished();
    }
    return S_OK;
}

}

#include "moc_mfdecodersourcereadercallback_p.cpp"
