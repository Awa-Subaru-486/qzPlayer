package qz.player.android;

import android.app.Activity;
import android.app.Application;
import android.app.PictureInPictureParams;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.media.AudioManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Rational;
import android.view.WindowManager;

import org.qtproject.qt.android.UsedFromNativeCode;

class QtAndroidUtils
{
    private static final String TAG = "QtAndroidUtils";
    static Activity m_activity = null; // Package-private for BatteryStatusReceiver access
    private static BatteryStatusReceiver m_batteryReceiver = null;

    @UsedFromNativeCode
    static void setContext(Context context)
    {
        if (context instanceof Activity) {
            m_activity = (Activity) context;
            registerBatteryReceiver();
        } else if (context instanceof Application) {
            ((Application) context).registerActivityLifecycleCallbacks(
                new Application.ActivityLifecycleCallbacks() {
                    @Override
                    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
                        m_activity = activity;
                        registerBatteryReceiver();
                    }
                    @Override
                    public void onActivityStarted(Activity activity) {
                        m_activity = activity;
                        registerBatteryReceiver();
                    }
                    @Override
                    public void onActivityResumed(Activity activity) {
                        m_activity = activity;
                        registerBatteryReceiver();
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
                            unregisterBatteryReceiver();
                            m_activity = null;
                        }
                    }
                }
            );
        }
    }

    private static void registerBatteryReceiver()
    {
        if (m_activity == null || m_batteryReceiver != null)
            return;

        m_batteryReceiver = new BatteryStatusReceiver();
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        m_activity.registerReceiver(m_batteryReceiver, filter);
    }

    private static void unregisterBatteryReceiver()
    {
        if (m_activity == null || m_batteryReceiver == null)
            return;

        try {
            m_activity.unregisterReceiver(m_batteryReceiver);
        } catch (Exception e) {
            // Ignore if already unregistered
        }
        m_batteryReceiver = null;
    }

    /**
     * Set the requested screen orientation.
     * @param orientation Android ActivityInfo screen orientation constant
     */
    @UsedFromNativeCode
    static void setRequestedOrientation(int orientation)
    {
        if (m_activity != null) {
            m_activity.setRequestedOrientation(orientation);
        }
    }

    /**
     * Get the current requested screen orientation.
     * @return Android ActivityInfo screen orientation constant
     */
    @UsedFromNativeCode
    static int getRequestedOrientation()
    {
        if (m_activity != null) {
            return m_activity.getRequestedOrientation();
        }
        return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
    }

    /**
     * Toggle between portrait and landscape orientation.
     */
    @UsedFromNativeCode
    static void toggleOrientation()
    {
        if (m_activity == null)
            return;

        int current = m_activity.getRequestedOrientation();
        if (current == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                || current == ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT) {
            m_activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        } else {
            m_activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        }
    }

    // ---- System Brightness ----

    /**
     * Get the system screen brightness (0-255).
     * @return brightness value, or -1 on error
     */
    @UsedFromNativeCode
    static int getSystemBrightness()
    {
        if (m_activity == null)
            return -1;
        try {
            return Settings.System.getInt(m_activity.getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS);
        } catch (Settings.SettingNotFoundException e) {
            return -1;
        }
    }

    /**
     * Set the system screen brightness (0-255).
     * Requires WRITE_SETTINGS permission (special permission, needs user approval).
     * @param brightness value in range 0-255
     */
    @UsedFromNativeCode
    static void setSystemBrightness(int brightness)
    {
        if (m_activity == null)
            return;
        if (brightness < 0) brightness = 0;
        if (brightness > 255) brightness = 255;

        final int br = brightness;
        m_activity.runOnUiThread(() -> {
            // Apply brightness to the current activity window (always works, no permission needed)
            WindowManager.LayoutParams lp = m_activity.getWindow().getAttributes();
            lp.screenBrightness = br / 255.0f;
            m_activity.getWindow().setAttributes(lp);
        });

        // Also try to set system-level brightness (requires WRITE_SETTINGS permission)
        if (Settings.System.canWrite(m_activity)) {
            Settings.System.putInt(m_activity.getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS_MODE,
                    Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL);

            Settings.System.putInt(m_activity.getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS, brightness);
        }
    }

    /**
     * Check if auto-brightness is enabled.
     * @return true if auto-brightness is on
     */
    @UsedFromNativeCode
    static boolean isAutoBrightness()
    {
        if (m_activity == null)
            return false;
        try {
            int mode = Settings.System.getInt(m_activity.getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS_MODE);
            return mode == Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC;
        } catch (Settings.SettingNotFoundException e) {
            return false;
        }
    }

    /**
     * Set auto-brightness mode.
     * @param enabled true to enable auto-brightness
     */
    @UsedFromNativeCode
    static void setAutoBrightness(boolean enabled)
    {
        if (m_activity == null)
            return;
        Settings.System.putInt(m_activity.getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS_MODE,
                enabled ? Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC
                        : Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL);
    }

    /**
     * Move the app task to background (like pressing Home).
     * This is the standard Android "exit" behavior.
     */
    @UsedFromNativeCode
    static void moveTaskToBack()
    {
        if (m_activity != null) {
            m_activity.moveTaskToBack(true);
        }
    }

    /**
     * Finish the current Activity, properly closing the app.
     * Should only be called on double-back-press, not normal back navigation.
     */
    @UsedFromNativeCode
    static void finishActivity()
    {
        if (m_activity != null) {
            m_activity.finish();
        }
    }

    /**
     * Re-apply the screen brightness to the current Activity window.
     * This is needed when the app resumes from background, as the window
     * attributes may have been reset.
     * @param brightness value in range 0-255
     */
    @UsedFromNativeCode
    static void applyWindowBrightness(int brightness)
    {
        if (m_activity == null)
            return;
        if (brightness < 0) brightness = 0;
        if (brightness > 255) brightness = 255;

        final int br = brightness;
        m_activity.runOnUiThread(() -> {
            WindowManager.LayoutParams lp = m_activity.getWindow().getAttributes();
            lp.screenBrightness = br / 255.0f;
            m_activity.getWindow().setAttributes(lp);
        });
    }

    // ---- System Volume ----

    /**
     * Get the current system volume for a given stream type.
     * Stream types: STREAM_VOICE_CALL=0, STREAM_SYSTEM=1, STREAM_RING=2,
     *               STREAM_MUSIC=3, STREAM_ALARM=4, STREAM_NOTIFICATION=5
     * @param streamType Android AudioManager stream type
     * @return current volume, or -1 on error
     */
    @UsedFromNativeCode
    static int getSystemVolume(int streamType)
    {
        if (m_activity == null)
            return -1;
        AudioManager am = (AudioManager) m_activity.getSystemService(Context.AUDIO_SERVICE);
        if (am == null)
            return -1;
        return am.getStreamVolume(streamType);
    }

    /**
     * Set the system volume for a given stream type.
     * @param streamType Android AudioManager stream type
     * @param volume volume value (0 to max)
     */
    @UsedFromNativeCode
    static void setSystemVolume(int streamType, int volume)
    {
        if (m_activity == null)
            return;
        AudioManager am = (AudioManager) m_activity.getSystemService(Context.AUDIO_SERVICE);
        if (am == null)
            return;
        int max = am.getStreamMaxVolume(streamType);
        if (volume < 0) volume = 0;
        if (volume > max) volume = max;
        am.setStreamVolume(streamType, volume, 0);
    }

    /**
     * Get the maximum volume for a given stream type.
     * @param streamType Android AudioManager stream type
     * @return max volume, or -1 on error
     */
    @UsedFromNativeCode
    static int getMaxSystemVolume(int streamType)
    {
        if (m_activity == null)
            return -1;
        AudioManager am = (AudioManager) m_activity.getSystemService(Context.AUDIO_SERVICE);
        if (am == null)
            return -1;
        return am.getStreamMaxVolume(streamType);
    }

    /**
     * Adjust the system volume by one step (raise/lower/mute).
     * @param streamType Android AudioManager stream type
     * @param direction one of AudioManager.ADJUST_LOWER(-1), ADJUST_RAISE(1), ADJUST_MUTE(100), ADJUST_UNMUTE(101)
     */
    @UsedFromNativeCode
    static void adjustSystemVolume(int streamType, int direction)
    {
        if (m_activity == null)
            return;
        AudioManager am = (AudioManager) m_activity.getSystemService(Context.AUDIO_SERVICE);
        if (am == null)
            return;
        am.adjustStreamVolume(streamType, direction, 0);
    }

    // ---- Battery ----

    /**
     * Get the current battery level as a percentage (0-100).
     * @return battery percentage, or -1 on error
     */
    @UsedFromNativeCode
    static int getBatteryLevel()
    {
        if (m_activity == null)
            return -1;
        BatteryManager bm = (BatteryManager) m_activity.getSystemService(Context.BATTERY_SERVICE);
        if (bm == null)
            return -1;
        return bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY);
    }

    /**
     * Check if the device is currently charging.
     * @return true if charging
     */
    @UsedFromNativeCode
    static boolean isBatteryCharging()
    {
        if (m_activity == null)
            return false;
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatus = m_activity.registerReceiver(null, filter);
        if (batteryStatus == null)
            return false;
        int status = batteryStatus.getIntExtra(android.os.BatteryManager.EXTRA_STATUS, -1);
        return status == android.os.BatteryManager.BATTERY_STATUS_CHARGING
                || status == android.os.BatteryManager.BATTERY_STATUS_FULL;
    }

    /**
     * Called from BatteryStatusReceiver when battery status changes.
     * This calls the native method to notify C++ side.
     */
    static void onBatteryStatusChanged()
    {
        try {
            notifyBatteryStatusChanged();
        } catch (UnsatisfiedLinkError e) {
            // Native method not loaded yet, ignore
            android.util.Log.w(TAG, "Native method not loaded: " + e.getMessage());
        } catch (Exception e) {
            android.util.Log.e(TAG, "Error notifying battery status: " + e.getMessage());
        }
    }

    /**
     * Native method to notify C++ about battery status change.
     * This is implemented in AndroidUtils.cpp.
     */
    native static void notifyBatteryStatusChanged();

    // ---- Picture in Picture ----

    /**
     * Enter Picture-in-Picture mode.
     * @param aspectRatioWidth aspect ratio width (e.g. 16)
     * @param aspectRatioHeight aspect ratio height (e.g. 9)
     * @return true if entering PiP mode was successful
     */
    @UsedFromNativeCode
    static boolean enterPictureInPicture(int aspectRatioWidth, int aspectRatioHeight)
    {
        if (m_activity == null)
            return false;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O)
            return false;

        try {
            Rational ratio = new Rational(aspectRatioWidth, aspectRatioHeight);
            PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
            builder.setAspectRatio(ratio);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                builder.setAutoEnterEnabled(false);
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                builder.setSeamlessResizeEnabled(true);
            }

            return m_activity.enterPictureInPictureMode(builder.build());
        } catch (Exception e) {
            android.util.Log.e(TAG, "Failed to enter PiP: " + e.getMessage());
            return false;
        }
    }

    /**
     * Check if the activity is currently in Picture-in-Picture mode.
     * @return true if in PiP mode
     */
    @UsedFromNativeCode
    static boolean isPictureInPicture()
    {
        if (m_activity == null)
            return false;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O)
            return false;
        return m_activity.isInPictureInPictureMode();
    }

    /**
     * Called from Activity.onPictureInPictureModeChanged to notify C++ side.
     * @param isInPiP true if entering PiP, false if exiting
     */
    static void onPictureInPictureModeChanged(boolean isInPiP)
    {
        try {
            notifyPictureInPictureChanged(isInPiP);
        } catch (UnsatisfiedLinkError e) {
            android.util.Log.w(TAG, "Native method not loaded: " + e.getMessage());
        } catch (Exception e) {
            android.util.Log.e(TAG, "Error notifying PiP change: " + e.getMessage());
        }
    }

    /**
     * Native method to notify C++ about PiP mode change.
     * This is implemented in AndroidUtils.cpp.
     */
    native static void notifyPictureInPictureChanged(boolean isInPiP);
}

/**
 * BroadcastReceiver to listen for battery status changes.
 */
class BatteryStatusReceiver extends android.content.BroadcastReceiver
{
    @Override
    public void onReceive(Context context, Intent intent)
    {
        if (intent == null || context == null)
            return;

        String action = intent.getAction();
        if (Intent.ACTION_BATTERY_CHANGED.equals(action)) {
            // Notify QtAndroidUtils, which will call the native method
            // Check if activity is still valid
            if (QtAndroidUtils.m_activity != null) {
                QtAndroidUtils.onBatteryStatusChanged();
            }
        }
    }
}
