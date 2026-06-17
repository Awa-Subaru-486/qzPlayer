#ifndef SOURCERESOLVER_H
#define SOURCERESOLVER_H

#include "mfstream_p.h"
#include <QUrl>

namespace windows {

// 源解析器，异步解析 URL 或流为 IMFMediaSource
class SourceResolver: public QObject, public IMFAsyncCallback
{
    Q_OBJECT
public:
    SourceResolver();

    ~SourceResolver();

    // IUnknown 接口
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef(void) override;
    STDMETHODIMP_(ULONG) Release(void) override;

    // IMFAsyncCallback 接口
    HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult *pAsyncResult) override;

    HRESULT STDMETHODCALLTYPE GetParameters(DWORD*, DWORD*) override;

    // 加载 URL 或流
    void load(const QUrl &url, QIODevice* stream);

    // 取消解析
    void cancel();

    // 关闭
    void shutdown();

    // 获取媒体源
    IMFMediaSource* mediaSource() const;

Q_SIGNALS:
    void error(long hr);
    void mediaSourceReady();

private:
    // 解析状态
    class State : public IUnknown
    {
    public:
        State(IMFSourceResolver *sourceResolver, bool fromStream);
        virtual ~State();

        STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject) override;

        STDMETHODIMP_(ULONG) AddRef(void) override;

        STDMETHODIMP_(ULONG) Release(void) override;

        IMFSourceResolver* sourceResolver() const;
        bool fromStream() const;

    private:
        long m_cRef;
        IMFSourceResolver *m_sourceResolver;
        bool m_fromStream;
    };

    long              m_cRef;
    IUnknown          *m_cancelCookie;
    IMFSourceResolver *m_sourceResolver;
    IMFMediaSource    *m_mediaSource;
    MFStream          *m_stream;
    QMutex            m_mutex;
};

}

#endif
