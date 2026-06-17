// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

package qz.multimedia.android;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;

import org.qtproject.qt.android.UsedFromNativeCode;

class QtContentUriResolver
{
    private static final String TAG = "QtContentUriResolver";
    private static Activity m_activity = null;

    @UsedFromNativeCode
    static void setContext(Context context)
    {
        if (context instanceof Activity) {
            m_activity = (Activity) context;
        } else if (context instanceof Application) {
            ((Application) context).registerActivityLifecycleCallbacks(
                new Application.ActivityLifecycleCallbacks() {
                    @Override
                    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
                        m_activity = activity;
                    }
                    @Override
                    public void onActivityStarted(Activity activity) {
                        m_activity = activity;
                    }
                    @Override
                    public void onActivityResumed(Activity activity) {
                        m_activity = activity;
                    }
                    @Override
                    public void onActivityPaused(Activity activity) {}
                    @Override
                    public void onActivityStopped(Activity activity) {}
                    @Override
                    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {}
                    @Override
                    public void onActivityDestroyed(Activity activity) {
                        if (m_activity == activity) {
                            m_activity = null;
                        }
                    }
                }
            );
        }
    }

    /**
     * Get the display name for a content:// URI.
     * Queries the ContentResolver for OpenableColumns.DISPLAY_NAME.
     * @param uriString the content:// URI string
     * @return the display name, or empty string if not found
     */
    @UsedFromNativeCode
    static String getContentDisplayName(String uriString)
    {
        if (m_activity == null)
            return "";
        try {
            Uri uri = Uri.parse(uriString);
            if (!"content".equals(uri.getScheme()))
                return "";
            String[] projection = { android.provider.OpenableColumns.DISPLAY_NAME };
            Cursor cursor = m_activity.getContentResolver().query(uri, projection, null, null, null);
            if (cursor != null) {
                try {
                    if (cursor.moveToFirst()) {
                        int index = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                        if (index >= 0)
                            return cursor.getString(index);
                    }
                } finally {
                    cursor.close();
                }
            }
        } catch (Exception e) {
            // fallback
        }
        return "";
    }
}
