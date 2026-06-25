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

package io.github.xiaotong6666.fusehide.status

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Binder
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.core.content.ContextCompat
import java.lang.ref.WeakReference

class HookStatusProbe(
    private val context: Context,
    private val onTimeout: (String) -> Unit,
    private val onStarted: (WeakReference<Binder>, Runnable) -> Unit,
) {
    private val appContext = context.applicationContext ?: context
    private val mainHandler = Handler(Looper.getMainLooper())

    fun start(requestToken: String) {
        val binder = Binder()
        val binderReference = WeakReference(binder)
        val timeoutRunnable = Runnable {
            Log.d("FuseHide", "hook status check timeout after ${TIMEOUT_MS}ms")
            onTimeout(requestToken)
        }
        onStarted(binderReference, timeoutRunnable)

        val intent = Intent(ACTION_GET_STATUS).setPackage(APP_PACKAGE)
        intent.putExtra(
            "EXTRA_PENDING_INTENT",
            PendingIntent.getBroadcast(context, 1, intent, PendingIntent.FLAG_IMMUTABLE),
        )
        intent.extras?.putBinder("EXTRA_BINDER", binder)
        intent.putExtra(EXTRA_STATUS_QUERY_TOKEN, requestToken)

        MEDIA_PROVIDER_PACKAGES.forEach { packageName ->
            intent.setPackage(packageName)
            Log.d("FuseHide", "send GET_STATUS to ${intent.`package`}")
            appContext.sendBroadcast(intent)
        }

        mainHandler.postDelayed(timeoutRunnable, TIMEOUT_MS)
    }

    companion object {
        private const val APP_PACKAGE = "io.github.xiaotong6666.fusehide"
        const val ACTION_SET_STATUS = "$APP_PACKAGE.SET_STATUS"
        private const val ACTION_GET_STATUS = "$APP_PACKAGE.GET_STATUS"
        const val EXTRA_STATUS_QUERY_TOKEN = "$APP_PACKAGE.extra.STATUS_QUERY_TOKEN"
        private const val TIMEOUT_MS = 2000L

        val MEDIA_PROVIDER_PACKAGES = listOf(
            "com.google.android.providers.media.module",
            "com.android.providers.media.module",
        )

        fun registerReceiverFlags(): Int = ContextCompat.RECEIVER_EXPORTED
    }
}
