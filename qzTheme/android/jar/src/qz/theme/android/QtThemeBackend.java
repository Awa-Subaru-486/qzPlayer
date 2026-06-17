// Copyright (C) 2025 awa

package qz.theme.android;

import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;

import org.qtproject.qt.android.UsedFromNativeCode;

class QtThemeBackend
{
    private static final String TAG = "QtThemeBackend";
    private static Context m_context = null;
    private static long m_nativePtr = 0;

    @UsedFromNativeCode
    static void setContext(Context context)
    {
        m_context = context;
    }

    /**
     * Called from C++ when the backend is constructed.
     * Registers ComponentCallbacks to listen for theme changes.
     */
    @UsedFromNativeCode
    static void registerBackend(long nativePtr)
    {
        m_nativePtr = nativePtr;
        if (m_context != null) {
            m_context.registerComponentCallbacks(m_configCallbacks);
        }
    }

    /**
     * Called from C++ when the backend is destroyed.
     * Unregisters ComponentCallbacks and clears the native pointer.
     */
    @UsedFromNativeCode
    static void unregisterBackend()
    {
        if (m_context != null) {
            m_context.unregisterComponentCallbacks(m_configCallbacks);
        }
        m_nativePtr = 0;
    }

    /**
     * Get the current system theme mode.
     * @return 0 = Light, 1 = Dark
     */
    @UsedFromNativeCode
    static int getSystemThemeMode()
    {
        if (m_context == null)
            return 0;

        int nightMode = m_context.getResources().getConfiguration().uiMode
                        & Configuration.UI_MODE_NIGHT_MASK;
        return (nightMode == Configuration.UI_MODE_NIGHT_YES) ? 1 : 0;
    }

    /**
     * Get the system accent color as ARGB int.
     * @return accent color as 0xAARRGGBB
     */
    @UsedFromNativeCode
    static int getSystemAccentColor()
    {
        if (m_context == null)
            return 0xFF3689E6;

        try {
            TypedValue typedValue = new TypedValue();
            ContextThemeWrapper wrapper = new ContextThemeWrapper(m_context,
                    android.R.style.Theme_DeviceDefault);
            wrapper.getTheme().resolveAttribute(android.R.attr.colorAccent, typedValue, true);

            if (typedValue.type >= TypedValue.TYPE_FIRST_COLOR_INT
                    && typedValue.type <= TypedValue.TYPE_LAST_COLOR_INT) {
                return typedValue.data;
            }
        } catch (Exception e) {
            // fallback
        }

        return 0xFF3689E6;
    }

    // ComponentCallbacks to detect theme (configuration) changes
    private static final ComponentCallbacks2 m_configCallbacks = new ComponentCallbacks2() {
        @Override
        public void onConfigurationChanged(Configuration newConfig)
        {
            if (m_nativePtr != 0) {
                int nightMode = newConfig.uiMode & Configuration.UI_MODE_NIGHT_MASK;
                int themeMode = (nightMode == Configuration.UI_MODE_NIGHT_YES) ? 1 : 0;
                onThemeChanged(m_nativePtr, themeMode);
            }
        }

        @Override
        public void onLowMemory() {}

        @Override
        public void onTrimMemory(int level) {}
    };

    // Native callback: Java -> C++
    private static native void onThemeChanged(long nativePtr, int themeMode);
}
