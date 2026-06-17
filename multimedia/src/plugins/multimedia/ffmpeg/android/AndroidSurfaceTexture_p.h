#ifndef ANDROIDSURFACETEXTURE_P_H
#define ANDROIDSURFACETEXTURE_P_H

#include <qobject.h>
#include <QtCore/qjniobject.h>

#include <QMatrix4x4>

QT_BEGIN_NAMESPACE

// Android SurfaceTexture 封装，通过 JNI 管理纹理更新
class AndroidSurfaceTexture : public QObject
{
    Q_OBJECT
public:
    explicit AndroidSurfaceTexture(quint32 texName);
    ~AndroidSurfaceTexture();

    jobject surfaceTexture();
    jobject surface();
    jobject surfaceHolder();
    inline bool isValid() const { return m_surfaceTexture.isValid(); }

    QMatrix4x4 getTransformMatrix();
    void release();
    void updateTexImage();

    void attachToGLContext(quint32 texName);
    void detachFromGLContext();

    static bool registerNativeMethods();

    quint64 index() const { return m_index; }
Q_SIGNALS:
    void frameAvailable();

private:
    void setOnFrameAvailableListener(const QJniObject &listener);

    QJniObject m_surfaceTexture;
    QJniObject m_surface;
    QJniObject m_surfaceHolder;
    const quint64 m_index = 0;
};

QT_END_NAMESPACE

#endif // ANDROIDSURFACETEXTURE_P_H
