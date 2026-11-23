/*
 * Copyright (C) 2026 XiaoTong6666
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.github.xiaotong6666.fusehide.config

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.os.Bundle
import android.util.Log
import androidx.core.content.ContextCompat
import io.github.xiaotong6666.fusehide.BuildConfig
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Runtime diagnostics are independent from hide rules: enabling logcat tracing must never
 * implicitly save draft hide rules or invalidate the FUSE caches.
 *
 * MediaProvider cannot read this application's preferences under SELinux. Its injected entry
 * queries the existing trusted provider and supplies the boolean to native code via JNI.
 */
object DebugLogSettings {
    const val METHOD_GET_DEBUG_LOGGING = "get_debug_logging"
    const val KEY_ENABLED = "enabled"
    const val ACTION_REFRESH_DEBUG_LOGGING = "${BuildConfig.APPLICATION_ID}.REFRESH_DEBUG_LOGGING"
    private const val PERMISSION_REFRESH = "${BuildConfig.APPLICATION_ID}.permission.REFRESH_DEBUG_LOGGING"

    private const val PREFS_NAME = "settings"
    private const val PREF_ENABLED = "enable_detailed_debug_logging"
    private val providerUri = Uri.parse("content://${BuildConfig.APPLICATION_ID}.hideconfig")
    private val injectedReceiverRegistered = AtomicBoolean(false)

    @JvmStatic
    fun isEnabled(context: Context): Boolean = context
        .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        .getBoolean(PREF_ENABLED, BuildConfig.DEBUG)

    @JvmStatic
    fun setEnabled(context: Context, enabled: Boolean): Boolean = context
        .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        .edit()
        .putBoolean(PREF_ENABLED, enabled)
        .commit()

    @JvmStatic
    fun toProviderBundle(context: Context): Bundle = Bundle().apply { putBoolean(KEY_ENABLED, isEnabled(context)) }

    @JvmStatic
    fun refreshInjectedProcesses(context: Context) {
        listOf(HideConfigStore.PACKAGE_MEDIA_GOOGLE, HideConfigStore.PACKAGE_MEDIA).forEach { pkg ->
            context.sendBroadcast(Intent(ACTION_REFRESH_DEBUG_LOGGING).setPackage(pkg))
        }
    }

    @JvmStatic
    fun applyFromProvider(context: Context): Boolean {
        val enabled = try {
            val bundle = context.contentResolver.call(providerUri, METHOD_GET_DEBUG_LOGGING, null, null)
            if (bundle == null || !bundle.containsKey(KEY_ENABLED)) {
                Log.w("FuseHide", "debug log setting unavailable from provider")
                return false
            }
            bundle.getBoolean(KEY_ENABLED)
        } catch (t: Throwable) {
            Log.w("FuseHide", "failed to read debug log setting", t)
            return false
        }
        return try {
            HideConfigNativeBridge.setDebugLoggingEnabled(enabled)
            true
        } catch (t: Throwable) {
            Log.e("FuseHide", "failed to update native debug logging", t)
            false
        }
    }

    /**
     * Called once from either Xposed or Zygisk after the native module is loaded.
     * Reading the app provider on a worker avoids binder I/O on MediaProvider's
     * main thread. A received intent is only a refresh hint, never an authority
     * to set the logging flag. The trusted provider owns its actual value.
     */
    @JvmStatic
    fun registerInjectedProcess(context: Context) {
        if (!injectedReceiverRegistered.compareAndSet(false, true)) return
        val app = context.applicationContext ?: context
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent?) {
                if (intent?.action != ACTION_REFRESH_DEBUG_LOGGING && intent?.action != Intent.ACTION_USER_UNLOCKED) {
                    return
                }
                val result = goAsync()
                Thread({
                    try {
                        applyFromProvider(app)
                    } finally {
                        result.finish()
                    }
                }, "FuseHide-debug-refresh").start()
            }
        }
        try {
            // An exported receiver without a sender permission lets arbitrary apps
            // create unbounded binder queries/threads in MediaProvider. Check a
            // signature permission on the app-originated refresh broadcast.
            ContextCompat.registerReceiver(
                app,
                receiver,
                IntentFilter(ACTION_REFRESH_DEBUG_LOGGING),
                PERMISSION_REFRESH,
                null,
                ContextCompat.RECEIVER_EXPORTED,
            )
            // Keep the system lifecycle broadcast separate: it does not carry
            // FuseHide's signature permission and is not exported to apps.
            ContextCompat.registerReceiver(
                app,
                receiver,
                IntentFilter(Intent.ACTION_USER_UNLOCKED),
                ContextCompat.RECEIVER_NOT_EXPORTED,
            )
        } catch (t: Throwable) {
            injectedReceiverRegistered.set(false)
            Log.e("FuseHide", "failed to register native debug logging receiver", t)
            return
        }
        Thread({ applyFromProvider(app) }, "FuseHide-debug-initial").start()
    }
}
