// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

package qz.multimedia.android;

import android.graphics.SurfaceTexture;

class QtSurfaceTextureListener implements SurfaceTexture.OnFrameAvailableListener
{
    private final long m_id;

    QtSurfaceTextureListener(long id)
    {
        m_id = id;
    }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture)
    {
        notifyFrameAvailable(m_id);
    }

    private static native void notifyFrameAvailable(long id);
}
