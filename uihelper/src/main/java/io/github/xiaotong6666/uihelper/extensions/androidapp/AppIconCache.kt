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

package io.github.xiaotong6666.uihelper.extensions.androidapp

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.util.LruCache
import androidx.core.graphics.drawable.toBitmap

object AppIconCache {
    private val memoryCache = object : LruCache<String, Bitmap>(20 * 1024 * 1024) {
        override fun sizeOf(key: String, value: Bitmap): Int = value.byteCount
    }

    fun getCached(appInfo: ApplicationInfo, sizePx: Int): Bitmap? {
        val key = "${appInfo.packageName}_$sizePx"
        return memoryCache.get(key)
    }

    fun loadIcon(context: Context, appInfo: ApplicationInfo, sizePx: Int): Bitmap? {
        val key = "${appInfo.packageName}_$sizePx"
        var bitmap = memoryCache.get(key)
        if (bitmap == null) {
            val drawable = appInfo.loadIcon(context.packageManager)
            if (drawable != null) {
                bitmap = drawable.toBitmap(width = sizePx, height = sizePx)
                memoryCache.put(key, bitmap)
            }
        }
        return bitmap
    }
}
